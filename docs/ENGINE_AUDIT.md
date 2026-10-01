# Engine cohort — implementation inventory (2026-10-02)

All functions below are C++20 and operate only on declared inputs. These are calculated features or **illustrative engineering workload expressions**, not verified trading predictors. Data minimums and implementation definitions are part of the paper's workload declaration.

| Stage | Declared direct inputs / upstream | Method / minimum | Qualification |
|---|---|---|---|
| trend | XAU 1m bars | EMA(8)-EMA(21), normalized by last close; >=21 bars | EMA initialized from first *retained* observation |
| momentum | XAU bars | Wilder RSI(14); >=15 bars | 50 on genuinely unchanged prices; no absent-result fallback |
| volatility | XAU bars | sqrt(mean squared log-return) on last 20 returns; >=21 bars | per-bar, not annualized; not a model forecast |
| atr | XAU bars | simple average true range of last 14; >=15 bars | explicit SMA rather than Wilder ATR |
| peer_corr | XAU/XAG bars | Pearson correlation of at least 12 matched returns; both current and preceding timestamps must match | zero variance = UNAVAILABLE |
| book_imbalance | last XAU book event | (bid quantity - ask quantity)/(sum) | only a top-of-book snapshot, not VPIN/order flow |
| spectral | XAU bars | 64 demeaned log returns; Goertzel frequency bins k=1..8; maximum bin energy / sum energy; >=65 bars | spectral concentration proxy, not wavelet coherence |
| regime | trend, volatility | tanh(trend/(eps+100*volatility)) | explicitly heuristic |
| risk | volatility, ATR | volatility + 0.001*ATR | explicitly heuristic |
| forecast | trend, momentum | 0.7*trend + 0.3*(RSI-50)/50 | deterministic illustrative fixture, not trained or predictive |
| fusion | regime, risk, forecast, peer_corr, book_imbalance | fixed linear combination; all parents required VALID | no calibrated probability/trade decision |

Bar source freshness threshold 120s; book source 5s. Stale direct parents make downstream unavailable. The threshold is a demo policy, not experimentally optimized. Per-engine cache keys incorporate declared source-content digests and ancestor identities. `ReadView` prohibits undeclared source reads; undeclared upstream results are not supplied. Histories are bounded at a default 4096 events per key; sliding retention affects EMA initialization and is an explicit workload assumption.