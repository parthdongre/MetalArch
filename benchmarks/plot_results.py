"""Rebuild optional diagnostic SVG figures from raw CSV; matplotlib not core dependency."""
from __future__ import annotations
import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

p=argparse.ArgumentParser()
p.add_argument("csv",type=Path)
p.add_argument("--engine-count",type=int,default=None,
               help="required unless manifest.json beside CSV declares registered_stages")
p.add_argument("--outdir",type=Path,required=True)
a=p.parse_args()
a.outdir.mkdir(parents=True,exist_ok=True)
manifest=a.csv.parent/"manifest.json"
count=a.engine_count
if count is None and manifest.exists():
 count=json.loads(manifest.read_text(encoding="utf-8")).get("registered_stages")
if not isinstance(count,int) or count<1:
 p.error("engine count unknown: provide --engine-count or adjacent manifest.json registered_stages")
by=defaultdict(list)
with a.csv.open(newline="",encoding="utf-8") as f:
 for r in csv.DictReader(f):by[r["policy"]].append(r)
fig,ax=plt.subplots(figsize=(8,4.6))
for name, rows in sorted(by.items()):
 values=sorted(float(r["wall_us"]) for r in rows)
 ax.plot(values,[(i+1)/len(values) for i in range(len(values))],label=name)
ax.set(xlabel="Local input-ingestion and execution latency (microseconds)",ylabel="Empirical CDF",
       title="MetalArch synthetic diagnostic — not real market latency")
ax.legend();ax.grid(alpha=.2)
fig.tight_layout();fig.savefig(a.outdir/"latency_cdf.svg");plt.close(fig)
fig,ax=plt.subplots(figsize=(7.5,4.6))
for name, rows in sorted(by.items()):
 comp=sum(int(r["computations"]) for r in rows)
 deferred=sum(int(r["deferred"]) for r in rows)
 total=count*len(rows)
 ax.scatter(comp,deferred/total,label=name,s=55)
 ax.annotate(name,(comp,deferred/total),xytext=(4,4),textcoords="offset points",fontsize=8)
ax.set(xlabel="Attempted compute invocations (all repeated trials)",ylabel="Explicitly deferred / all output slots",
       title="Synthetic modeled-cost trade-off; status differs from B0")
ax.grid(alpha=.2)
fig.tight_layout();fig.savefig(a.outdir/"compute_vs_deferral.svg");plt.close(fig)
print(a.outdir)
