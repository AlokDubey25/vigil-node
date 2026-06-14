import math
from typing import Tuple, List

# It is to match order in FeatureVectir::toVector in cpp
FEATURE_NAMES: List[str] = [
    "velocity",
    "priceDeviation",
    "cancelRate",
    "sizeRatio",
    "timeBetween",
    "repeatPriceRate",
]

# it to match cpp caps in feature_extractor.cpp
FEATURE_RANGES = {
    "velocity":         (0.0, 100.0),
    "priceDeviation":   (0.0,   1.0),
    "cancelRate":       (0.0,   1.0),
    "sizeRatio":        (0.0,  20.0),
    "timeBetween":      (0.0, 300.0),
    "repeatPriceRate":  (0.0,   1.0),
}

def validate_features(features: list) -> Tuple[bool, str]:
    """
    Main Idea here is like - 
    We'll check a feature vector before passing to the ML model.
    Returns (True, "") on success.
    Returns (False, reason) on failure.
    """

    # 1 - must be list
    if not isinstance(features, list):
        return False, f"expected list, got {type(features)._name_}"
    
    # 2 - must have 6 values
    if len(features) != 6:
        return False, f"expected 6 features, got {len(features)}"
    
    # 3 - check every value
    for i, val in enumerate(features):
        name = FEATURE_NAMES[i]

        # must be a number 
        if isinstance(val, bool) or not isinstance(val, (int, float)):
            return False, f"{name} must be a number, got {type(val).__name__}"
        
        # must not be NaN or infinite
        if math.isnan(val) or math.isinf(val):
            return False, f"{name} is NaN or infinite"
        
        # must be within valid range
        low, high = FEATURE_RANGES[name]
        if val < low or val > high:
            return False, f"{name}={val:.4f} outside range [{low}, {high}]"
        
    return True, ""  # for all good