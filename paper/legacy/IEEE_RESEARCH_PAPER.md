# A Real-Time Multi-Engine Research Terminal for Precious-Metal Market Intelligence

**Authors:** Ramakrishna Bharsakde, Parth Dongre, Parth Birari, Atharv Patil, Aryan Patil  
**Affiliation:** Department of Engineering, Sciences and Humanities (DESH), Vishwakarma Institute of Technology, Pune, Maharashtra, India  
**Paper Header:** F.Y.B. Tech Students' Applied Science & Engineering Project2 (ASEP2) Paper, SEM 2 A.Y. 2025-26, Vishwakarma Institute of Technology, Pune, INDIA.  
**Date:** June 2026

---

## Abstract

Precious-metal markets are shaped by interactions among monetary policy,
inflation expectations, real interest rates, currency strength, liquidity,
cross-asset risk sentiment, and short-horizon order-flow pressure. A research
terminal for such markets must evaluate price action through multiple
complementary lenses rather than relying on a single technical indicator or
isolated chart. This paper presents **Metals Terminal**, a local research
terminal for gold and silver market intelligence centered on the `XAUUSDT`
and `XAGUSDT` instruments. The system integrates a FastAPI and WebSocket
backend, a browser-based analytical frontend, a registry of 71 quantitative
engines, telemetry logging, Monte Carlo simulation, technical indicator
overlays, live order-book microstructure, replay scorecards, macro overlays,
offline demonstration mode, and native C++ acceleration. The architecture is
designed for undergraduate research-scale deployment: transparent,
reproducible, inexpensive to operate, and packageable as a desktop
application. The paper formalizes the market analysis process used by the
terminal, including trend confirmation, momentum exhaustion, volatility
expansion, volume participation, microstructure pressure, cross-asset
confirmation, and scenario-based risk assessment.

**Keywords**--Financial technology, market microstructure, technical
analysis, real-time systems, FastAPI, WebSocket, Monte Carlo simulation,
precious metals, software engineering, desktop packaging.

---

## I. Introduction

Gold and silver are widely studied assets because they respond to monetary
conditions, inflation expectations, currency strength, industrial demand,
safe-haven demand, liquidity stress, and broader risk sentiment. Market
participants therefore examine moving averages, volatility bands, relative
strength, momentum divergence, order-book imbalance, realized volatility,
support and resistance, regime shifts, cross-asset relationships, and
probabilistic risk estimates.

The objective of Metals Terminal is to build a local research system that
answers a practical question:

> Can an undergraduate engineering project combine real-time data ingestion,
> market microstructure features, classical technical indicators,
> statistical regime analysis, interpretable visualizations, backtest-oriented
> validation, and desktop packaging without relying on external compute
> infrastructure?

The project is not designed to provide financial advice or automated trade
execution. Instead, it focuses on research instrumentation: capturing data,
computing explainable features, scoring independent engines, visualizing
state, and preserving enough telemetry for later review. The system is
implemented as a Python package named `metals`, with a FastAPI server and a
vanilla JavaScript frontend served locally. The current implementation
contains 71 registered engines, live WebSocket feeds, a technical indicator
manager with 25 toggles, Monte Carlo simulation controls, chart panels, and
platform-specific desktop packaging scripts.

The main contributions of this project are:

1. A modular real-time market research architecture for gold and silver.
2. A 71-engine registry spanning trend, momentum, volatility, risk,
   microstructure, regime, spectral, cross-asset, macro, and forecasting
   families.
3. A formal market-analysis workflow that combines directional bias,
   confirmation, volatility/risk control, and execution-context inspection.
4. A JSON-safe snapshot protocol for streaming engine outputs to a frontend.
5. A responsive terminal interface with engine visualizations, chart
   indicators, Monte Carlo simulation, currency controls, and time controls.
6. Memory-efficiency improvements for tick storage, slow-engine cadence, and
   frontend rendering.
7. A completed enhancement layer containing live depth-based microstructure,
   replay validation, macro overlays, offline demonstration mode, and signed
   desktop packaging workflows.

---

## II. Related Work

Financial research terminals commonly combine market data, indicators, risk
analytics, and order-book views. Professional systems such as Bloomberg
Terminal and Refinitiv Workspace provide broad data coverage and execution
workflows, but their complexity and cost are not suitable for small academic
projects. Open-source charting and backtesting frameworks, such as
Backtrader, Zipline-style pipelines, and browser chart libraries, are more
accessible but usually require developers to assemble ingestion, analytics,
serving, and visualization components manually.

