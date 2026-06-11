import os, sys, pytest 
import pandas as pd

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
sys.path.insert(0, ROOT)

MODEL_PATH = os.path.join(ROOT, "models/saved/fraud_model_xgb.pkl")
FEATURES   = ["velocity", "priceDeviation", "cancelRate", "sizeRatio", "timeBetween", "repeatPriceRate"]

#   helpers
def load_model():
    import joblib
    return joblib.load(MODEL_PATH)

def score(model, values: list) -> float:
    """ GET fraud probability for a feature vector..."""
    df = pd.DataFrame([values], columns=FEATURES)
    return float(model.predict_proba(df)[0][1])

# TEST 01 - model file must exist
def test_model_file_exists():
    assert os.path.exists(MODEL_PATH), (
        f"Model not found: {MODEL_PATH}\n"
        "   Fix: python python/models/train_xgb.py"
    )
    
# Test 02 - model load wtihout error
def test_model_loads():
    model = load_model()
    assert model is not None

# Test 03 - prediction are always 0.0 to 1.0
def test_prediction_in_range():
    model = load_model()
    # normal order
    p = score(model, [2.0, 0.01, 0.0, 1.0, 45.0, 0.05])
    assert 0.0 <= p <= 1.0, f"Score out of range: {p}"

# Test 04 - obvious fraud scores higher than obvious normal
def test_fraud_scores_higher_than_normal():
    model = load_model()

    # textbook normal: low velocity, near mid, small size, regular timing
    normal_score = score(model, [1.0, 0.005, 0.0, 1.0, 60.0, 0.05])

    # textbook normal: low velocity, near mid, small size, regular timing
    fraud_score = score(model, [80.0, 0.20, 0.0, 15.0, 0.5, 0.90])

    assert fraud_score > normal_score, (f"Expected fraud ({fraud_score:.4f}) > normal ({normal_score:.4f})")

# Test 05 - normal order scores below 0.5
def test_normal_order_low_score():
    model = load_model()
    p = score(model, [1.0, 0.005, 0.0, 1.0, 60.0, 0.05])
    assert p < 0.5, f"Normal order should score below 0.5, got {p:.4f}"