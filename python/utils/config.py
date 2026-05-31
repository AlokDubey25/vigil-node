import json
from pathlib import Path

# Resolve it here 
_ROOT = Path(__file__).resolve().parent.parent.parent
_config = None

def load_config(path="config/settings.json") -> dict:
    global _config
    if _config is None:
        config_path = _ROOT / path
        try:
            with open(path) as f:
                _config = json.load(f)
        except FileNotFoundError:
            raise SystemExit(
                f"\n[ERROR] settings.json not found at {config_path}\n"
                f"Run: cp config/settings.example.json config/settings.json\n"
            )
    return _config