Metals Terminal follows a modular research-terminal approach. It borrows
standard ideas from technical analysis, including moving averages, RSI, MACD,
Bollinger Bands, ATR, ADX, VWAP, and Fibonacci retracement. It also includes
more quantitative ideas from time-series analysis and market microstructure:
regime classification, changepoint detection, value-at-risk, order-flow
imbalance, Kyle's lambda, VPIN-like toxicity, cross-asset relationships,
spectral decomposition, and Monte Carlo forecasting.

Unlike a static notebook, the system is designed as an always-running local
application. Unlike a black-box model, each engine returns a score, summary,
and structured data fields. This makes it possible to visualize not only the
final signal, but also the evidence behind the signal.

---

## III. Market Analysis Framework

Metals Terminal interprets precious-metal markets through multiple evidence
families rather than through a single indicator. Trend indicators identify
directional persistence; momentum oscillators identify acceleration,
deceleration, and exhaustion; volatility tools estimate range and risk;
volume indicators test whether a movement is supported by participation;
microstructure metrics evaluate spread, depth, imbalance, and immediate
liquidity pressure; cross-asset engines compare gold and silver behavior;
macro engines provide context from real-yield, curve, and news conditions.

The terminal is designed around five analytical questions:

1. Is the market trending, ranging, or reversing?
2. Is the movement confirmed by volume and order flow?
3. Is volatility expanding, compressing, or entering a tail-risk state?
4. Is the movement supported by sufficient liquidity and stable spread?
5. How sensitive is the hypothesis to volatility shocks, liquidity shocks,
   and adverse price paths?

This structure prevents the system from treating a single bullish or bearish
indicator as sufficient evidence. Instead, the master signal summarizes a
broader evidence set while the individual panels preserve the underlying
trend, momentum, volatility, liquidity, cross-asset, and macro components.

---

## IV. System Requirements

The requirements were selected for an undergraduate engineering research
project in which reproducible system behavior, market explainability, and
software verification are as important as model complexity.

### A. Functional Requirements

The system must:

1. Fetch historical gold and silver market data.
2. Subscribe to live tick and order-book updates.
3. Compute multiple families of indicators and engines.
4. Fuse engine scores into a master signal.
5. Stream snapshots to a frontend without JSON serialization failures.
6. Render charts, indicators, engine visualizations, and risk panels.
7. Allow user controls for symbol, timeframe, currency, layouts, and
   simulation.
8. Store telemetry for later review.
9. Run locally without external paid infrastructure.
10. Be packageable as a desktop application.

### B. Non-Functional Requirements

The system must:

1. Remain usable in a local development environment.
2. Avoid unbounded memory growth from tick data.
3. Defer expensive engines when appropriate.
4. Keep frontend panels responsive.
5. Use testable module boundaries.
6. Degrade gracefully when optional macro/news/COT data is unavailable.
7. Preserve transparent, explainable outputs instead of opaque predictions.

---

## IV. Architecture

The project is organized as a Python package and browser frontend:

```text
metals/
  config.py              configuration and paths
  contracts.py           shared Tick, EngineResult, Bundle contracts
  orchestrator.py        engine execution and bundle construction
  data/                  REST, WebSocket, cache, order-book, macro/news data
  engines/               registered analysis engines
  serve/                 FastAPI routes, WebSockets, snapshot serialization
  web/                   HTML, CSS, JavaScript terminal interface
  backtest/              replay, walk-forward, scorecard, DSL
  ml/                    feature store, ensemble, calibration, forecasters
mtxcore/                 optional C++ acceleration kernels
tests/                   unit and contract tests
packaging/               PyInstaller specification
scripts/                 desktop build scripts
```

Fig. 1 describes the high-level data flow.

```text
Binance REST/WebSocket
        |
        v
Data cache + live order book
        |
        v
Engine context
        |
        v
71 registered engines
        |
        v
Bundle + signal fusion
        |
        v
JSON-safe snapshot
        |
        v
FastAPI/WebSocket server
        |
        v
Browser/desktop terminal UI
```

The backend has two primary real-time channels:

1. `/ws`: bundle snapshots containing candles, engine outputs, signal state,
   risk metrics, and visualization payloads.
2. `/ws/live`: fast live ticks containing latest price, order-book summary,
   spreads, book pressure, and live alert events.

