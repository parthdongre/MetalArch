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


## Phase III — audited ASEP2-derived native extensions (default expanded cohort)

All eight extensions depend only on the primary bar source. These are numerically specified **raw features** with a bounded lookback, not unvalidated copies of ASEP2's 0–100 scores. When a denominator is undefined, the engine emits UNAVAILABLE with no fresh numerical value. Per-bar volatility is intentionally *not annualized* because source intervals are not assumed equivalent.

| Native ID | Historical ASEP2 lineage | Native formula / minimum | Explicit adaptation |
|---|---|---|---|
| roc12 | `oscillator_bank.py` `_roc` | `100 * (close_t / close_(t-12) - 1)`; 13 bars | Raw % return, not fused score. |
| williams_r14 | `oscillator_bank.py` `_williams_r` | `-100 * (highest_high14-close)/(highest_high14-lowest_low14)`; 14 bars | Zero-range window is UNAVAILABLE, not a fabricated 0. |
| cci20 | `oscillator_bank.py` `_cci` | Typical price `(H+L+C)/3`, 20-bar mean/MAD, `(TP-mean)/(0.015*MAD)`; 20 bars | Zero MAD is UNAVAILABLE. |
| parkinson20 | `realized_vol.py` `_parkinson` | `sqrt(mean(log(H/L)^2)/(4*ln 2))`; 20 bars | Per bar; historical fixed annualization deliberately omitted. |
| garman_klass20 | `realized_vol.py` `_garman_klass` | `sqrt(max(0,mean(0.5*log(H/L)^2-(2ln2-1)*log(C/O)^2)))`; 20 bars | Per bar, roundoff-clamped nonnegative. |
| amihud20 | `amihud_roll.py` illiquidity component | `mean_20(abs(log(C_t/C_(t-1)))/(C_t*V_t))`; 21 bars | Denominator is price × base volume, *not verified USD traded value*. Zero included volume = UNAVAILABLE. Roll-spread component is **not** implemented. |
| volume_obv30 | `volume.py` OBV slope and surge | Local 30-bar OBV slope / 30-bar mean volume, multiplied by `0.5*tanh(slope)*clamp(last_volume/mean20,0,2)`; 30 bars | Local OBV origin avoids arbitrary historical offset; latest zero volume gives zero surge. Not identical to old normalized score. |
| cusum40 | `changepoint.py` `_cusum` component | 40 standardized log returns; positive and negative CUSUM with allowance 0.5; output final max statistic; 41 bars | Only CUSUM implemented; **not** BOCPD or a calibrated change probability. Flat return history produces statistic zero. |

Both the old 11-stage workload and this 19-stage expansion are built using the same descriptor-derived DAG and the same B0/B1/B2/B3/P policy evaluator. Obtain a machine-readable stage inventory from `metalarch_cli inventory core|expanded` instead of copying a historical engine count into the paper.
