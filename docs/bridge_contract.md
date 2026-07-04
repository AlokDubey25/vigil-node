# Bridge Contract — C++ ↔ Python Protocol

## Startup

Python → C++: {"ready": true}\n

## Per order (C++ → Python)

[velocity, price_dev, cancel_rate,
 size_ratio, time_between, repeat_price]\n
Example: [12.0, 0.45, 0.62, 1.8, 180.5, 0.3]\n

## Response (Python → C++)

{"score": float}\n
Example: {"score": 0.8731}\n

## Error response

{"error": "message"}\n

## Rules

- Always newline-terminated
- Always flush immediately after writing
- Score is float 0.0 (safe) to 1.0 (fraud)
- C++ fallback score: 0.5 if timeout > 100ms
