#!/usr/bin/env python3
"""Measure the first moments of isolated koto notes (Docs/validation/attack-highs-2026-11-07.md).

    measure_attacks.py recording.wav > attacks.json
    measure_attacks.py render.wav --model > attacks.json

Notes are measure_koto_notes.py's isolated onsets (a render: every onset with
0.6 s clear after it). Each onset is refined to where the 1.5-12 kHz energy
(0.5 ms smoothing) first passes 10% of its peak within 30 ms. Per note: the
10-90% rise of the broadband envelope (1 ms smoothing); the click, the 6-14
kHz level over the first 3 ms against 15-30 ms; drop_hi and drop_lo, the 2.5-6
kHz and 0.25-1 kHz levels over 2-12 ms against 50-70 ms; pre, the 2-8 kHz
level over 12 to 2 ms before the onset against 60 to 30 ms before.
"""
import sys,json,math,numpy as np
sys.path.insert(0,str(__import__('pathlib').Path(__file__).parent))
import measure_koto_notes as m
from scipy import signal
def band(x,r,lo,hi):
    sos=signal.butter(4,[lo,hi],'bandpass',fs=r,output='sos');return signal.sosfiltfilt(sos,x)
def db(e):return 10*np.log10(e+1e-20)
def feats(x,r,t,end):
    a0=int((t-0.03)*r);a1=int((t+0.03)*r)
    if a0<int(0.1*r):return None
    h=band(x[a0-int(0.1*r):a1+int(0.5*r)],r,1500,12000)
    env=np.convolve(h**2,np.ones(int(0.0005*r))/int(0.0005*r),'same')
    off=int(0.1*r);w=env[off:off+(a1-a0)]
    pk=w.max();k=np.argmax(w>0.1*pk);tr=(a0+k)/r   # refined onset: 10% of the HF peak
    o=int(tr*r)
    f0=m.f0_estimate(x[o+int(0.03*r):o+int(0.35*r)],r)
    out=dict(t=tr,f0=f0)
    # broadband rise
    bb=np.abs(signal.hilbert(x[o-int(0.01*r):o+int(0.1*r)]));bb=np.convolve(bb,np.ones(48)/48,'same')
    p=bb.max();i10=np.argmax(bb>0.1*p);i90=np.argmax(bb>0.9*p);out['rise_ms']=(i90-i10)/r*1000
    # click: 6-14 kHz first 3 ms against 15-30 ms
    hh=band(x[o-int(0.05*r):o+int(0.1*r)],r,6000,14000)**2;z=int(0.05*r)
    out['click']=db(hh[z-int(0.001*r):z+int(0.003*r)].mean())-db(hh[z+int(0.015*r):z+int(0.03*r)].mean())
    # early decay of highs vs lows: 2.5-6k and 0.25-1k, 2-12 ms vs 50-70 ms
    for name,lo,hi in (('hi',2500,6000),('lo',250,1000)):
        b=band(x[o-int(0.05*r):o+int(0.15*r)],r,lo,hi)**2
        out['drop_'+name]=db(b[z+int(0.002*r):z+int(0.012*r)].mean())-db(b[z+int(0.05*r):z+int(0.07*r)].mean())
    # pre-onset noise 2-8k: [-12,-2] ms vs [-60,-30] ms
    pre=band(x[o-int(0.1*r):o+int(0.01*r)],r,2000,8000)**2;q=int(0.1*r)
    out['pre']=db(pre[q-int(0.012*r):q-int(0.002*r)].mean())-db(pre[q-int(0.06*r):q-int(0.03*r)].mean())
    # pitch glide: strongest of partials 1-3, cents at 20-50, 50-90, 90-150 ms against 250-400 ms
    seg=x[o:o+int(0.45*r)]
    if len(seg)<int(0.45*r) or end-tr<0.45:return out
    def peak(s,fc):
        n=1<<16;S=np.abs(np.fft.rfft(s*np.hanning(len(s)),n));f=np.fft.rfftfreq(n,1/r)
        mm=(f>fc*0.97)&(f<fc*1.03);i=np.argmax(np.where(mm,S,0));return f[i],S[i]
    best=max((1,2,3),key=lambda h:peak(seg[int(0.25*r):],h*f0)[1])
    ref=peak(seg[int(0.25*r):int(0.4*r)],best*f0)[0]
    for a,b in ((0.02,0.05),(0.05,0.09),(0.09,0.15)):
        fr=peak(seg[int(a*r):int(b*r)],best*f0)[0];out[f'glide{int(a*1000)}']=1200*math.log2(fr/ref)
    return out
x,r=m.mono(sys.argv[1]);res=[]
if len(sys.argv)>2 and sys.argv[2]=='--model':
    times=m.onsets(x,r);T=[t for t in times]
    for i,t in enumerate(T):
        nxt=T[i+1] if i+1<len(T) else len(x)/r
        if nxt-t<0.6:continue
        f=feats(x,r,t,nxt);f and res.append(f)
else:
    times=m.onsets(x,r)
    for i,t in enumerate(times):
        nxt=times[i+1] if i+1<len(times) else len(x)/r;prev=times[i-1] if i>0 else -10
        if nxt-t<0.6 or t-prev<0.25:continue
        if np.sqrt(np.mean(x[int(t*r):int((t+0.3)*r)]**2))<0.003:continue
        f=feats(x,r,t,nxt);f and res.append(f)
json.dump(res,sys.stdout,default=float)
