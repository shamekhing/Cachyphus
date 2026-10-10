#!/usr/bin/env python3
"""Render the CASHYPHUS effect recordings and the ending's ambience.
Uses NumPy and the system libsndfile for OGG Vorbis export; neither is needed at runtime.

Ogg output is not byte-reproducible: libsndfile gives each stream a random serial
number, so re-running rewrites the .ogg container even when the audio inside it is
identical. Compare the decoded samples, not the files -- tools/verify_audio.py does.
"""
from pathlib import Path
import ctypes as C
import wave
import subprocess
import tempfile
import numpy as np

ROOT = Path(__file__).resolve().parent.parent
A = ROOT / 'assets/audio'
RATE = 44100


def read(path):
    with wave.open(str(path)) as w:
        raw = w.readframes(w.getnframes())
        a = np.frombuffer(raw, dtype='<i2').astype(np.float32).reshape(-1,w.getnchannels()).mean(axis=1)/32768
        rate = w.getframerate()
    if rate != RATE:
        a = np.interp(np.arange(round(len(a)*RATE/rate))*rate/RATE, np.arange(len(a)),a).astype(np.float32)
    return a


def trim(a, seconds=.5):
    a = a[:int(seconds*RATE)].copy()
    if len(a):
        fade = min(len(a)//4, int(.012*RATE))
        a[:fade] *= np.linspace(0,1,fade)
        a[-fade:] *= np.linspace(1,0,fade)
    return a


def pitch(a, semitones):
    ratio = 2**(semitones/12)
    return np.interp(np.arange(max(1,int(len(a)/ratio)))*ratio,np.arange(len(a)),a).astype(np.float32)


def norm(a, peak=.65):
    m=np.max(np.abs(a)) if len(a) else 0
    return (a*min(4,peak/m) if m else a).astype(np.float32)


def put(dst, src, at, gain=1):
    i=int(at*RATE)
    if i<0: src=src[-i:]; i=0
    k=min(len(src),len(dst)-i)
    if k>0: dst[i:i+k]+=src[:k]*gain


def wav(name,a):
    out=A/'sfx'/name
    # A constant offset is a step at both edges of a one-shot: the speaker cone
    # is already displaced when the file starts and snaps back when it ends, and
    # that is a click. Measured, walk_away carried +0.022 of it. Remove the mean
    # before taking the headroom, so every effect starts and ends at zero.
    if len(a): a=a-a.mean()
    # Two of these are a raw transient cut out of a longer recording rather than a
    # trimmed sample -- measured, dialogue_tick opened at 0.198 and choice_select at
    # 0.287 -- and a one-shot that starts at 0.29 is a step, which is the click
    # this game has already shipped once. Three milliseconds at each end puts every
    # effect on and off silence.
    fade=min(int(.003*RATE),len(a)//4) if len(a) else 0
    if fade>0:
        a[:fade]*=np.linspace(0,1,fade)
        a[-fade:]*=np.linspace(1,0,fade)
    a=np.clip(a,-.95,.95)
    with wave.open(str(out),'wb') as w:
        w.setnchannels(1);w.setsampwidth(2);w.setframerate(RATE)
        w.writeframes((a*32767).astype('<i2').tobytes())

# libsndfile writes OGG/Vorbis directly and is used only by this authoring tool.
class Info(C.Structure):
    _fields_=[('frames',C.c_longlong),('samplerate',C.c_int),('channels',C.c_int),('format',C.c_int),('sections',C.c_int),('seekable',C.c_int)]
sf=C.CDLL('libsndfile.so.1')
sf.sf_open.argtypes=[C.c_char_p,C.c_int,C.POINTER(Info)];sf.sf_open.restype=C.c_void_p
sf.sf_writef_float.argtypes=[C.c_void_p,C.POINTER(C.c_float),C.c_longlong];sf.sf_writef_float.restype=C.c_longlong
sf.sf_close.argtypes=[C.c_void_p]
def ogg(path,a):
    a=np.asarray(a,dtype=np.float32)
    if a.ndim==1:a=np.column_stack((a,a))
    a=np.clip(a,-.95,.95).copy()
    info=Info(0,RATE,2,0x200000|0x0060,0,0)
    fd=sf.sf_open(str(path).encode(),0x20,C.byref(info))
    if not fd:raise RuntimeError(f'cannot open OGG {path}')
    written=sf.sf_writef_float(fd,a.ctypes.data_as(C.POINTER(C.c_float)),len(a))
    sf.sf_close(fd)
    if written!=len(a):raise RuntimeError(f'short OGG write {path}')

coins=[norm(trim(read(A/'sources/coin_8bit'/f'coin{i}.wav'),.32)) for i in range(1,11)]
def decoded(path):
    decoder = ROOT / 'build/debug/cashyphus_decode'
    if not decoder.exists():
        decoder = ROOT / 'build/release/cashyphus_decode'
    if not decoder.exists():
        raise SystemExit('Build the native game first to provide cashyphus_decode')
    with tempfile.TemporaryDirectory() as d:
        out = Path(d) / 'decoded.wav'
        subprocess.run([str(decoder), str(path), str(out)], check=True, stdout=subprocess.DEVNULL)
        return read(out)
real=[norm(trim(decoded(A/'sources/coin_real'/f'coin.{i}.ogg'),.6)) for i in range(1,13)]
jingle=norm(decoded(A/'sources/coin_jingle/coinsounds011015.ogg'))
paper=norm(read(A/'sources/paper/Paper Crushed - 1.wav'))
paper2=norm(read(A/'sources/paper/Paper Sound - 4.wav'))
birds=norm(decoded(A/'sources/nature/birds-isaiah658.ogg'))
wind=norm(decoded(A/'sources/nature/wind woosh loop.ogg'))
leaves=norm(decoded(A/'sources/nature/moving leaves stereo.ogg'))
steps_l=norm(decoded(A/'sources/nature/Fantozzi-SandL1.ogg'))
steps_r=norm(decoded(A/'sources/nature/Fantozzi-SandR1.ogg'))
more=[norm(trim(read(A/'sources/money_more'/f'Money_{i:02d}.wav'),.5)) for i in range(1,7)]
casino=norm(trim(decoded(A/'sources/casino/chips-collide-1.ogg'),.5))
drop_src=read(A/'sources/coin_drop/coin_drop.wav')
drop_onset=max(0,int(np.flatnonzero(np.abs(drop_src)>.02)[0]) - int(.012*RATE))
drop=norm(trim(drop_src[drop_onset:],.65))
purchase=norm(trim(read(A/'sources/purchase/snd_purchase.wav'),1.15))

# Each cue has a distinct edit, source family, or rhythmic layer.
for i in range(4):wav(f'push_0{i+1}.wav',norm(pitch(coins[i],[-1,0,1,-2][i]),.56))
wav('push_strong.wav',norm(pitch(coins[4],-5)+np.pad(real[0][:5000],(0,max(0,len(pitch(coins[4],-5))-5000)))[:len(pitch(coins[4],-5))]*.35,.65))
wav('brace_start.wav',norm(real[1],.45));wav('brace_loop.wav',norm(real[2],.35));wav('brace_end.wav',norm(real[3],.4))
wav('ball_roll_loop.wav',norm(real[4],.32));wav('ball_slip.wav',norm(np.concatenate((real[5],real[6])),.6))
wav('stamina_low.wav',norm(paper,.45));wav('grip_exhausted.wav',norm(np.concatenate((more[2],real[7])),.6))
wav('aging.wav',norm(drop,.5))
wav('summit.wav',norm(np.concatenate((casino,coins[8],coins[9],jingle[:int(.3*RATE)])),.7))
wav('death.wav',norm(drop,.45));wav('ball_downhill.wav',norm(np.concatenate((real[9],real[10],real[11])),.62))
wav('reincarnation.wav',norm(np.concatenate((more[0][:2205],purchase,coins[9])),.68))
wav('dialogue_tick.wav',norm(coins[0][:int(.045*RATE)],.22));wav('choice_select.wav',norm(coins[2][:int(.12*RATE)],.42))
wav('walk_away.wav',norm(paper2[:int(.8*RATE)],.3))

# The music is NOT rendered here. It is MintoDog's CC0 chiptune, baked into the
# binary by tools/gen_music.py and worn down at playback time -- see CREDITS.md.
#
# What this script makes is the effects and the ending's ambience.


# Four CC0 field recordings: birds, wind, rustling leaves and footsteps.
# No money-derived material appears in the freedom ambience.
length=int(19.2*RATE)
nature=np.zeros(length,dtype=np.float32)
for src,gain in ((birds,.14),(wind,.08)):
    tiled=np.tile(src,int(np.ceil(length/len(src))))[:length]
    nature+=tiled*gain
for bar in range(8):
    put(nature,leaves[:int(.8*RATE)],bar*2.4+1.0,.06)
for step in range(10):
    step_audio=steps_l if step%2==0 else steps_r
    put(nature,step_audio[:int(.4*RATE)],.7+step*.7,.11 if step<5 else .07)
xf=int(.7*RATE)
nature[:xf]=nature[:xf]*np.linspace(0,1,xf)+nature[-xf:]*np.linspace(1,0,xf)
nature[-xf:]*=np.linspace(1,0,xf)
# The field recordings are quiet on their own: measured, the raw bed peaked at
# 0.095 against the score's 0.38, which is inaudible under the ambience bus and
# would leave the walk-away ending silent. The ambience is a bed, not a whisper,
# so it gets the same normalise every other output gets before it is written.
oga=norm(np.column_stack((nature,nature)),.5)
ogg(A/'ambience/freedom.ogg',oga)
print('rendered',len(list((A/'sfx').glob('*.wav'))),'effects,',len(list((A/'music').glob('*.ogg'))),'music loops, and ambience')
