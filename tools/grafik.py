#!/usr/bin/env python3
"""grafik.py - bar chart (mean ± stddev) of sure_ms per scenario from an olcum CSV."""
import csv
import statistics as st
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

girdi, cikti = sys.argv[1], sys.argv[2]
veri = {}
with open(girdi, newline="") as f:
    for r in csv.DictReader(f):
        veri.setdefault(r["senaryo"], []).append(float(r["sure_ms"]))

ad = list(veri)
ort = [st.mean(veri[a]) for a in ad]
sd = [st.stdev(veri[a]) if len(veri[a]) > 1 else 0.0 for a in ad]
n = len(next(iter(veri.values())))

fig, ax = plt.subplots(figsize=(7, 3.6))
ax.bar(ad, ort, yerr=sd, capsize=4, color="#3b6ea5")
ax.set_ylabel("süre (ms)")
ax.set_title(f"Senaryo başına çalışma süresi (ort ± sd, n={n})")
ax.tick_params(axis="x", rotation=25)
fig.tight_layout()
fig.savefig(cikti, dpi=130)
