import shap, pandas as pd
from python.models import ensemble

FEATURE_NAMES = ["velocity", "priceDeviation", "cancelRate",
                 "sizeRatio", "timeBetween", "repeatPriceRate"]
_explainer = None

def get_explainer():
    global _explainer
    if _explainer is None:
        ensemble.load_models()
        _explainer = shap.TreeExplainer(ensemble._xgb)
    return _explainer

def explain_text(features: list) -> str:
    explainer = get_explainer()
    df = pd.DataFrame([features], columns=FEATURE_NAMES)
    shap_values = explainer.shap_values(df)[0]
    total_abs = sum(abs(v) for v in shap_values) or 1e-9
    top = sorted(zip(FEATURE_NAMES, shap_values),
                 key=lambda x: abs(x[1]), reverse=True)[:3]
    parts = []
    for name, val in top:
        direction = "increased" if val > 0 else "decreased"
        pct = round(abs(val) / total_abs * 100, 1)
        parts.append(f"{name} {direction} the score ({pct}%)")
    return "; ".join(parts)