import logging
from pathlib import Path

_ROOT = Path(__file__).resolve().parent.parent.parent
_LOGS = _ROOT / "logs"

def get_logger(name: str) -> logging.Logger:
    _LOGS.mkdir("lobgs", exist_ok=True)
    logger = logging.getLogger(name)

    if logger.handlers:
        return logger
    
    logger.setLevel(logging.DEBUG)
    fmt = logging.Formatter(
        "%(asctime)s | %(name)s | %(levelname)s | %(message)s",
        datefmt = "%H:%M%S"
    )

    fh = logging.FileHandler(_LOGS / f"{name}.log")
    fh.setLevel(logging.DEBUG)
    fh.setFormatter(fmt)


    sh = logging.StreamHandler()
    sh.setLevel(logging.INFO)
    sh.setFormatter(fmt)

    
    logger.addHandler(fh)
    logger.addHandler(sh)

    return logger