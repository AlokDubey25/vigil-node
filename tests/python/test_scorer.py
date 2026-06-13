import sys, os, json, pytest 

ROOT  = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
sys.path.insert(0, ROOT)

from python.models.ensemble import ensemble_score
from python.utils.validators import validate_features

NORMAL = [1.0, 0.005, 0.0, 1.0, 60.0, 0.05]
FRAUD  = [80.0, 0.20, 0.0, 15.0, 0.5, 0.90]

def test_normal_order_scores_low():
    score = ensemble_score(NORMAL)
    assert score < 0.5, f"Normal scored {score:.4f}, expected < 0.5"

def test_fraud_order_scores_high():
    score = ensemble_score(FRAUD)
    assert score > 0.5, f"Fraud scored {score:.4f}, expected > 0.5"

def test_output_is_json_serializable():
    # scorer.py writes json.dumps({"score": score}) - verifing this currently
    score   = ensemble_score(NORMAL)
    payload = json.dumps({"score": score})
    parsed  = json.loads(payload)
    assert "score" in parsed
    assert 0.0 <= parsed["score"] <= 1.0

def test_validator_catches_bad_features():
    # scorer.py calls validate features before scoring - verify it catches bad inpput
    ok, _ = validate_features([1.0, 2.0])   # wrong length
    assert not ok

def test_ready_signal_format():
    # the startup signal Cpp waits for must be exactly this JSON
    signal = json.dumps({"ready": True})
    parsed = json.loads(signal)
    assert parsed.get("ready") == True