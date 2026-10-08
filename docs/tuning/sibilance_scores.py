"""Scores sibilance measures against frame labels taken from a spectrogram of the same recording.

Frames are 20 ms (hop) with a 2048-sample Hann window. A frame is active when its total power is within 40 dB of the 95th
percentile, sibilant when the 4 to 9 kHz power exceeds the 100 Hz to 1 kHz power, and clearly non-sibilant (voiced) when it is
more than 15 dB below it. The measures come from SibilanceProbe, which writes the band envelopes made with the plugin's own
filters and detectors, and the output of the plugin's SibilanceDetector, for the same frames.

usage: python3 sibilance_scores.py recording.wav frames.csv [rendered.wav ...]

Each rendered file (written by VocalRenderProbe from the same recording, aligned to it) is reported as the mean level change of
two bands, in dB, over the sibilant frames and over the voiced frames.
"""
import sys
import warnings
import numpy as np
import scipy.io.wavfile as wavfile
import scipy.signal as signal
from scipy.stats import rankdata

warnings.filterwarnings("ignore")  # the WAV writer's extra chunks


def load(path):
    rate, x = wavfile.read(path)
    if x.dtype == np.int32:
        return rate, x.astype(np.float64) / 2**31
    if x.dtype == np.int16:
        return rate, x.astype(np.float64) / 2**15
    return rate, x.astype(np.float64)


def power(x, rate):
    _, _, spectrum = signal.stft(x, rate, nperseg=2048, noverlap=2048 - int(0.02 * rate), window="hann")
    return np.abs(spectrum) ** 2


def auc(a, b):
    ranks = rankdata(np.concatenate([a, b]))
    return (ranks[: len(a)].sum() - len(a) * (len(a) + 1) / 2) / (len(a) * len(b))


def ramp(values, low, high):
    u = np.clip((values - low) / (high - low), 0.0, 1.0)
    return u * u * (3.0 - 2.0 * u)


rate, recording = load(sys.argv[1])
frames = np.genfromtxt(sys.argv[2], delimiter=",", names=True)
p = power(recording, rate)
freqs = np.fft.rfftfreq(2048, 1.0 / rate)
n = min(p.shape[1], len(frames))


def band(spectrum, lo, hi):
    return spectrum[(freqs >= lo) & (freqs < hi), :n].sum(0)


db = lambda v: 10.0 * np.log10(v + 1e-30)
contrast = db(band(p, 4000, 9000)) - db(band(p, 100, 1000))
total = db(band(p, 50, 22050))
active = total > np.percentile(total, 95) - 40.0
sibilant = active & (contrast > 0.0)
voiced = active & (contrast < -15.0)
ambiguous = active & (contrast <= 0.0) & (contrast >= -15.0)
print("frames %d, active %d, sibilant %d, clearly non-sibilant %d, in between %d\n" % (n, active.sum(), sibilant.sum(), voiced.sum(), ambiguous.sum()))

env = {name: np.maximum(frames[name][:n], 1e-9) for name in frames.dtype.names if name not in ("time", "detector")}
candidates = {
    "6.5 kHz band-pass / 9 kHz high-pass band (v1.3.2)": env["bp65"] / env["hp9"],
    "6.5 kHz band-pass / broadband": env["bp65"] / env["broad"],
    "4th order 4-9 kHz band-pass / broadband": env["bp49"] / env["broad"],
    "3 kHz high-pass / broadband": env["hp3"] / env["broad"],
    "6.5 kHz band-pass / 3 kHz high-pass": env["bp65"] / env["hp3"],
    "4th order 4-9 kHz band-pass / 100 Hz-1 kHz band": env["bp49"] / env["mid"],
}
print("%-52s %6s | sibilant P10/P50/P90      | voiced P50/P90/P99" % ("measure", "AUC"))
for name, values in candidates.items():
    print("%-52s %6.3f | %7.3f %7.3f %7.3f | %7.3f %7.3f %7.3f" % (name, auc(values[sibilant], values[voiced]), *np.percentile(values[sibilant], [10, 50, 90]), *np.percentile(values[voiced], [50, 90, 99])))

print("\nramp on the 4th order band-pass / broadband ratio: mean value on sibilant frames, share above 0.5 | mean on voiced frames, share above 0.2 | mean on in-between frames, share of active frames above 0.2")
ratio = candidates["4th order 4-9 kHz band-pass / broadband"]
for low, high in [(0.06, 0.30), (0.08, 0.35), (0.10, 0.40), (0.10, 0.50), (0.15, 0.45)]:
    s = ramp(ratio, low, high)
    print("(%.2f, %.2f): %.3f %.3f | %.4f %.4f | %.3f %.4f" % (low, high, s[sibilant].mean(), (s[sibilant] > 0.5).mean(), s[voiced].mean(), (s[voiced] > 0.2).mean(), s[ambiguous].mean(), (s[active] > 0.2).mean()))

if "detector" in frames.dtype.names:
    d = frames["detector"][:n]
    print("\nSibilanceDetector output (frame mean): sibilant frames mean %.3f, share above 0.5 %.3f | voiced frames mean %.4f, share above 0.2 %.4f | in-between mean %.3f | AUC %.3f"
          % (d[sibilant].mean(), (d[sibilant] > 0.5).mean(), d[voiced].mean(), (d[voiced] > 0.2).mean(), d[ambiguous].mean(), auc(d[sibilant], d[voiced])))

if len(sys.argv) > 3:
    print("\nband level change of a rendered file over the recording, mean dB: sibilant frames | voiced frames")
    for path in sys.argv[3:]:
        _, y = load(path)
        y = np.pad(y, (0, max(0, len(recording) - len(y))))[: len(recording)]
        py = power(y, rate)
        parts = []
        for low, high in [(3000, 9000), (9000, 10700)]:
            change = db(band(py, low, high)) - db(band(p, low, high))
            parts.append("%d-%d Hz: %.2f | %.2f" % (low, high, change[sibilant].mean(), change[voiced].mean()))
        print("%-24s %s" % (path.split("/")[-1], ";  ".join(parts)))
