#!/usr/bin/env python3
"""Create animated chart(s) from load test CSV samples.

Outputs:
- pod_touch_evolution.gif/mp4
- db_population_evolution.gif/mp4
"""

from __future__ import annotations

import argparse
import csv
from collections import defaultdict
from datetime import datetime
from pathlib import Path

try:
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation
except ImportError as exc:  # pragma: no cover
    raise SystemExit("Missing dependency: matplotlib. Install with: pip install matplotlib") from exc


def parse_ts(raw: str) -> datetime:
    return datetime.fromisoformat(raw.replace("Z", "+00:00"))


def load_pod_data(csv_path: Path):
    series: dict[str, list[tuple[datetime, int]]] = defaultdict(list)
    with csv_path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            series[row["pod"]].append((parse_ts(row["timestamp"]), int(row["handshake_count"])))

    for pod in series:
        series[pod].sort(key=lambda item: item[0])

    return dict(sorted(series.items()))


def load_db_data(csv_path: Path):
    times: list[datetime] = []
    messages: list[int] = []
    users: list[int] = []

    with csv_path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            times.append(parse_ts(row["timestamp"]))
            messages.append(int(row["messages_count"]))
            users.append(int(row["users_count"]))

    return times, messages, users


def save_animation(anim: FuncAnimation, output_path: Path, fps: int):
    suffix = output_path.suffix.lower()
    if suffix == ".gif":
        anim.save(str(output_path), writer="pillow", fps=fps)
        return
    if suffix == ".mp4":
        # Requires ffmpeg available in PATH.
        anim.save(str(output_path), writer="ffmpeg", fps=fps)
        return
    raise SystemExit(f"Unsupported output format: {suffix}. Use .gif or .mp4")


def animate_pod_series(input_dir: Path, output_path: Path, fps: int):
    data = load_pod_data(input_dir / "pod_touch_samples.csv")
    if not data:
        raise SystemExit("No pod touch data found")

    max_len = max(len(v) for v in data.values())
    max_y = max((y for series in data.values() for _, y in series), default=1)

    fig, ax = plt.subplots(figsize=(11, 5))
    lines = {}
    for pod in data:
        line, = ax.plot([], [], marker="o", linewidth=1.8, markersize=3.5, label=pod)
        lines[pod] = line

    ax.set_title("Chat-Service Pod Touches (Animated)")
    ax.set_xlabel("Time")
    ax.set_ylabel("Handshake Count")
    ax.grid(alpha=0.25)
    ax.legend(loc="upper left")
    ax.set_ylim(0, max(2, int(max_y * 1.1)))

    def init():
        for line in lines.values():
            line.set_data([], [])
        return list(lines.values())

    def update(frame_idx: int):
        min_x = None
        max_x = None
        for pod, series in data.items():
            sub = series[: frame_idx + 1]
            if not sub:
                continue
            xs = [t for t, _ in sub]
            ys = [v for _, v in sub]
            lines[pod].set_data(xs, ys)
            if min_x is None or xs[0] < min_x:
                min_x = xs[0]
            if max_x is None or xs[-1] > max_x:
                max_x = xs[-1]

        if min_x is not None and max_x is not None:
            ax.set_xlim(min_x, max_x)

        return list(lines.values())

    anim = FuncAnimation(
        fig,
        update,
        init_func=init,
        frames=max_len,
        interval=int(1000 / max(1, fps)),
        blit=False,
        repeat=False,
    )

    fig.autofmt_xdate()
    fig.tight_layout()
    save_animation(anim, output_path, fps)
    plt.close(fig)


def animate_db_series(input_dir: Path, output_path: Path, fps: int):
    times, messages, users = load_db_data(input_dir / "db_population_samples.csv")
    if not times:
        raise SystemExit("No db population data found")

    fig, ax = plt.subplots(figsize=(11, 5))
    (line_messages,) = ax.plot([], [], marker="o", linewidth=1.8, markersize=3.5, label="messages_count")
    (line_users,) = ax.plot([], [], marker="s", linewidth=1.5, markersize=3.0, label="users_count")

    ax.set_title("MongoDB Population (Animated)")
    ax.set_xlabel("Time")
    ax.set_ylabel("Document Count")
    ax.grid(alpha=0.25)
    ax.legend(loc="upper left")

    max_y = max(max(messages), max(users), 1)
    ax.set_ylim(0, max(2, int(max_y * 1.1)))

    def init():
        line_messages.set_data([], [])
        line_users.set_data([], [])
        return [line_messages, line_users]

    def update(frame_idx: int):
        sub_times = times[: frame_idx + 1]
        line_messages.set_data(sub_times, messages[: frame_idx + 1])
        line_users.set_data(sub_times, users[: frame_idx + 1])
        ax.set_xlim(times[0], times[min(frame_idx, len(times) - 1)])
        return [line_messages, line_users]

    anim = FuncAnimation(
        fig,
        update,
        init_func=init,
        frames=len(times),
        interval=int(1000 / max(1, fps)),
        blit=False,
        repeat=False,
    )

    fig.autofmt_xdate()
    fig.tight_layout()
    save_animation(anim, output_path, fps)
    plt.close(fig)


def main() -> int:
    parser = argparse.ArgumentParser(description="Animate load-test charts")
    parser.add_argument("--input-dir", required=True, help="Directory with load-test CSV samples")
    parser.add_argument(
        "--format",
        choices=["gif", "mp4"],
        default="gif",
        help="Animation output format",
    )
    parser.add_argument("--fps", type=int, default=4, help="Frames per second for animation")
    args = parser.parse_args()

    input_dir = Path(args.input_dir).resolve()
    if not input_dir.exists():
        raise SystemExit(f"Input directory not found: {input_dir}")

    pod_out = input_dir / f"pod_touch_evolution.{args.format}"
    db_out = input_dir / f"db_population_evolution.{args.format}"

    animate_pod_series(input_dir, pod_out, args.fps)
    animate_db_series(input_dir, db_out, args.fps)

    print(f"Wrote: {pod_out}")
    print(f"Wrote: {db_out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
