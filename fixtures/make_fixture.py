"""Deterministic synthetic trace generator; standard library, outside the C++ hot path."""
import argparse
import math
from pathlib import Path

T = 1_000_000_000_000
STEP = 60_000_000_000
parser = argparse.ArgumentParser(description="Deterministic SIMULATED MA1 workload generator")
parser.add_argument("--bars", type=int, default=70)
parser.add_argument("--start-index", type=int, default=1,
                    help="Separate calibration/evaluation intervals without overlapping events")
parser.add_argument("--output", type=Path, default=Path(__file__).with_name("synthetic_70.ma1"))
args = parser.parse_args()
if args.bars < 1 or args.bars > 100000 or args.start_index < 1 or args.start_index + args.bars > 1000000:
    parser.error("bars must be in 1..100000 and start-index + bars at most 1000000")
out = args.output
out.parent.mkdir(parents=True, exist_ok=True)
with out.open("w", encoding="utf-8", newline="\n") as fp:
    fp.write("# MA1 deterministic synthetic test fixture; NOT recorded market data\n")
    for i in range(args.start_index, args.start_index + args.bars):
        ts = T + i * STEP
        x = 120.0 + 0.08 * i + 2.0 * math.sin(i * 0.6)
        y = 90.0 + 0.12 * i + 1.5 * math.cos(i * 0.4)
        for symbol, close, delay in [("XAU", x, 1), ("XAG", y, 2)]:
            nums = [close, close * 1.01, close * 0.99, close, 100.0 + i]
            fp.write("|".join(["MA1", "B", "fixture", symbol, "1m", str(i), str(ts),
                                str(ts + delay * 1_000_000_000),
                                *(repr(n) for n in nums), "2"]) + "\n")
        fp.write("|".join(["MA1", "L", "fixture", "XAU", "live", str(i), str(ts),
                            str(ts + 3_000_000_000), "100", "101", "100", "50", "0", "2"]) + "\n")
print(out)