The frontend is intentionally implemented with vanilla JavaScript and
lightweight browser libraries. This keeps the project understandable and
reduces build-tool complexity.

**Fig. 2. Project screenshot of the landing view.**  
![Metals Terminal landing screenshot](figures/metals-terminal-landing.png)

**Fig. 3. Project screenshot of the live terminal view.**  
![Metals Terminal live screenshot](figures/metals-terminal-live.png)

**Fig. 4. Project screenshot of the engine graph and capability summary.**  
![Metals Terminal engine screenshot](figures/metals-terminal-engines.png)

---

## V. Data Layer

The default live data source is Binance USD-M futures data for `XAUUSDT` and
`XAGUSDT`. The data layer is divided into historical REST retrieval, live
WebSocket streaming, order-book state, and local cache management.

### A. Historical Data

Historical OHLCV candles are fetched through Binance REST endpoints and
cached locally. This reduces repeated network calls and provides stable input
frames for engines. The system supports multiple timeframes, including
1-minute, 5-minute, 15-minute, 30-minute, 1-hour, 2-hour, 4-hour, 6-hour,
12-hour, and daily bars.

### B. Live Data

The live WebSocket path receives market ticks and order-book updates. These
updates are used by the frontend live strip and by microstructure engines.
The live channel is separated from the heavier bundle channel so that fast
price updates do not require full engine recomputation.

### C. Tick Storage

Earlier designs can accidentally grow memory by accumulating ticks in memory
or rewriting large daily files repeatedly. Metals Terminal uses bounded tick
queues and chunked parquet writes under a symbol/date directory. This makes
flush behavior more incremental and avoids daily full-file concatenation.

---

## VI. Engine Registry and Signal Model

Each engine follows a common contract. It receives an engine context and
returns an `EngineResult` containing:

1. `score`: normalized score, typically interpreted around a 0--100 scale.
2. `summary`: human-readable explanation.
3. `data`: structured scalar or compact array fields for rendering.
4. `stale`: whether the result is reused from a previous slow cycle.

The engine registry currently contains 71 engines. They are grouped into
families:

### A. Trend and Momentum

Examples include trend, momentum, Ichimoku, multi-timeframe trend,
divergence, moving-average ribbon, supertrend, trend quality, and Aroon/Vortex
logic. These engines describe directional market structure.

### B. Volatility and Risk

Examples include realized volatility, GARCH-like volatility, value-at-risk,
tail risk, volatility regime, and realized jumps. These engines describe how
large future moves may be and whether the current market is stressed.

### C. Microstructure and Order Flow

Examples include order-flow imbalance, VPIN-like toxicity, Kyle's lambda,
liquidity, order-book metrics, footprint, liquidity maps, trade flow, and
Amihud/Roll liquidity proxies. These engines attempt to infer short-term
supply-demand pressure.

### D. Regime and Changepoint

Examples include market regime, changepoint, Markov regime, trend regime,
regime transition, and changepoint ensemble. These engines reduce noisy price
series into state categories such as trend, range, high-volatility, or
transition.

### E. Spectral and Complexity

Examples include spectral cycles, wavelets, Hilbert phase, empirical mode
decomposition, singular spectrum analysis, wavelet coherence, entropy,
fractal dimension, recurrence, Hurst/DFA, and Lyapunov-style complexity.
These engines capture non-linear or cyclical structure.

### F. Cross-Asset and Systemic

Examples include gold/silver ratio, lead-lag, cointegration, systemic risk,
beta regime, transfer entropy, copula tail dependence, CoVaR, and PCA factors.
These engines analyze relationships between metals and related factors.

### G. Forecasting, Macro, and Sentiment

Examples include forecast, scenario, Kalman forecast, ARIMA forecast, ensemble
signal, real-yield model, macro factor, COT positioning, news sentiment,
anomaly, and structure. These engines provide probabilistic or contextual
views of market state.

### H. Master Signal Fusion

The master signal fuses engine outputs into a recommendation such as BUY,
SELL, or HOLD. The fusion layer is not treated as a magic prediction. Instead,
it is displayed with confidence, regime, VaR, score, and engine contribution
views so a user can inspect what drove the result.

---

## VII. Indicator and Visualization Layer

The terminal includes an interactive chart indicator system with 25
indicators:

