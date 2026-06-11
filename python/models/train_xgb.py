import os, sys
import pandas as pd
import joblib
from xgboost import XGBClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, f1_score

ROOT        = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
DATA_PATH   = os.path.join(ROOT, "data/training_data/processed/features_train.csv")
MODEL_PATH  = os.path.join(ROOT, "models/saved/fraud_model_xgb.pkl")

# these must match to featurevec::toVector()
FEATURES = ["velocity", "priceDeviation",   "cancelRate",
            "sizeRatio","timeBetween",  "repeatPriceRate", ]

def train() -> float:
    # 01 - LOAD DATA
    if not os.path.exists(DATA_PATH):
        print(f"[ERROR] CSV not found: {DATA_PATH}")
        print(" Run: python data/training_data/simulate_data.py")
        sys.exit(1)

    df = pd.read_csv(DATA_PATH)
    X = df[FEATURES]
    y = df["is_fraud"]

    n_fraud  = int(y.sum())
    n_normal = int((y == 0).sum())
    print(f"Loaded {len(df)} rows - {n_normal} normal, {n_fraud} fraud")

    # 02 - TRAIN/TEST Split 
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)
    print(f"Train: {len(X_train)} rows  Test: {len(X_test)} rows")

    # 03 - CLASS IMBALANCE Wt
    # scale_pos_wt = normal_count/fraud_count
    # tells XGBoost: missing one fraud case costs this many times more
    pos_weight = int((y_train == 0).sum() / (y_train == 1).sum())
    print(f"scale_pos_weight = {pos_weight}")

    # 04 - TRAIN
    model = XGBClassifier(
        n_estimators    = 200,
        max_depth       = 6,
        learning_rate   = 0.1,
        scale_pos_weight= pos_weight,
        eval_metric     = "logloss",
        random_state    = 42,
        verbosity        = 0,                            
    )

    print("Training...")
    model.fit(X_train, y_train)
    print("Done...")

    # 05 - EVALUATION
    y_pred = model.predict(X_test)
    f1     = f1_score(y_test, y_pred)

    print("\n CLASSIFICATION REPORT ")
    print(classification_report(y_test, y_pred, target_names = ["normal", "fraud"]))
    print(f"F1 Score (fraud class): {f1:.4f}")

    if f1 < 0.75:
        print("[WARN] F1 below 0.75 - check traiding data")
    else:
        print("[OK] F1 looks good")

    # 06 - SAVE MODEL
    os.makedirs(os.path.dirname(MODEL_PATH), exist_ok=True)
    joblib.dump(model, MODEL_PATH)
    print(f"\nModel saved -> {MODEL_PATH}")

    return f1

if __name__ == "__main__":
    train()