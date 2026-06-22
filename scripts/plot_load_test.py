#!/usr/bin/env python3
"""Render PNG charts from load test CSV samples."""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from datetime import datetime
from pathlib import Path

try:
    import matplotlib.pyplot as plt
except ImportError as exc:  # pragma: no cover - runtime dependency guard
    raise SystemExit(
        "Missing dependency: matplotlib. Install with: pip install matplotlib"
    ) from exc


def parse_ts(raw: str) -> datetime:
    return datetime.fromisoformat(raw.replace("Z", "+00:00"))


def plot_pod_samples(input_dir: Path) -> Path:
    csv_path = input_dir / "pod_touch_samples.csv"
    output = input_dir / "pod_touch_chart.png"

    points: dict[str, list[tuple[datetime, int]]] = defaultdict(list)
    with csv_path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            points[row["pod"]].append((parse_ts(row["timestamp"]), int(row["handshake_count"])))

    if not points:
        raise SystemExit(f"No pod data found in {csv_path}")

    fig, ax = plt.subplots(figsize=(11, 5))
    for pod, series in sorted(points.items()):
        series.sort(key=lambda item: item[0])
        xs = [item[0] for item in series]
        ys = [item[1] for item in series]
        ax.plot(xs, ys, marker="o", linewidth=1.6, markersize=3.5, label=pod)

    ax.set_title("Chat-Service Pod Touches (Cumulative Handshakes)")
    ax.set_xlabel("Time")
    ax.set_ylabel("Handshake Count")
    ax.legend(loc="upper left")
    ax.grid(alpha=0.25)
    fig.autofmt_xdate()
    fig.tight_layout()
    fig.savefig(output, dpi=140)
    plt.close(fig)
    return output


def plot_db_samples(input_dir: Path) -> Path:
    csv_path = input_dir / "db_population_samples.csv"
    output = input_dir / "db_population_chart.png"

    times: list[datetime] = []
    messages: list[int] = []
    users: list[int] = []

    with csv_path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            times.append(parse_ts(row["timestamp"]))
            messages.append(int(row["messages_count"]))
            users.append(int(row["users_count"]))

    if not times:
        raise SystemExit(f"No db data found in {csv_path}")

    fig, ax = plt.subplots(figsize=(11, 5))
    ax.plot(times, messages, marker="o", linewidth=1.8, markersize=3.5, label="messages_count")
    ax.plot(times, users, marker="s", linewidth=1.3, markersize=3.0, label="users_count")
    ax.set_title("MongoDB Population Over Time")
    ax.set_xlabel("Time")
    ax.set_ylabel("Document Count")
    ax.legend(loc="upper left")
    ax.grid(alpha=0.25)
    fig.autofmt_xdate()
    fig.tight_layout()
    fig.savefig(output, dpi=140)
    plt.close(fig)
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description="Plot load-test metrics")
    parser.add_argument("--input-dir", required=True, help="Directory with CSV samples")
    args = parser.parse_args()

    input_dir = Path(args.input_dir).resolve()
    if not input_dir.exists():
        raise SystemExit(f"Input directory not found: {input_dir}")

    pod_chart = plot_pod_samples(input_dir)
    db_chart = plot_db_samples(input_dir)

    print(f"Wrote: {pod_chart}")
    print(f"Wrote: {db_chart}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