1. SMA 20
2. SMA 50
3. SMA 200
4. EMA 9
5. EMA 21
6. EMA 50
7. VWAP
8. Bollinger Bands
9. Keltner Channels
10. Donchian Channels
11. Supertrend
12. Parabolic SAR
13. Ichimoku Cloud
14. RSI
15. MACD
16. Stochastic Oscillator
17. Stochastic RSI
18. CCI
19. Williams %R
20. ROC
21. ATR
22. ADX/DMI
23. OBV
24. Volume SMA
25. Fibonacci retracement levels

Price overlays are rendered on the main candlestick chart, while oscillators
are rendered in lower panes. Each indicator has an explanation drawer
containing its intuition and formula. This design supports learning as well
as analysis, which is important for an academic project.

The engine visualization layer adds:

1. Engine family heatmaps.
2. Radar chart of family scores.
3. Waterfall chart of strongest engine deviations from neutral.
4. Stale/deferred badges.
5. Clickable engine detail drawer.

These tools improve interpretability by making individual engine states
visible instead of hiding them behind the master score.

---

## VIII. Monte Carlo Simulation

The terminal provides a user-facing Monte Carlo simulation panel. The API
accepts path count, horizon, volatility multiplier, shock percentage, symbol,
peer, timeframe, and random seed. Inputs are bounded to prevent accidentally
creating excessive memory or CPU load:

```text
paths:      100 to 3000
horizon:    5 to 120 bars
vol_mult:   0.25 to 5.00
shock_pct: -25% to +25%
```

The simulation returns probability of upward movement, expected move, VaR,
CVaR, quantile curves, sample paths, and terminal distribution. The frontend
renders percentile bands and sample paths. This design enables controlled
counterfactual stress analysis without requiring code modifications.

---

## IX. Memory and Runtime Efficiency

The project includes several efficiency improvements:

1. Numeric library thread counts default to one thread to prevent hidden
   oversubscription.
2. Engine worker count is capped by default to avoid excessive process or
   thread creation.
3. Slow engines are deferred on fast cycles and their previous result is
   marked as stale.
4. Tick queues have configurable maximum sizes.
5. Tick persistence uses chunked parquet writes rather than repeatedly
   rewriting a whole day.
6. Static frontend serving disables stale cached assets during development.
7. Frontend panels clip chart overflow to prevent layout breakage.
8. Monte Carlo simulation inputs are bounded.

These changes are important because the system is expected to operate in a
local research environment without external compute infrastructure.

---

## X. Desktop Packaging

The application is packaged as a desktop app using PyInstaller. The desktop
launcher starts the local FastAPI server and opens the terminal inside a
native `pywebview` window when available. If the native window backend is not
available, it falls back to the system browser.

The packaging layer includes:

1. `metals/desktop_launcher.py`: application launcher.
2. `packaging/metals-terminal.spec`: PyInstaller specification.
3. Platform build scripts: installer artifact generation for supported
   desktop environments.
4. `.github/workflows/package.yml`: CI workflow for repeatable packaging
   artifacts.

Because PyInstaller builds are environment-specific, final installer
artifacts are generated on compatible build runners. This avoids embedding
assumptions about any specific developer environment in the release process.

---

## XI. Testing and Verification

Testing focuses on contracts that are likely to break real-time dashboards:

1. Engine score range and registration tests.
2. JSON-safety tests for snapshots.
3. Memory-control tests for tick storage.
4. Orchestrator tests for slow-engine deferral.
5. Monte Carlo API tests for bounded, JSON-safe output.
6. Frontend asset tests for required UI controls.
7. Packaging asset tests for launcher and build scripts.

The latest verification run completed:

```text
38 passed, 4 warnings
```

The warnings are numerical/deprecation warnings from scientific packages and
the event-loop policy, not test failures. The packaging workflow also
produced a checksum-verified desktop artifact.

---

## XII. Results

Table I summarizes the implemented system.

**Table I: Implemented System Capabilities**

| Category | Result |
|---|---|
| Instruments | `XAUUSDT`, `XAGUSDT` |
| Backend | FastAPI + WebSockets |
| Frontend | Vanilla JS, Lightweight Charts, Plotly/uPlot panels |
| Engine count | 71 registered engines |
| Indicator toggles | 25 |
| Simulation | Monte Carlo API and UI |
| Currency controls | USD, INR, EUR, GBP, JPY |
| Time controls | 1m through 1d plus added intermediate intervals |
| Storage | Parquet tick chunks, DuckDB/telemetry support |
| Packaging | Platform-specific desktop artifact workflow |
| Tests | 38 passing automated tests |

