"""Compare the exported C prediction with Python on every dataset row."""
import argparse
import json
import math
import subprocess

from train_model import FEATURES, load_rows, predict


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset")
    parser.add_argument("model")
    parser.add_argument("scorer")
    args = parser.parse_args()
    rows = load_rows(args.dataset)
    with open(args.model) as source:
        model = json.load(source)
    if model["features"] != FEATURES:
        raise ValueError("Model feature order mismatch")
    vectors = [[row[name] for name in FEATURES] for row in rows]
    # Exercise both clipping boundaries as well as the measured mazes.
    vectors += [[0.0]*len(FEATURES), [1e6]*len(FEATURES)]
    text = "".join(" ".join(map(str, row)) + "\n" for row in vectors)
    output = subprocess.check_output([args.scorer], input=text, text=True)
    scores = list(map(float, output.splitlines()))
    for row, actual in zip(vectors, scores, strict=True):
        expected = predict(model, row)
        if not math.isclose(actual, expected, rel_tol=1e-12, abs_tol=1e-12):
            raise AssertionError(f"C/Python score mismatch: {actual} vs {expected}")
    print(f"C/Python predictions match on {len(vectors)} vectors (tolerance 1e-12).")


if __name__ == "__main__":
    main()
