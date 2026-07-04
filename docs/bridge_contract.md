# Bridge Contract: C++ <-> Python IPC

The bridge connects the deterministic C++ trading engine to the Python ML scorer. C++ owns the order pipeline, feature extraction, graph score, timeout/fallback behavior, and final allow/block decision. Python owns model loading, feature validation, ensemble inference, and optional explanation text.

The process boundary is intentional: a Python model crash, slow inference call, or validation error must not crash the trading engine.

## Participants

| Side         | File                        | Responsibility                                                                                             |
|--------------|-----------------------------|------------------------------------------------------------------------------------------------------------|
| C++ parent   |`../bridge.cpp`,`../bridge.h`| Fork Python, write one feature vector per order, read one JSON response, return a fraud score or fallback. |
| Python child | `../scorer.py`              | Load models, validate features, score with XGBoost + Random Forest ensemble, write JSON responses.         |

The normal command is:

```bash
python3 python/bridge/scorer.py
```

The engine should be launched from the repository root so this relative path resolves.

## Transport

The bridge uses two OS pipes created by C++:

| Pipe                           | Direction     | Payload                                              |
|--------------------------------|---------------|------------------------------------------------------|
| Parent write fd -> child stdin | C++ to Python | One newline-terminated JSON feature array per order. |
| Child stdout -> parent read fd | Python to C++ | One newline-terminated JSON object per request.      |

Rules for both sides:

- Encode every message as one JSON value on one line.
- Terminate every message with `\n`.
- Flush immediately after writing.
- Do not write logs to stdout from Python; stdout is reserved for protocol messages. Logs must go to stderr or a log file.
- Treat malformed, missing, empty, or late responses as bridge failures and use fallback scoring.

## Startup handshake

After the child process starts, Python loads the ML models before accepting feature vectors.

```text
Python -> C++
{"ready": true}\n
```

Startup behavior:

| Case                                                  | C++ behavior                                             |
|-------------------------------------------------------|----------------------------------------------------------|
| `{"ready": true}` received                            | Mark bridge ready and use live ML scoring.               |
| EOF, malformed JSON, error object, or startup timeout | Mark bridge not ready and use fallback score for orders. |

The architecture allows a longer startup wait than per-order scoring because model loading can be slower than inference.

## Per-order request

C++ sends the six-feature output from `FeatureExtractor::extract()` as a positional JSON array.

```json
[7.0, 0.12, 0.6, 4.5, 0.8, 0.75]
```

Feature order is fixed:

| Index | Name              | Meaning                                              | Expected range   |
|-------|-------------------|------------------------------------------------------|------------------|
| 0     | `velocity`        | Orders per minute in the user's rolling window.      | `0.0` to `100.0` |
| 1     | `priceDeviation`  | Distance from reference/mid price as a fraction.     | `0.0` to `1.0`   |
| 2     | `cancelRate`      | Cancelled orders divided by recent orders.           | `0.0` to `1.0`   |
| 3     | `sizeRatio`       | Current quantity divided by recent average quantity. | `0.0` to `20.0`  |
| 4     | `timeBetween`     | Time-spacing signal for recent orders.               | `0.0` to `300.0` |
| 5     | `repeatPriceRate` | Fraction of recent orders at the same price.         | `0.0` to `1.0`   |

The field names above are documentation names only. The wire payload is an array, not an object, to keep serialization small and stable.

## Successful response

Python returns a JSON object with a fraud probability.

```json
{"score": 0.845}
```

Architecture-level response shape also allows an explanation string:

```json
{"score": 0.845, "explanation": "high velocity and repeated price levels increased the score"}
```

Response fields:

| Field         | Required | Type   | Meaning                                                           |
|---------------|----------|--------|-------------------------------------------------------------------|
| `score`       | yes      | number | ML fraud probability from `0.0` safe to `1.0` fraudulent.         |
| `explanation` | no       | string | Human-readable model reason, typically SHAP-derived when enabled. |

C++ clamps accepted scores into `[0.0, 1.0]` before combining them with the graph score.

## Error response

Python may reject invalid input or report scoring failures:

```json
{"error": "velocity outside range [0, 100]"}
```

C++ must treat any response without a usable `score` as a bridge failure for that order and return the fallback score.

## Timeout and fallback

Per README architecture, per-order scoring is latency bounded by `bridge_timeout_ms` from `config/settings.json`.

Default:

```json
{
  "bridge_timeout_ms": 100
}
```

Failure handling contract:

| Failure              | C++ result           | Bridge readiness                           |
|----------------------|----------------------|--------------------------------------------|
| Python not ready     | `0.5` fallback score | Not ready                                  |
| Write to pipe fails  | `0.5` fallback score | Not ready                                  |
| Per-order timeout    | `0.5` fallback score | May remain ready if process is still alive |
| Empty response / EOF | `0.5` fallback score | Not ready                                  |
| Malformed JSON       | `0.5` fallback score | Ready state unchanged unless pipe failed   |
| `{"error": ...}`     | `0.5` fallback score | Ready state unchanged                      |
| Score outside range  | Clamp to `[0.0, 1.0]`| Ready state unchanged                      |

`0.5` is deliberately neutral: it does not declare the order safe, but it also does not let the Python process become a single point of failure.

## Final fraud decision

The bridge returns only the ML score. C++ computes the graph score independently and combines both signals:

```text
combined = mlScore * ml_weight + graphScore * graph_weight
```

Default weights from `config/settings.json`:

```json
{
  "fraud_threshold": 0.80,
  "ml_weight": 0.70,
  "graph_weight": 0.30
}
```

If `combined > fraud_threshold`, C++ rejects the order, escalates the user's action (`WARN`, `TEMP_BLOCK`, or `PERMANENT_BLOCK`), and writes a `risk_log` row. Otherwise the order enters the L2 order book.

## Compatibility notes

- The wire request must remain a six-element array unless both C++ `FeatureVector::toJSON()` and Python `validate_features()` are changed together.
- Python stdout is protocol-only. Any extra print can corrupt the bridge.
- New optional response fields are backward compatible as long as `score` remains present and numeric.
- Changes to feature ranges must be reflected in C++ feature extraction, Python validation, tests, and this contract.
