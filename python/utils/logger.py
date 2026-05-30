import logging, os

def get_logger(name: str) -> logging.Logger:
    os.makedirs("lobgs", exist_ok=True)
    logger = logging.getLogger(name)

    if logger.handlers:
        return logger
    
    logger.setLevel(logging.DEBUG)
    fmt = logging.Formatter(
        "%(asctime)s | %(name)s | %(levelname)s | %(message)s",
        datefmt = "%H:%M%S"
    )

    fh = logging.FileHandler(f"logs/{name}.log")
    fh.setFormatter(fmt)
    sh = logging.StreamHandler()
    sh.setFormatter(fmt)
    logger.addHandler(fh)
    logger.addHandler(sh)

    return logger