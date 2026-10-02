"""Exploratory P policy sensitivity. Works only on the SIMULATED fixture."""
import argparse
import csv
import hashlib
import json
import subprocess
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent
p=argparse.ArgumentParser()
p.add_argument("--trace",type=Path,default=ROOT/"fixtures"/"synthetic_70.ma1")
p.add_argument("--exe",type=Path,default=ROOT/"build"/"metalarch_compare")
p.add_argument("--runs",type=int,default=3)
p.add_argument("--cohort",choices=["core","expanded"],default="expanded")
p.add_argument("--warmup-bars",type=int,default=65)
p.add_argument("--budgets",type=int,nargs="+",default=[4_000_000,6_000_000,8_000_000,10_000_000,12_000_000,16_000_000])
p.add_argument("--outdir",type=Path,default=ROOT/"artifacts"/"budget_sweep")
a=p.parse_args()
a.outdir.mkdir(parents=True,exist_ok=True)
records=[]
for budget in a.budgets:
    output=a.outdir/f"events_budget_{budget}.csv"
    cmd=[str(a.exe.resolve()),str(a.trace.resolve()),str(output),str(a.runs),str(a.warmup_bars),str(budget),a.cohort]
    proc=subprocess.run(cmd,cwd=ROOT,check=True,text=True,capture_output=True)
    (a.outdir/f"run_budget_{budget}.txt").write_text(proc.stdout,encoding="utf-8")
    rows=list(csv.DictReader(output.open(encoding="utf-8",newline="")))
    for policy in ["B2_cache","P_freshness"]:
        sel=[r for r in rows if r["policy"]==policy]
        records.append({"budget_nominal_ns":budget,"policy":policy,"samples":len(sel),
                        "computations":sum(int(r["computations"]) for r in sel),
                        "deferrals":sum(int(r["deferred"]) for r in sel),
                        "cache_hits":sum(int(r["hits"]) for r in sel),
                        "valid":sum(int(r["valid"]) for r in sel),
                        "stale":sum(int(r["stale"]) for r in sel),
                        "unavailable":sum(int(r["unavailable"]) for r in sel),
                        "status_mismatches_vs_B0":sum(int(r["status_mismatches"]) for r in sel),
                        "unflagged_age_violations":sum(int(r["unflagged_age_violations"]) for r in sel),
                        "raw_sha256":hashlib.sha256(output.read_bytes()).hexdigest(),
                       })
(a.outdir/"sweep_summary.json").write_text(json.dumps({
 "kind":"EXPLORATORY_SYNTHETIC_SCHEDULER_SENSITIVITY_NOT_MAIN_RESULT",
 "cohort":a.cohort,"runs_per_budget":a.runs,"warmup_bars":a.warmup_bars,"trace_sha256":hashlib.sha256(a.trace.read_bytes()).hexdigest(),
 "candidate_budget_is_nominal_not_actual_CPU_time":True,"records":records},indent=2)+"\n",encoding="utf-8")
for rec in records:
 print(rec["budget_nominal_ns"],rec["policy"],"computes",rec["computations"],
       "valid",rec["valid"],"deferred",rec["deferrals"])
