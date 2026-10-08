"""Measures how far the 4x interpolated peak of the plugin's output exceeds the limiter ceiling when the limiter is working.

usage: python3 limiter_peak.py recording.wav LimiterPeakProbe-binary

The recording is driven 12 dB up through the output gain, so that the limiter engages on its peaks, and rendered at
ceilings of 0, -1 and -3 dBFS.
"""
import subprocess, sys, tempfile, os, warnings, numpy as np, scipy.io.wavfile as wf, scipy.signal as sg
warnings.filterwarnings("ignore")
rate, x = wf.read(sys.argv[1])
x = x.astype(np.float64) / (2**15 if x.dtype == np.int16 else 2**31 if x.dtype == np.int32 else 1)
binary = os.path.abspath(sys.argv[2])
os.chdir(tempfile.mkdtemp())
x.astype(np.float32).tofile("in.f32")
for ceiling in ("0", "-1", "-3"):
    subprocess.run([binary, ceiling, "12"], check=True)
    y = np.fromfile("out.f32", dtype=np.float32).astype(np.float64)[20000:]
    sample, true = np.abs(y).max(), np.abs(sg.resample_poly(y, 4, 1)).max()
    print("ceiling %s dB: sample peak %.2f dBFS, true peak %.2f dBFS, excess over the ceiling %.2f dB" % (ceiling, 20 * np.log10(sample), 20 * np.log10(true), 20 * np.log10(true) - float(ceiling)))
