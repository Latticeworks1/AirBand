"""Scores the sibilance detector of the plugin against two candidates built on the contrast between 5 to 10 kHz and 1 to 5 kHz.

The detector is reproduced from SibilanceDetector.h: two cascaded 6 kHz band-passes (Q 1.5) over the whole signal, both
followed with a 1 ms attack and 50 ms release. The candidates are the contrast of the levels of 4th order Butterworth
band-passes at 5 to 10 kHz and 1 to 5 kHz, in dB, and the same contrast plus the 5 to 10 kHz level over the whole signal.
Frame labels are those of sibilance_scores.py. Each measure is also evaluated on white and pink noise at -40 dBFS and on
tones, and the result states how many voiced and sibilant frames of the recording read lower than the noise or tone.

usage: python3 sibilance_contrast.py recording.wav
"""
import sys, warnings, ctypes, subprocess, tempfile, os, numpy as np, scipy.signal as sg, scipy.io.wavfile as wf
warnings.filterwarnings("ignore")
from scipy.stats import rankdata
os.chdir(tempfile.mkdtemp())
open("env.c", "w").write("""void env(const float*x,float*y,int n,float a,float r){float s=0;for(int i=0;i<n;i++){float v=x[i]<0?-x[i]:x[i];float c=v>s?a:r;s=c*s+(1-c)*v;y[i]=s;}}""")
subprocess.run(["clang", "-O2", "-shared", "-o", "env.dylib", "env.c"], check=True)
lib = ctypes.CDLL("./env.dylib")
rate, x = wf.read(sys.argv[1])
x = x.astype(np.float64) / (2**15 if x.dtype == np.int16 else 2**31 if x.dtype == np.int32 else 1)
def envelope(v):
    v = np.ascontiguousarray(v, dtype=np.float32); y = np.empty_like(v)
    lib.env(v.ctypes.data_as(ctypes.c_void_p), y.ctypes.data_as(ctypes.c_void_p), len(v), ctypes.c_float(np.exp(-1 / (0.001 * rate))), ctypes.c_float(np.exp(-1 / (0.05 * rate))))
    return np.maximum(y, 1e-9).astype(np.float64)
bp = lambda lo, hi, v: sg.sosfilt(sg.butter(4, [lo, hi], "bandpass", fs=rate, output="sos"), v)
def rbj(f0, q):
    w = 2*np.pi*f0/rate; a = np.sin(w)/(2*q); b0, b1, b2 = a, 0, -a; a0, a1, a2 = 1+a, -2*np.cos(w), 1-a
    return np.array([[b0/a0, b1/a0, b2/a0, 1, a1/a0, a2/a0]])
bp6 = lambda v: sg.sosfilt(np.vstack([rbj(6000, 1.5)]*2), v)
def measures(v):
    tot = envelope(v); old = envelope(bp6(v)) / tot
    hi = envelope(bp(5000, 10000, v)); lo = envelope(bp(1000, 5000, v))
    new = 20*np.log10(hi/lo); oldrel = 20*np.log10(hi/tot)
    return {"old: 6k BP / total (ratio)": old, "new: L5-10k - L1-5k (dB)": new, "hybrid: new + (L5-10k - total) (dB)": new + oldrel}
M = measures(x)
# labels, as sibilance_scores.py
hop = int(0.02*rate)
_, _, S = sg.stft(x, rate, nperseg=2048, noverlap=2048-hop, window="hann"); P = np.abs(S)**2
fr = np.fft.rfftfreq(2048, 1/rate); n = P.shape[1]
band = lambda lo, hi: P[(fr >= lo) & (fr < hi)].sum(0)
db = lambda v: 10*np.log10(v+1e-30)
c = db(band(4000, 9000)) - db(band(100, 1000)); t = db(band(50, 22050))
act = t > np.percentile(t, 95) - 40; sib = act & (c > 0); voi = act & (c < -15)
idx = np.minimum(np.arange(n)*hop, len(x)-1)
def auc(a, b):
    r = rankdata(np.concatenate([a, b])); return (r[:len(a)].sum() - len(a)*(len(a)+1)/2) / (len(a)*len(b))
rng = np.random.default_rng(1); N = 5*rate
white = 0.01*rng.standard_normal(N)
pink = sg.lfilter([0.049922035, -0.095993537, 0.050612699, -0.004408786], [1, -2.494956002, 2.017265875, -0.522189400], rng.standard_normal(N)); pink *= 0.01/pink.std()
tt = np.arange(N)/rate
sigs = {"white -40 dBFS": white, "pink -40 dBFS": pink}
for f in (3000, 5000, 7000, 10000, 12000): sigs["tone %d Hz -30 dBFS" % f] = 0.03*np.sin(2*np.pi*f*tt)
print("frames active %d, sibilant %d, voiced %d" % (act.sum(), sib.sum(), voi.sum()))
for name, m in M.items():
    f = m[idx]; a, b = f[sib], f[voi]
    print("\n%s  AUC %.3f  sibilant P10/P50/P90 %.2f %.2f %.2f  voiced P50/P90/P99 %.2f %.2f %.2f" % (name, auc(a, b), *np.percentile(a, [10, 50, 90]), *np.percentile(b, [50, 90, 99])))
    for sn, s in sigs.items():
        v = np.median(measures(s)[name][rate:])
        print("   %-20s median %7.2f | above %5.1f%% of voiced, %5.1f%% of sibilant frames" % (sn, v, 100*np.mean(b < v), 100*np.mean(a < v)))
