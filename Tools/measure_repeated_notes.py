#!/usr/bin/env python3
"""Measure how a repeated note's strokes vary (Docs/validation/repeated-notes-2026-11-07.md).

    measure_repeated_notes.py <wav> <from s> <to s> [shortest gap s]

Onsets are spectral-flux peaks at 1.5-8 kHz (512-sample Hann frames, 64-sample
hop, threshold the median plus 1.2 deviations, at least the gap apart, 50 ms
by default). For each onset: the level over 3 ms before to 25 ms after it,
and the spectral centroid of the 30 ms after it. Prints the intervals, the
levels and centroids, their spreads, even against odd strokes, and the lag-1
correlations of the intervals and levels. Recordings with mains hum cannot be
timed closer than its period this way.
"""
import sys,numpy as np,soundfile as sf
sys.path.insert(0,'/tmp/claude-0/rep')
x,r=sf.read(sys.argv[1]);x=x if x.ndim==1 else x.mean(1)
t0,t1=float(sys.argv[2]),float(sys.argv[3]);seg=x[int(t0*r):int(t1*r)+int(0.05*r)]
N=512;H=64
fr=np.lib.stride_tricks.sliding_window_view(seg,N)[::H]*np.hanning(N)
S=np.abs(np.fft.rfft(fr,axis=1)); f=np.fft.rfftfreq(N,1/r)
band=(f>1500)&(f<8000)
flux=np.maximum(0,np.diff(np.log(S[:,band]+1e-5),axis=0)).sum(1)
thr=np.median(flux)+1.2*np.std(flux)
on=[];last=-99
for i in range(1,len(flux)-1):
    if flux[i]>thr and flux[i]>=flux[i-1] and flux[i]>=flux[i+1] and (i-last)*H/r>(float(sys.argv[4]) if len(sys.argv)>4 else 0.05) and (i+1)*H/r<t1-t0:
        on.append((i+1)*H/r+N/2/r);last=i
on=np.array(on)
F=[]
for t in on:
    a=int(t*r);w=seg[a-int(0.003*r):a+int(0.025*r)]
    s2=seg[a:a+int(0.03*r)]*np.hanning(int(0.03*r));SS=np.abs(np.fft.rfft(s2));ff=np.fft.rfftfreq(len(s2),1/r)
    F.append((20*np.log10(np.sqrt(np.mean(w**2))),np.sum(ff*SS)/np.sum(SS)))
F=np.array(F);ioi=np.diff(on)*1000
print(len(on),'ioi',np.round(ioi).astype(int).tolist())
print('lvl',np.round(F[:,0]-F[:,0].mean(),1).tolist())
print('cen',np.round(F[:,1]).astype(int).tolist())
if len(on)>5:
  print('ioi mean %.0f cv %.1f%%  even-odd ioi %.1f ms | lvl even-odd %.2f dB | cen even/odd %.3f | lvlSD %.2f cenSD %.1f%% | lag1 corr ioi %.2f lvl %.2f'%(ioi.mean(),ioi.std()/ioi.mean()*100,ioi[::2].mean()-ioi[1::2].mean(),F[::2,0].mean()-F[1::2,0].mean(),F[::2,1].mean()/F[1::2,1].mean(),F[:,0].std(),F[:,1].std()/F[:,1].mean()*100,np.corrcoef(ioi[:-1],ioi[1:])[0,1],np.corrcoef(F[:-1,0],F[1:,0])[0,1]))
