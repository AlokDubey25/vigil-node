import os, sys
import pandas as pd
import joblib
from sklearn.ensemble import RandomForestClassifier
from sklearn.model_selection import train_test_split
from sklearn.metrics import classification_report, f1_score


ROOT       = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
DATA_PATH  = os.path.join(ROOT, "data/training_data/processed/features_train.csv")
MODEL_PATH = os.path.join(ROOT, "models/saved/fraud_model_rf.pkl")

FEATURES = ["velocity", "priceDeviation", "cancelRate", "sizeRatio", "timeBetween", "repeatPriceRate",]

def train() -> float:
    if not os.path.exists(DATA_PATH):
        print(f"[ERROR] CSV not found: {DATA_PATH}")
        print(" RUN: python data/training_data/simulate_data.py")
        sys.exit(1)

    df = pd.read_csv(DATA_PATH)
    X, y = df[FEATURES], df["is_fraud"]

    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)
    print(f"Loaded {len(df)} rows - training Random Forest...")

    model = RandomForestClassifier(
        n_estimators=100,
        max_depth=10,
        max_features = "sqrt",
        class_weight="balanced",
        random_state=42,
        n_jobs=-1,
    )

    model.fit(X_train, y_train)
    y_pred = model.predict(X_test)
    f1     = f1_score(y_test, y_pred)

    print(classification_report(y_test, y_pred, target_names=["normal", "fraud"]))
    print(f"F1 Score (fraud): {f1:.4f}")

    os.makedirs(os.path.dirname(MODEL_PATH), exist_ok=True)
    joblib.dump(model, MODEL_PATH)
    print(f"Model saved -> {MODEL_PATH}")
    return f1

if __name__ == "__main__":
    train() 

# resolved issue