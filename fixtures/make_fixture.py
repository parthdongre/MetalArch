"""Deterministic synthetic trace generator; standard library, outside the C++ hot path."""
import math
from pathlib import Path

T = 1_000_000_000_000
STEP = 60_000_000_000
out = Path(__file__).with_name("synthetic_70.ma1")
with out.open("w", encoding="utf-8", newline="\n") as fp:
    fp.write("# MA1 deterministic synthetic test fixture; NOT recorded market data\n")
    for i in range(1, 71):
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