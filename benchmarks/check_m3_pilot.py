"""Validate a paired *diagnostic* only. Not a speedup significance test."""
from __future__ import annotations
import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

def sha256(p: Path) -> str:
    with p.open('rb') as f:
        h=hashlib.sha256()
        for part in iter(lambda:f.read(1<<20),b''): h.update(part)
    return h.hexdigest()

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('baseline',type=Path)
    parser.add_argument('accelerated',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    base=json.loads((args.baseline/'summary.json').read_text())
    inc=json.loads((args.accelerated/'summary.json').read_text())
    bm=json.loads((args.baseline/'manifest.json').read_text())
    am=json.loads((args.accelerated/'manifest.json').read_text())
    if bm['trace_sha256']!=am['trace_sha256'] or bm['registered_stages']!=am['registered_stages'] or bm['registered_stages']!=19:
        raise ValueError('experiment mismatch: traces and stage counts must agree')
    if bm['cohort']!='expanded' or am['cohort']!='expanded-inc':
        raise ValueError('unexpected reference/candidate cohort')
    if set(base)!=set(inc):raise ValueError('policies are not identical')
    policy={}
    for name in sorted(base):
        left,right=base[name],inc[name]
        for prop in ('samples','computations','deferred'):
            if left[prop]!=right[prop]:raise ValueError(f'{name} unexpected change in {prop}')
        if name in ('B0_full','B1_parallel','B2_cache'):
            if left['status_mismatches_vs_B0'] or right['status_mismatches_vs_B0']:
                raise ValueError(f'{name}: internal oracle mismatch')
        policy[name]={
            'baseline_p50_us':left['wall_p50_us'],
            'incremental_p50_us':right['wall_p50_us'],
            'difference_us':right['wall_p50_us']-left['wall_p50_us'],
            'baseline_p95_us':left['wall_p95_us'],
            'incremental_p95_us':right['wall_p95_us'],
            'same_compute_invocations':True,
            'same_explicit_deferrals':True,
            'samples_each':left['samples']
        }
    report={
        'status':'SINGLE_HOST_SYNTHETIC_M3_DIAGNOSTIC_NOT_PUBLICATION_CLAIM',
        'created_utc':datetime.now(timezone.utc).isoformat(),
        'trace_sha256':bm['trace_sha256'],
        'registered_stages':19,
        'source_modes':['SIMULATED'],
        'baseline_raw_csv_sha256':sha256(args.baseline/'events.csv'),
        'incremental_raw_csv_sha256':sha256(args.accelerated/'events.csv'),
        'baseline_manifest_sha256':sha256(args.baseline/'manifest.json'),
        'incremental_manifest_sha256':sha256(args.accelerated/'manifest.json'),
        'caveats':['Studies ran sequentially, not randomized or CPU-pinned',
                   'Timings include synthetic ingestion and full graph execution',
                   'Opt-in rolling index uses 880 extra bytes per enabled bar stream on tested ABI',
                   'Separate descriptor revisions yield deliberately different fingerprint identities',
                   'No statistically established full-workload performance improvement'],
        'policies':policy
    }
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2,sort_keys=True)+'\n')
    print(f"validated {len(policy)} policies; trace {report['trace_sha256']}; output {args.output}")

if __name__=='__main__':main()
