"""Fit N64 effects beside the sequel's shared bank and five vehicle banks.

Only donor effects are low-pass filtered and decimated; retail audio is intact.
Pitch and loop positions follow the sample-rate change. Use the same external
psxavenc executable already required by the Dreamland importer.
"""
from pathlib import Path
import subprocess
import tempfile
import wave
import numpy as np


def resident_sample(samples, loop_start, pitch, encoder: Path):
    samples=np.asarray(samples,dtype=np.float64)
    # Windowed sinc suppresses frequencies above the new Nyquist limit.
    x=np.arange(-31,32,dtype=np.float64)
    taps=0.5*np.sinc(0.5*x)*np.hamming(len(x));taps/=taps.sum()
    reduced=np.convolve(samples,taps,mode='same')[::2]
    pcm=np.asarray(np.clip(np.rint(reduced),-32768,32767),dtype='<i2')
    with tempfile.TemporaryDirectory(prefix='v82_dreamland_audio_') as folder:
        source=Path(folder)/'input.wav';output=Path(folder)/'output.adpcm'
        with wave.open(str(source),'wb') as stream:
            stream.setnchannels(1);stream.setsampwidth(2);stream.setframerate(22050)
            stream.writeframes(pcm.tobytes())
        subprocess.run([str(encoder),'-q','-t','spu','-f','22050','-a','16','-D',str(source),str(output)],check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                       creationflags=getattr(subprocess,"CREATE_NO_WINDOW",0))
        frames=bytearray(output.read_bytes())
    count=(len(pcm)+27)//28
    if len(frames)<count*16:raise ValueError('SPU encoder truncated an effect')
    frames=frames[:count*16]
    loop_frame=None
    for i in range(count):frames[i*16+1]=0 if loop_start is None else 2
    if loop_start is None:frames[(count-1)*16+1]=1
    else:
        loop_frame=min(count,max(1,round(loop_start/2/28)))
        frames[(loop_frame-1)*16+1]=6;frames[(count-1)*16+1]=3
    return bytes(16)+frames,max(1,round(pitch/2)),dict(
        source_sample_count=len(samples),resident_sample_count=len(pcm),
        sample_rate_divisor=2,spu_frame_count=count+1,loop_spu_frame=loop_frame)
