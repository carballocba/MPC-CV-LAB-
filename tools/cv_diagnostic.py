#!/usr/bin/env python3
"""Build a single-output fixed DC scene or summarize recorded meter readings."""
import argparse
import csv
from decimal import Decimal, InvalidOperation
import sys

from scene_tool import atomic_write, encode

LEVELS = (0, 1, 2, 3)
PHASES = {"hold", "stop", "close"}
FIELDS = ("phase", "active_channel", "setpoint_v", "measured_channel", "measured_v")


def scene(channel, volts):
    if channel not in range(1, 9) or volts not in LEVELS:
        raise ValueError("expected channel 1–8 and setpoint 0, 1, 2 or 3 V")
    routes = [{} for _ in range(8)]
    routes[channel - 1] = {
        "source": 0, "scale": 0, "offset_volts": volts,
        "minimum_volts": 0, "maximum_volts": 3,
    }
    return encode({
        "schema_version": 1, "kind": "performance_scene",
        "enabled_mask": 1 << (channel - 1), "nodes": [{"type": "off"}],
        "routes": routes,
    })


def report(stream):
    reader = csv.DictReader(stream)
    if tuple(reader.fieldnames or ()) != FIELDS:
        raise ValueError("expected columns " + ",".join(FIELDS))
    rows = []
    seen = set()
    for row in reader:
        phase = row["phase"]
        active = int(row["active_channel"])
        setpoint = int(row["setpoint_v"])
        measured = int(row["measured_channel"])
        reading = Decimal(row["measured_v"])
        if (None in row or phase not in PHASES or active not in range(1, 9)
                or setpoint not in LEVELS or measured not in range(1, 9)
                or not reading.is_finite() or abs(reading) > 100):
            raise ValueError(f"invalid measurement at line {reader.line_num}")
        key = (phase, active, setpoint, measured)
        if key in seen:
            raise ValueError(f"duplicate measurement at line {reader.line_num}")
        seen.add(key)
        expected = setpoint if phase == "hold" and active == measured else 0
        rows.append((*key, reading, expected, (reading - expected) * 1000))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    build = commands.add_parser("scene")
    build.add_argument("channel", type=int, choices=range(1, 9))
    build.add_argument("volts", type=int, choices=LEVELS)
    build.add_argument("output")
    readings = commands.add_parser("report")
    readings.add_argument("input")
    args = parser.parse_args()
    try:
        if args.command == "scene":
            atomic_write(args.output, scene(args.channel, args.volts))
        else:
            with open(args.input, newline="", encoding="utf-8") as stream:
                rows = report(stream)
            writer = csv.writer(sys.stdout)
            writer.writerow((*FIELDS, "expected_v", "error_mv"))
            writer.writerows(rows)
    except (OSError, ValueError, TypeError, InvalidOperation) as error:
        parser.exit(2, f"{error}\n")


if __name__ == "__main__":
    main()
