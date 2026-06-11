import os, sys
import pandas as pd
import joblib
from sklearn.model_selection import train_test_split
from sklearn.metrics import ( classification_report, confusion_matrix, f1_score, precision_score, recall_score, roc_auc_score)

ROOT        = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
MODEL_PATH  = os.path.join(ROOT, "models/saved/fraud_model_xgb,pkl")
DATA_PATH   = os.path.join(ROOT, "data/trainig_data/processed/features_train.csv")

FEATURES = ["velocity", "priceDeviation", "cancelRate", "sizeRatio", "timeBetween", "repeatPriceRate", ]

def evalute():
    # LOAD MODEL
    if not os.path.exists(MODEL_PATH):
        print(f"[ERROR] Model not found: {MODEL_PATH}")
        print(" Run: python python/models/train_xgb.py")
        sys.exit(1)

    model = joblib.load(MODEL_PATH)

    # reproduce the exact same test split as training
    df = pd.read_csv(DATA_PATH)
    X, y = df[FEATURES], df["is_fraud"]
    _, X_test, _, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)
    y_pred = model.predict(X_test)
    y_prob = model.predict_proba(X_test)[:,1]

    # REPORT
    print("=================================")
    print("     Vigil Node - Model Report   ")
    print("=================================")
    print(f"Test rows  : {len(y_test)}")
    print(f"Fraud rows : {y_test.sum()}")
    print()
    print(classification_report(y_test, y_pred, target_names=["normal", "fraud"]))
    print("-- Key metrics (fraud class) --")
    print(f"Precision : {precision_score(y_test, y_pred):.4f}")
    print(f"Recall    : {recall_score(y_test, y_pred):.4f}")
    print(f"F1 Score  : {f1_score(y_test, y_pred):.4f}")
    print(f"ROC AUC   : {roc_auc_score(y_test, y_prob):.4f}")

    # CONFUSION MATRIX
    cm = confusion_matrix(y_test, y_pred)
    print("\n  CONFUSION MATRIX  ")
    print(f"    True  Normal   (correct)    : {cm[0][0]}")
    print(f"    False Positive (overkill)   : {cm[0][1]}")
    print(f"    False Negative (missed!)    : {cm[1][0]}")
    print(f"    True  Fraud    (caught)     : {cm[1][1]}")

    # features importance
    print("\n  FEATURE IMPORTANCE  ")
    imps = model.feature_importances_
    for name, imp in sorted(zip(FEATURES, imps), key=lambda x: x[1], reverse=True):
        bar = "█" * int(imp * 40)
        print(f"    {name:<20} {imp:.4f}  {bar}")

if __name__ == "__main__":
    evalute()