import json, os

_config = None

def load_config(path="config/settings.json"):
    global _config
    if _config is None:
        with open(path) as f:
            _config = json.load(f)
    return _config
