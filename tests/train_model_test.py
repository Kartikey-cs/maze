import pathlib
import os
import shlex
import subprocess
import tempfile
import unittest

from tools import train_model as train

ROOT = pathlib.Path(__file__).resolve().parents[1]


class TrainingTests(unittest.TestCase):
    def test_seed_groups_do_not_leak(self):
        rows = [{"seed": s, "difficulty": d} for s in range(1, 11) for d in range(3)]
        training, testing = train.split_seeds(rows)
        self.assertEqual({r["seed"] for r in testing}, {5, 10})
        self.assertFalse({r["seed"] for r in training} & {r["seed"] for r in testing})

    def test_constant_and_duplicate_features_are_stable(self):
        x = [[i, i, 3] for i in range(10)]
        y = [i / 10 for i in range(10)]
        model = train.fit(x, y)
        self.assertLess(train.mse(model, x, y), 1e-6)
        self.assertEqual(train.predict(model, [-100, -100, 3]), 0)
        self.assertEqual(train.predict(model, [100, 100, 3]), 1)

    def test_exported_c_matches_python(self):
        # Nonconstant fractional features and a constant column cover scaling.
        x = [[i, i+1, i%3, 2, i%2, 2*i, 1+i/3] for i in range(20)]
        model = train.fit(x, [i / 20 for i in range(20)])
        with tempfile.TemporaryDirectory() as directory:
            header = pathlib.Path(directory) / "maze_model.h"
            train.export_header(model, header)
            binary = pathlib.Path(directory) / "score"
            subprocess.run(shlex.split(os.environ.get("CC", "cc")) + ["-Wall", "-Wextra", "-Werror", "-std=c11", "-I", directory,
                            str(ROOT / "src/maze_model_score.c"), "-o", str(binary)], check=True)
            probes = x + [[0]*7, [1000]*7]
            output = subprocess.check_output([str(binary)], input="".join(
                " ".join(map(str, row)) + "\n" for row in probes), text=True)
            for row, result in zip(probes, output.splitlines(), strict=True):
                self.assertAlmostEqual(float(result), train.predict(model, row), places=12)

    def test_dataset_repeats_and_labels_match_trials(self):
        command = [str(ROOT / "build/maze_dataset"), "10", "8", "1"]
        first = subprocess.check_output(command)
        self.assertEqual(first, subprocess.check_output(command))
        with tempfile.TemporaryDirectory() as directory:
            dataset = pathlib.Path(directory) / "data.csv"
            dataset.write_bytes(first)
            rows = train.load_rows(dataset)
            self.assertEqual(len(rows), 30)
            training, testing = train.split_seeds(rows)
            self.assertEqual(len(training), 24)
            self.assertEqual(len(testing), 6)
            # Corrupted labels must never silently enter training.
            dataset.write_text(first.decode().replace(
                first.decode().splitlines()[1], first.decode().splitlines()[1].rsplit(",", 1)[0] + ",2"))
            with self.assertRaises(ValueError):
                train.load_rows(dataset)

    def test_invalid_dataset_arguments(self):
        for args in (["0"], ["2", "0"], ["2", "8", "4294967295"], ["-1"]):
            result = subprocess.run([str(ROOT / "build/maze_dataset"), *args], capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(result.stdout, b"")

    def test_split_requires_train_and_test_seeds(self):
        with self.assertRaises(ValueError):
            train.split_seeds([{"seed": 1}])


if __name__ == "__main__":
    unittest.main()
