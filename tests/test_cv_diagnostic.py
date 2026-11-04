import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from cv_diagnostic import FIELDS, LEVELS, report, scene
from scene_tool import decode


class DiagnosticTest(unittest.TestCase):
    def test_every_output_and_level(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "in.mcvs"
            result = Path(directory) / "out.mcvs"
            for channel in range(1, 9):
                for volts in LEVELS:
                    binary = scene(channel, volts)
                    decoded = decode(binary)
                    self.assertEqual(decoded["enabled_mask"], 1 << (channel - 1))
                    for index, route in enumerate(decoded["routes"], 1):
                        if index == channel:
                            self.assertEqual((route["source"], route["scale"],
                                              route["offset_volts"], route["maximum_volts"]),
                                             (0, 0, volts, 3))
                        else:
                            self.assertIsNone(route["source"])
                    source.write_bytes(binary)
                    subprocess.run(["build/scene_io_test", source, result], check=True)
                    self.assertEqual(result.read_bytes(), binary)
        with self.assertRaises(ValueError):
            scene(1, 4)

    def test_measurement_errors(self):
        header = ",".join(FIELDS) + "\n"
        results = report(io.StringIO(header + "hold,1,2,1,1.987\nhold,1,2,2,0.002\nstop,1,2,1,-0.001\n"))
        self.assertEqual([row[-1] for row in results], [-13, 2, -1])
        self.assertEqual(report(io.StringIO(header)), [])
        with self.assertRaises(ValueError):
            report(io.StringIO(header + "close,1,3,1,NaN\n"))
