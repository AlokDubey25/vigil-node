import os
import pandas as pd
import joblib

ROOT     = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
XGB_PATH = os.path.join(ROOT, "models/saved/fraud_model_xgb.pkl")
RF_PATH  = os.path.join(ROOT, "models/saved/fraud_model_rf.pkl")

FEATURES = ["velocity", "priceDeviation", "cancelRate", "sizeRatio", "timeBetween", "repeatPriceRate",]

# to save cache and loads once when process starts, stays in RAM
_xgb = None
_rf  = None

def load_models():
    global _xgb, _rf
    if not os.path.exists(XGB_PATH):
        raise FileNotFoundError(
            f"XGBoost model not found: {XGB_PATH}\n"
            "   Run: python3 python/models/train_xgb.py"
        )
    
    if not os.path.exists(RF_PATH):
        raise FileNotFoundError(
            f"RF model not found: {RF_PATH}\n"
            "   Run: python3 python/models/train_rf.py"
        )

    if _xgb is None:
        _xgb = joblib.load(XGB_PATH)
    if _rf is None:
        _rf = joblib.load(RF_PATH)

def ensemble_score(features: list) -> float:
    """
    Takes 6 floats [vel, pdev, crate, sratio, tbetween, rpprice].
    Returns fraud probability 0.0 (safe) to 1.0 (fraud).
    Average of XGBoost and Random Forest.
    """
    load_models()
    df = pd.DataFrame([features], columns=FEATURES)
    xgb_prob = float(_xgb.predict_proba(df)[0][1])
    rf_prob  = float(_rf.predict_proba(df)[0][1])

    return round((xgb_prob + rf_prob)/ 2.0, 4)

