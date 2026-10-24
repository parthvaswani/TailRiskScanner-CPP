# asymmetric-leap-scanner

A C++20 LEAP put scanner prototype for credit-sensitive auto lenders. It prices European puts with Black-Scholes, solves implied volatility with Newton-Raphson, calculates annualized realized volatility from closes, and flags candidates when `HV / IV` exceeds the configured threshold.

The checked-in provider is deterministic mock data for CACC, CPSS, and CRMT. It exercises the `IBKRMarketDataProvider` boundary but does not connect to Trader Workstation or IB Gateway. Implement that boundary with the official IBKR C++ socket API (`EClientSocket`/`EWrapper`) to use live data; the IBKR SDK is not bundled with this repository.

## Build

Requirements: CMake 3.20+, and a C++20 compiler. On Linux, the build links `Threads::Threads`; on Windows it additionally links `ws2_32`.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Configure and run

Copy `config/app_config.json.template` to `config/app_config.json`, then adjust the IBKR connection values and scanner thresholds. The generated config file is ignored by Git. Current execution uses the mock provider, so the connection values are accepted for interface compatibility and no TWS/IB Gateway connection is made.

```sh
./build/asymmetric-leap-scanner
```

Pass a different JSON config path as the first argument if needed:

```sh
./build/asymmetric-leap-scanner path/to/app_config.json
```

Without a config file, built-in defaults are used. Candidate puts must have DTE strictly greater than `minimum_dte`, a negative delta greater than `maximum_put_delta` (for example, greater than -0.20), and an `HV / IV` ratio strictly greater than `minimum_asymmetry_ratio` to receive the opportunity signal.

## Model notes

The pricing model assumes European exercise, continuous compounding, and no dividends. Realized volatility is the sample standard deviation of log returns annualized by `sqrt(252)`. The sample provider generates reproducible synthetic quotes and close histories; it is suitable for build and workflow checks, not investment decisions.
