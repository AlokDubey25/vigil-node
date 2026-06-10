import numpy as np
import pandas as pd
import os

np.random.seed(42)

N_NORMAL = 4000
N_FRAUD  = 400        # here we took 10% fraud rate 


# Every freking thing is normal 
def noraml_order() -> dict:
    return{
    "velocity":         np.random.exponential(2.0),
    "priceDeviation":   np.random.exponential(0.01),
    "cancelRate":       np.random.beta(1, 10),
    "sizeRatio":        np.random.lognormal(0, 0.3),
    "timeBetween":      np.random.gamma(2, 30),
    "repeatPriceRate":  np.random.beta(1, 5),
    "is_fraud": 0,
    }

# bots: very high vel and very small time b/w orders
def bot_order() -> dict:
    return{
    "velocity":         np.random.uniform(30, 100),
    "priceDeviation":   np.random.exponential(0.02),
    "cancelRate":       np.random.beta(2, 5),
    "sizeRatio":        np.random.lognormal(0.5, 0.3),
    "timeBetween":      np.random.uniform(0, 2),
    "repeatPriceRate":  np.random.beta(3, 2),
    "is_fraud": 1,
    }

# spoofing: far from mid-price, high cancel rate, repeat same level
def spoof_order() -> dict:
    return{
    "velocity":         np.random.uniform(10, 50),
    "priceDeviation":   np.random.uniform(0.08, 0.30),
    "cancelRate":       np.random.uniform(0.60, 0.99),
    "sizeRatio":        np.random.uniform(3, 15),
    "timeBetween":      np.random.uniform(0.5, 5),
    "repeatPriceRate":  np.random.uniform(0.60, 0.99),
    "is_fraud": 1,
    }

# wash trading: large size ratio, moderate velocity, repeat price
def wash_order() -> dict:
    return{
    "velocity":         np.random.uniform(5, 30),
    "priceDeviation":   np.random.exponential(0.01),
    "cancelRate":       np.random.beta(1, 8),
    "sizeRatio":        np.random.uniform(5, 20),
    "timeBetween":      np.random.uniform(0.1, 10),
    "repeatPriceRate":  np.random.uniform(0.60, 1.0),
    "is_fraud": 1,
    }

# generate all records
fraud_generators = [bot_order() for _ in range(N_NORMAL)]

records = [noraml_order() for _ in range(N_NORMAL)]

# cycle through fraud patterns evenly
for i in range(N_FRAUD):
    records.append(fraud_generators[i % 3]())

# shuffle so fraud isn't all at end 
np.random.shuffle(records)

df = pd.DataFrame(records)

# clip to match cpp caps like it much be identical to features_ext .cpp
df["velocity"]          = df["velocity"].clip(0, 100)
df["priceDeviation"]    = df["priceDeviation"].clip(0, 1)
df["cancelRate"]        = df["cancelRate"].clip(0, 1)
df["sizeRatio"]         = df["sizeRatio"].clip(0, 20)
df["timeBetween"]       = df["timeBetween"].clip(0, 300)
df["repeatPriceRate"]   = df["repeatPriceRate"].clip(0, 1)

# save
output_dir  = "data/training_data/processed"
output_path = f"{output_dir}/features_train.csv"

os.makedirs(output_dir, exist_ok=True)
df.to_csv(output_path, index=False)

# report
total  = len(df)
fraud  = int(df["is_fraud"].sum())
normal = total - fraud

print(f"Generated {total} records: {normal} normal, {fraud} fraud")
print(f"Fraud rate: {fraud/total*100:.1f}%")
print(f"Saved to:   {output_path}")
print()
print("Feature stats (fraud vs normal):")
features = ["velocity", "priceDeviation", "cancelRate", "sizeRatio", "timeBetween", "repeatPriceRate"]

for f in features:
    norm_mean  = df[df.is_fraud == 0][f].mean()
    fraud_mean = df[df.is_fraud == 1][f].mean()
    print(f"    {f:<20} normal={norm_mean:.3f}  fraud={fraud_mean:.3f}")