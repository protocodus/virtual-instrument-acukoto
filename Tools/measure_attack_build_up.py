#!/usr/bin/env python3
"""How each band of an attack builds up (Docs/validation/attack-build-up-2026-11-07.md).

    measure_attack_build_up.py audio.wav attacks.json [audio.wav attacks.json ...]

For each note of measure_attacks.py's output with f0 from 180 to 950 Hz, in
the 0.2-0.8, 0.8-1.5, 1.5-3, 3-6 and 6-12 kHz bands (4th-order Butterworth,
zero phase), squared and smoothed over 0.5 ms or two periods of the band's
lower edge, whichever is longer, from the refined onset: the time the band
first passes 3 dB under its peak over -2 to 40 ms, and its mean level over
the first 1.5 ms against 4-12 ms. Prints each band's medians over the notes.
"""
import sys,json,numpy as np
sys.path.insert(0,str(__import__('pathlib').Path(__file__).parent))
import measure_koto_notes as m
from scipy import signal
B=[(200,800),(800,1500),(1500,3000),(3000,6000),(6000,12000)]
for wav,js in zip(sys.argv[1::2],sys.argv[2::2]):
    d=[n for n in json.load(open(js)) if 180<n['f0']<950];x,r=m.mono(wav)
    rise={b:[] for b in B};early={b:[] for b in B}
    for n in d:
        o=int(n['t']*r)
        for lo,hi in B:
            e=signal.sosfiltfilt(signal.butter(4,[lo,hi],'bandpass',fs=r,output='sos'),x[o-int(0.05*r):o+int(0.06*r)])**2
            w=int(max(0.0005,2/lo)*r);e=np.convolve(e,np.ones(w)/w,'same');z=int(0.05*r)
            pk=e[z-int(0.002*r):z+int(0.04*r)].max()
            rise[(lo,hi)].append((np.argmax(e[z-int(0.002*r):]>pk*10**-0.3)-int(0.002*r))/r*1000)
            early[(lo,hi)].append(10*np.log10(e[z:z+int(0.0015*r)].mean()/e[z+int(0.004*r):z+int(0.012*r)].mean()))
    print(f'{wav}: {len(d)} notes')
    for b in B:
        print('  %5d-%5d Hz  to -3 dB %4.1f ms   first 1.5 ms against 4-12 ms %+5.1f dB'%(b[0],b[1],np.median(rise[b]),np.median(early[b])))