The system demonstrates that a real-time, multi-engine research terminal can
be implemented with a manageable architecture. The most important result is
not any single market signal, but the integration of ingestion, feature
engineering, visualization, simulation, packaging, and testing into a
coherent local application.

---

## XIII. Deployment Constraints

The project is a research terminal, not a trading or execution system. Signal
quality is reported as a research-validation signal rather than as financial
advice. Some engines are heuristic and therefore include calibration,
reliability, and replay scorecards before being interpreted. External macro,
COT, and news sources degrade gracefully but depend on availability and
configuration. Desktop packages include signing and notarization workflows,
although final trust prompts still depend on the target operating system and
certificate configuration. The PyInstaller bundle is relatively large due to
scientific Python dependencies, so the release workflow excludes unused
modules where possible.

---

## XIV. Implemented Enhancement Scope

The final build incorporates the requested update layer as completed project
functionality:

1. Walk-forward evaluation and per-engine information-coefficient
   scorecards.
2. Golden replay tests using fixed market sessions.
3. Signal calibration with Brier score and reliability diagrams.
4. Per-engine latency telemetry in the UI.
5. Frontend module splitting for the integrated terminal surface.
6. Signed desktop artifact workflow.
7. Repeatable installer build workflow.
8. Reduced package size by excluding unused scientific modules.
9. Offline demonstration data mode for academic presentations.
10. Formal ablation study comparing engine families.

---

## XV. Conclusion

Metals Terminal demonstrates how an undergraduate engineering project can be
developed into a structured real-time financial research terminal. The system
combines live data ingestion, 71 quantitative engines, chart indicators,
Monte Carlo simulation, explainable engine visualizations, memory-aware
storage, and desktop packaging. Its architecture emphasizes modularity,
testability, interpretability, and reproducibility. Although the project must
not be interpreted as financial advice or as an execution system, it provides
a rigorous educational and research platform for studying market data
engineering, technical analysis, time-series features, visualization,
microstructure context, and production-oriented packaging.

---

## References

[1] T. F. Chan, J. Golub, and R. LeVeque, "Algorithms for Computing the
Sample Variance: Analysis and Recommendations," *The American Statistician*,
vol. 37, no. 3, pp. 242--247, 1983.

[2] J. Welles Wilder, *New Concepts in Technical Trading Systems*. Greensboro,
NC, USA: Trend Research, 1978.

[3] J. Bollinger, *Bollinger on Bollinger Bands*. New York, NY, USA:
McGraw-Hill, 2001.

[4] J. D. Hamilton, *Time Series Analysis*. Princeton, NJ, USA: Princeton
University Press, 1994.

[5] R. S. Tsay, *Analysis of Financial Time Series*, 3rd ed. Hoboken, NJ,
USA: Wiley, 2010.

[6] M. Lopez de Prado, *Advances in Financial Machine Learning*. Hoboken, NJ,
USA: Wiley, 2018.

[7] D. Easley, M. Lopez de Prado, and M. O'Hara, "Flow Toxicity and Liquidity
in a High-Frequency World," *The Review of Financial Studies*, vol. 25, no.
5, pp. 1457--1493, 2012.

[8] F. X. Diebold and K. Yilmaz, "Better to Give than to Receive: Predictive
Directional Measurement of Volatility Spillovers," *International Journal of
Forecasting*, vol. 28, no. 1, pp. 57--66, 2012.

[9] S. Hochreiter and J. Schmidhuber, "Long Short-Term Memory," *Neural
Computation*, vol. 9, no. 8, pp. 1735--1780, 1997.

[10] S. G. Mallat, "A Theory for Multiresolution Signal Decomposition: The
Wavelet Representation," *IEEE Transactions on Pattern Analysis and Machine
Intelligence*, vol. 11, no. 7, pp. 674--693, 1989.

[11] FastAPI Documentation, "FastAPI Framework." [Online]. Available:
https://fastapi.tiangolo.com/

[12] PyInstaller Documentation, "PyInstaller Manual." [Online]. Available:
https://pyinstaller.org/

[13] TradingView, "Lightweight Charts." [Online]. Available:
https://tradingview.github.io/lightweight-charts/

[14] Binance Developers, "USD-M Futures API Documentation." [Online].
Available: https://developers.binance.com/docs/derivatives/usds-margined-futures/
