import sys, os, json
import logging

ROOT  = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
sys.path.insert(0, ROOT)

from python.models.ensemble import ensemble_score, load_models
from python.utils.validators import validate_features
from python.utils.logger import get_logger

log = get_logger("scorer")

def write(payload: dict):
    # One JSON line to stdout and flush immediately.
    # flush=True - without it cpp blocks forever
    print(json.dumps(payload), flush=True)

def main():
    # Startip : load models
    log.info("Loadinf mmodels......")
    try:
        load_models()
    except Exception as e:
        log.error(f"Failed to load models: {e}")
        write({"error": f"model load failed: {e}"})
        sys.exit(1)

    # send ready signal - Cpp waits for this before sending order
    write({"ready": True})
    log.info("Scorer ready - waiting for feature vectors")

    # main loop: one feature vector in, one score out
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue

        # parse JSON from Cpp
        try:
            features = json.loads(line)
        except json.JSONDecodeError as e:
            log.error(f"JSON parse error on: {line!r}")
            write({"error": "invalid JSON"})
            continue

        # validate befroe scoreing - catches Cpp bugs early
        ok, reason = validate_features(features)
        if not ok:
            log.warning(f"Validation failed: {reason}")
            write({"error": reason})
            continue

        # score and reply
        try:
            score = ensemble_score(features)
            log.debug(f"score={score:.4f} features={features}")
            write({"score": score})
        except Exception as e:
            log.error(f"Scoring error: {e}")
            write({"error": str(e)})

if __name__ == "__main__":
    main()
'''
there is an error in douwnloading something like files , lib and ubuntu issue
'''
