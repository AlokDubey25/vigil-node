import sys, os, json
import logging

sys.stdout.reconfigure(line_buffering = True)
ROOT  = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
sys.path.insert(0, ROOT)

from python.models.ensemble import ensemble_score, load_models
from python.utils.validators import validate_features
from python.utils.logger import get_logger
from python.models.explain import explain_text

log = get_logger("scorer")

def write(payload: dict):
    try:
        print(json.dumps({"ready": True}), flush=True)
    except BrokenPipeError:
        pass

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
    try:
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
            s = ensemble_score(features)
            payload = {"score": s}
            if s > 0.3:
                try:
                    payload["explanation"] = explain_text(features)
                except Exception as e:
                    log.warning(f"explain failed: {e}")
            write(payload)
                
    except (BrokenPipeError, EOFError):
        log.info("Pipe closed by Cpp - existing")
    except KeyboardInterrupt:
        log.info("Interrupted - existing")


if __name__ == "__main__":
    main()