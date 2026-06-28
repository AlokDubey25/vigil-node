# Vigil Node

Real-time pre-trade fraud detection engine.
C++ order book + Python ML ensemble + graph-based wash-trading
detection + a live terminal dashboard.

## What it does

Every order is validated, scored by two independent signals —
a trained ML ensemble (XGBoost + Random Forest) and a trading
network graph (cycle detection for wash trading) — and either
allowed into the order book or blocked before it can execute.

## Architecture

``` Structure
Order → SQLite (audit) → Blacklist check → Feature extraction
  → ML score (Python, via pipe) ─┐
  → Graph score (C++)            ├─→ combined verdict → allow / block
                                  ┘
Order book → matching engine → trades → SQLite → live dashboard
```

## Setup

```bash
git clone <repo-url> && cd vigil-node

# C++ engine
mkdir build && cd build && cmake .. && make && cd ..

# Python environment
python3 -m venv .venv && source .venv/bin/activate
pip install -r python/requirements.txt

# generate training data and train both ML models
python data/training_data/simulate_data.py
python python/models/train_xgb.py
python python/models/train_rf.py

# config
cp config/settings.example.json config/settings.json
```

## Run

```bash
./scripts/reset_db.sh
./build/vigil                              # terminal 1 — the engine

python python/dashboard/dashboard.py        # terminal 2 — live dashboard
```

## Tests

```bash
cd build && ctest --output-on-failure       # C++ — 5 binaries
pytest tests/python/ -v                     # Python — 33 tests
```

## Status

8 of 8 planned phases complete. See docs/architecture.md for
full system design and docs/schema.md for the database schema.
