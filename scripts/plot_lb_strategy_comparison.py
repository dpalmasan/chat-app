#!/usr/bin/env python3
"""Create comparison charts across load-balancing strategies."""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from datetime import datetime
from pathlib import Path

try:
    import matplotlib.pyplot as plt
except ImportError as exc:  # pragma: no cover
    raise SystemExit("Missing dependency: matplotlib. Install with: pip install matplotlib") from exc


def parse_ts(raw: str) -> datetime:
    return datetime.fromisoformat(raw.replace("Z", "+00:00"))


def latest_pod_counts(csv_path: Path) -> dict[str, int]:
    grouped: dict[str, list[tuple[datetime, int]]] = defaultdict(list)
    with csv_path.open("r", encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle):
            grouped[row["pod"]].append((parse_ts(row["timestamp"]), int(row["handshake_count"])))

    out: dict[str, int] = {}
    for pod, series in grouped.items():
        series.sort(key=lambda item: item[0])
        out[pod] = series[-1][1]
    return out


def latest_db_counts(csv_path: Path) -> tuple[int, int]:
    latest: tuple[datetime, int, int] | None = None
    with csv_path.open("r", encoding="utf-8", newline="") as handle:
        for row in csv.DictReader(handle):
            ts = parse_ts(row["timestamp"])
            msg = int(row["messages_count"])
            users = int(row["users_count"])
            if latest is None or ts > latest[0]:
                latest = (ts, msg, users)

    if latest is None:
        return (0, 0)
    return (latest[1], latest[2])


def strategy_dirs(root_dir: Path) -> list[Path]:
    out = []
    for child in sorted(root_dir.iterdir()):
        if not child.is_dir():
            continue
        if (child / "pod_touch_samples.csv").exists() and (child / "db_population_samples.csv").exists():
            out.append(child)
    return out


def plot_pod_share(root_dir: Path, dirs: list[Path]) -> Path:
    output = root_dir / "strategy_pod_touch_comparison.png"

    strategy_names = [d.name for d in dirs]
    counts_per_strategy = [latest_pod_counts(d / "pod_touch_samples.csv") for d in dirs]
    all_pods = sorted({pod for c in counts_per_strategy for pod in c.keys()})

    x = list(range(len(strategy_names)))
    bottoms = [0] * len(strategy_names)

    fig, ax = plt.subplots(figsize=(11, 6))

    for pod in all_pods:
        values = [counts.get(pod, 0) for counts in counts_per_strategy]
        ax.bar(x, values, bottom=bottoms, label=pod)
        bottoms = [b + v for b, v in zip(bottoms, values)]

    ax.set_title("Pod Touch Counts by Load-Balancing Strategy")
    ax.set_xlabel("Strategy")
    ax.set_ylabel("Cumulative ws touches")
    ax.set_xticks(x)
    ax.set_xticklabels(strategy_names, rotation=20, ha="right")
    ax.grid(axis="y", alpha=0.25)
    ax.legend(loc="upper right")
    fig.tight_layout()
    fig.savefig(output, dpi=140)
    plt.close(fig)
    return output


def plot_db_growth(root_dir: Path, dirs: list[Path]) -> Path:
    output = root_dir / "strategy_db_growth_comparison.png"

    strategy_names = [d.name for d in dirs]
    values = [latest_db_counts(d / "db_population_samples.csv")[0] for d in dirs]

    x = list(range(len(strategy_names)))
    fig, ax = plt.subplots(figsize=(11, 5))
    ax.bar(x, values)
    ax.set_title("Final messages_count by Strategy")
    ax.set_xlabel("Strategy")
    ax.set_ylabel("messages_count")
    ax.set_xticks(x)
    ax.set_xticklabels(strategy_names, rotation=20, ha="right")
    ax.grid(axis="y", alpha=0.25)
    fig.tight_layout()
    fig.savefig(output, dpi=140)
    plt.close(fig)
    return output


def main() -> int:
    parser = argparse.ArgumentParser(description="Compare load-balancing strategies")
    parser.add_argument("--root-dir", required=True, help="Root directory containing strategy subdirectories")
    args = parser.parse_args()

    root_dir = Path(args.root_dir).resolve()
    if not root_dir.exists():
        raise SystemExit(f"Directory not found: {root_dir}")

    dirs = strategy_dirs(root_dir)
    if len(dirs) < 2:
        raise SystemExit("Need at least two strategy result directories to compare")

    out1 = plot_pod_share(root_dir, dirs)
    out2 = plot_db_growth(root_dir, dirs)
    print(f"Wrote: {out1}")
    print(f"Wrote: {out2}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
