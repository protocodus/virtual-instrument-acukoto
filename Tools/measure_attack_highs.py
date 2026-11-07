#!/usr/bin/env python3
"""How fast an attack's highs die (Docs/validation/attack-highs-2026-11-07.md).

    measure_attack_highs.py audio.wav attacks.json [audio.wav attacks.json ...]

For each note of measure_attacks.py's output with f0 from 180 to 950 Hz, by
register (180-300, 300-500, 500-950 Hz): the 1-2, 2-4, 4-8 and 8-14 kHz bands
(4th-order Butterworth, zero phase), squared and smoothed over 10 ms, and the
slope of a line through their level from 20 to 200 ms after the onset, in
dB/s. Prints each register's medians.
"""
import sys,json,numpy as np
sys.path.insert(0,str(__import__('pathlib').Path(__file__).parent))
import measure_koto_notes as m
from scipy import signal
bands=((1000,2000),(2000,4000),(4000,8000),(8000,14000))
def b(s,r,lo,hi):return signal.sosfiltfilt(signal.butter(4,[lo,hi],'bandpass',fs=r,output='sos'),s)
regs=((180,300),(300,500),(500,950))
out={}
for p,j in zip(sys.argv[1::2],sys.argv[2::2]):
    x,r=m.mono(p);d=json.load(open(j));R={g:[] for g in regs}
    for n in d:
        g=[g for g in regs if g[0]<n['f0']<=g[1]]
        if not g:continue
        o=int(n['t']*r);s=x[o:o+int(0.25*r)]
        if len(s)<int(0.25*r):continue
        row=[]
        for lo,hi in bands:
            y=b(s,r,lo,hi)**2;e=10*np.log10(np.convolve(y,np.ones(int(0.01*r))/int(0.01*r),'same')+1e-20)
            t=np.arange(len(e))/r;mm=(t>0.02)&(t<0.2)
            row.append(-np.polyfit(t[mm],e[mm],1)[0])
        R[g[0]].append(row)
    print('%-12s'%p.split('/')[-1][:12],' | '.join('%d-%dHz n=%d: '%(g[0],g[1],len(R[g]))+' '.join('%4.0f'%v for v in (np.median(R[g],0) if R[g] else [np.nan]*4)) for g in regs))
