import sys
import json

# reconfigure stdout to flush every line immediately
sys.stdout.reconfigure(line_buffering=True)
sys.stderr.reconfigure(line_buffering=True)

# send ready signal RIGHT NOW — before any heavy imports
# this is the only way to beat the WSL /mnt/c/ import slowness
print(json.dumps({"ready": True}), flush=True)

# NOW do all the slow imports — C++ is already unblocked
import logging
import os
import sys as _sys

logging.basicConfig(
    stream=sys.stderr,
    level=logging.INFO,
    format='%(asctime)s | scorer | %(levelname)s | %(message)s',
    datefmt='%H:%M:%S'
)
log = logging.getLogger(__name__)

log.info("Loading models...")

# add project root to path so imports work
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
if ROOT not in sys.path:
    sys.path.insert(0, ROOT)

from python.models.ensemble import ensemble_score, load_models
from python.utils.validators import validate_features

load_models()
log.info("Models loaded — scoring active")

def write(payload: dict):
    print(json.dumps(payload), flush=True)

# main scoring loop
for raw_line in sys.stdin:
    line = raw_line.strip()
    if not line:
        continue
    try:
        features = json.loads(line)
        ok, msg = validate_features(features)
        if not ok:
            write({"error": msg})
            continue

        s = ensemble_score(features)
        payload = {"score": float(s)}

        if s > 0.3:
            try:
                from python.models.explain import explain_text
                payload["explanation"] = explain_text(features)
            except Exception as e:
                log.warning(f"explain failed: {e}")

        write(payload)

    except json.JSONDecodeError as e:
        log.error(f"bad JSON input: {e}")
        write({"error": f"JSON parse error: {e}"})
    except Exception as e:
        log.error(f"scoring error: {e}")
        write({"error": str(e)})