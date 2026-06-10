import sys, os, math, pytest 

sys.path.insert(0, os.path.join(os.path.dirname(__file__),
                                    "../.."))

from python.utils.validators import validate_features

GOOD = [2.0, 0.02, 0.0, 1.5, 30.0, 0.1]         # all features with valid range

# valid input
def test_valid_vector_passes():
    ok, msg = validate_features(GOOD)
    assert ok == True
    assert msg == ""

def test_all_zeros_passes():
    # first order from a new user returns zeros - must be valid
    ok, msg = validate_features([0.0, 0.0, 0.0, 1.0, 0.0, 0.0])
    assert ok == True

def test_max_values_pass():
    # values at cap limits should still be valid
    ok, _ = validate_features([100.0, 1.0, 1.0, 20.0, 300.0, 1.0])
    assert ok == True

# wrong input type
def test_not_a_list_fails():
    ok, msg = validate_features("not a list")
    assert ok == False
    assert "expected list" in msg

# wrong length
def test_too_few_features_fails():
    ok, msg = validate_features([1.0, 2.0, 3.0])
    assert ok == False
    assert "6" in msg

def test_too_many_features_fails():
    ok, msg = validate_features([1.0] * 7)  # 7 features
    assert ok == False
    assert "6" in msg

def test_empty_list_fails():
    ok, msg = validate_features([])
    assert ok == False

# NaN and infinity
def test_nan_velocity_fails():
    bad = GOOD.copy()
    bad[0] = math.nan       # vel = NaN
    ok, msg = validate_features(bad)
    assert ok == False
    assert "velocity" in msg

def test_infinity_fails():
    bad = GOOD.copy()
    bad[3] = math.inf       # sizeRatio = infinity
    ok, msg = validate_features(bad)
    assert ok == False
    assert "sizeRatio" in msg

# out of range values
def test_velocity_over_cap_fails():
    bad = GOOD.copy()
    bad[0] = 101.0          # vel > 100 cap
    ok, msg = validate_features(bad)
    assert ok == False
    assert "velocity" in msg

def test_negative_value_fails():
    bad = GOOD.copy()
    bad[2] = -0.1           # cancelRate can't be neg
    ok, msg = validate_features(bad)
    assert ok == False
    assert "cancelRate" in msg

def test_size_ratio_over_cap_fails():
    bad = GOOD.copy()
    bad[3] = 20.5           # sizeRatio > 20 cap
    ok, msg = validate_features(bad)
    assert ok == False