"""Plots for the CSV files written by run_sim and sweep.

  plot.py path  PATH.csv [PATH.csv ...] [--labels NAME ...] [--out FILE]
      Mid with the maker's bid and ask, and inventory, over time. Several files
      are drawn side by side on shared axes, which is meaningful when they come
      from the same seed: the mid is then the same in every panel.

  plot.py dist  SEEDS.csv [SEEDS.csv ...] [--at PARAM=VALUE ...] [--out FILE]
      Distribution of final P&L across seeds, one panel per file. A file that
      holds a whole sweep needs --at to say which grid value to show.

  plot.py sweep SWEEP.csv [--out FILE]
      Mean final P&L and mean maximum absolute inventory against the swept
      parameter. A grid written by sweep --log gets a logarithmic x-axis.

  plot.py inventory DIR [DIR ...] [--out FILE]
      Mean absolute inventory against the fraction of the run elapsed, one line
      per directory. Each directory holds the step logs of one strategy, one
      file per seed, and its name labels the line.

A strategy keeps the same colour in every figure.
"""

import argparse
import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")  # write files without needing a display
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.ticker import FuncFormatter, LogLocator

SURFACE = "#fcfcfb"
INK = "#0b0b0b"
INK_SECONDARY = "#52514e"
INK_MUTED = "#898781"
GRID = "#e1e0d9"
AXIS = "#c3c2b7"
STRATEGY_COLOURS = {"symmetric": "#2a78d6", "skew": "#eb6834", "as": "#1baf7a"}
DEFAULT_COLOUR = STRATEGY_COLOURS["symmetric"]
SPARE_COLOURS = ["#eda100", "#e87ba4", "#008300", "#4a3aa7", "#e34948"]
TIME_SLICES = 100  # resolution of the inventory profile along the run

plt.rcParams.update({
    "figure.facecolor": SURFACE,
    "axes.facecolor": SURFACE,
    "savefig.facecolor": SURFACE,
    "savefig.dpi": 150,
    "font.family": "sans-serif",
    "font.sans-serif": ["Segoe UI", "Helvetica Neue", "Arial", "DejaVu Sans"],
    "font.size": 10,
    "text.color": INK,
    "axes.titlesize": 11,
    "axes.titleweight": "semibold",
    "axes.titlelocation": "left",
    "axes.labelcolor": INK_SECONDARY,
    "axes.edgecolor": AXIS,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.grid": True,
    "axes.axisbelow": True,
    "grid.color": GRID,
    "grid.linewidth": 0.8,
    "xtick.color": AXIS,
    "ytick.color": AXIS,
    "xtick.labelcolor": INK_MUTED,
    "ytick.labelcolor": INK_MUTED,
    "lines.linewidth": 1.5,
    "legend.frameon": False,
})


def save(fig, out):
    fig.savefig(out)
    print(f"wrote {out}")


def read_numeric(path, names):
    """Returns the named columns of an all-numeric CSV as arrays."""
    with open(path, newline="") as file:
        header = next(csv.reader(file))
    columns = [header.index(name) for name in names]
    return np.loadtxt(path, delimiter=",", skiprows=1, usecols=columns, unpack=True, ndmin=2)


def read_rows(path):
    with open(path, newline="") as file:
        rows = list(csv.DictReader(file))
    if not rows:
        raise SystemExit(f"{path}: no data rows")
    return rows


def plot_path(args):
    labels = args.labels or [Path(path).stem for path in args.files]
    if len(labels) != len(args.files):
        raise SystemExit("--labels needs one name per file")
    fig, axes = plt.subplots(
        2, len(args.files), figsize=(4.6 * len(args.files) + 0.8, 5.6), squeeze=False,
        sharex=True, sharey="row", height_ratios=[3, 2], layout="constrained")
    for column, (path, label) in enumerate(zip(args.files, labels)):
        time, mid, bid, ask, inventory = read_numeric(
            path, ["time", "mid", "bid", "ask", "inventory"])
        colour = STRATEGY_COLOURS.get(label, DEFAULT_COLOUR)
        price_axis, inventory_axis = axes[:, column]

        price_axis.fill_between(time, bid, ask, color=colour, alpha=0.3, linewidth=0,
                                label="bid to ask")
        price_axis.plot(time, mid, color=INK, linewidth=0.9, label="mid")
        price_axis.set_title(label)

        inventory_axis.axhline(0, color=AXIS, linewidth=0.8)
        inventory_axis.plot(time, inventory, color=colour, linewidth=1.2, drawstyle="steps-post")
        inventory_axis.set_xlabel("time (s)")
        inventory_axis.margins(x=0)
    axes[0, 0].set_ylabel("price")
    axes[1, 0].set_ylabel("inventory (units)")
    axes[0, 0].legend(loc="best")
    save(fig, args.out)


def select(rows, path, picks):
    """Returns the rows of one grid value: the only one, or the one --at names."""
    param = rows[0]["param"]
    values = sorted({float(row["value"]) for row in rows})
    if param in picks:
        value = min(values, key=lambda candidate: abs(candidate - picks[param]))
        if not np.isclose(value, picks[param]):
            raise SystemExit(f"{path}: no rows with {param} = {picks[param]:g}; it has {values}")
    elif len(values) == 1:
        value = values[0]
    else:
        raise SystemExit(f"{path}: holds {len(values)} values of {param}; pick one with "
                         f"--at {param}=VALUE")
    return [row for row in rows if np.isclose(float(row["value"]), value)], value


def plot_dist(args):
    picks = {}
    for pick in args.at:
        name, _, value = pick.partition("=")
        picks[name] = float(value)

    panels = []
    for path in args.files:
        rows, value = select(read_rows(path), path, picks)
        pnl = np.array([float(row["final_pnl"]) for row in rows])
        panels.append((rows[0]["strategy"], f"{rows[0]['param']} = {value:g}", pnl))

    # Shared bins and axes, so width and position compare directly across panels.
    bins = np.histogram_bin_edges(np.concatenate([pnl for _, _, pnl in panels]), bins=40)
    fig, axes = plt.subplots(
        len(panels), 1, figsize=(7.2, 1.9 * len(panels) + 0.7), squeeze=False,
        sharex=True, sharey=True, layout="constrained")
    for axis, (strategy, setting, pnl) in zip(axes[:, 0], panels):
        colour = STRATEGY_COLOURS.get(strategy, DEFAULT_COLOUR)
        axis.hist(pnl, bins=bins, color=colour, edgecolor=SURFACE, linewidth=1)
        axis.axvline(pnl.mean(), color=INK, linewidth=1)
        axis.set_title(f"{strategy} ({setting})")
        axis.set_ylabel("runs")
        axis.grid(axis="x", visible=False)
        axis.text(0.995, 0.92, f"mean {pnl.mean():.1f}   std {pnl.std(ddof=1):.1f}   "
                  f"{len(pnl)} seeds", transform=axis.transAxes, ha="right", va="top",
                  color=INK_SECONDARY)
    axes[-1, 0].set_xlabel("final P&L (line marks the mean)")
    save(fig, args.out)


def is_geometric(values):
    """True for a grid with a constant ratio between neighbours, as sweep --log writes."""
    if len(values) < 3 or np.any(values <= 0):
        return False
    ratios = values[1:] / values[:-1]
    return bool(np.allclose(ratios, ratios[0], rtol=1e-6) and not np.isclose(ratios[0], 1))


def plot_sweep(args):
    rows = read_rows(args.file)
    strategy, param = rows[0]["strategy"], rows[0]["param"]
    value, pnl, pnl_std, inventory = (
        np.array([float(row[name]) for row in rows])
        for name in ["value", "final_pnl", "std_final_pnl", "max_abs_inventory"])
    colour = STRATEGY_COLOURS.get(strategy, DEFAULT_COLOUR)
    marker = dict(marker="o", markersize=6, markeredgecolor=SURFACE, markeredgewidth=1.2)

    # Two panels rather than two y-axes: P&L and inventory share no scale.
    fig, (pnl_axis, inventory_axis) = plt.subplots(
        2, 1, figsize=(7.2, 6.0), sharex=True, layout="constrained")
    pnl_axis.fill_between(value, pnl - pnl_std, pnl + pnl_std, color=colour, alpha=0.15,
                          linewidth=0)
    pnl_axis.plot(value, pnl, color=colour, **marker)
    pnl_axis.set_title(f"{strategy}: sweep of {param}, {rows[0]['seeds']} seeds per value")
    pnl_axis.set_ylabel("final P&L\nmean, band of ±1 std across seeds")

    inventory_axis.plot(value, inventory, color=colour, **marker)
    inventory_axis.set_ylim(bottom=0)
    inventory_axis.set_ylabel("max |inventory| in a run\nmean across seeds")
    inventory_axis.set_xlabel(param)
    if is_geometric(value):
        inventory_axis.set_xscale("log")
        inventory_axis.xaxis.set_major_formatter(FuncFormatter(lambda tick, _: f"{tick:g}"))
        inventory_axis.set_xlabel(f"{param} (log scale)")
    save(fig, args.out)


def inventory_profile(path):
    """Mean |inventory| over each of TIME_SLICES equal stretches of one step log."""
    (inventory,) = read_numeric(path, ["inventory"])
    if len(inventory) < TIME_SLICES:
        raise SystemExit(f"{path}: {len(inventory)} rows are too few for {TIME_SLICES} slices")
    # Rows are evenly spaced in time, so equal row counts are equal stretches of the run.
    edges = np.linspace(0, len(inventory), TIME_SLICES + 1).astype(int)
    return np.add.reduceat(np.abs(inventory), edges[:-1]) / np.diff(edges)


def plot_inventory(args):
    # Time is normalised because what Avellaneda-Stoikov does depends on the share
    # of the run that is left, and so that logs of different horizons can be mixed.
    elapsed = (np.arange(TIME_SLICES) + 0.5) / TIME_SLICES
    spare = iter(SPARE_COLOURS)  # for directories not named after a strategy
    fig, axis = plt.subplots(figsize=(7.2, 4.4), layout="constrained")
    for directory in args.directories:
        logs = sorted(Path(directory).glob("*.csv"))
        if not logs:
            raise SystemExit(f"{directory}: no .csv step logs")
        profile = np.mean([inventory_profile(log) for log in logs], axis=0)
        label = Path(directory).name
        colour = STRATEGY_COLOURS.get(label) or next(spare, INK_SECONDARY)
        axis.plot(elapsed, profile, color=colour, label=f"{label} ({len(logs)} runs)")
        axis.annotate(label, (elapsed[-1], profile[-1]), xytext=(6, 0),
                      textcoords="offset points", va="center", color=INK_SECONDARY)
    # A log axis because an uncontrolled position is tens of times a controlled one.
    axis.set_yscale("log")
    axis.yaxis.set_major_locator(LogLocator(subs=(1, 2, 5)))
    axis.yaxis.set_major_formatter(FuncFormatter(lambda tick, _: f"{tick:g}"))
    axis.set_title("Absolute inventory through the run")
    axis.set_xlabel("fraction of the run elapsed")
    axis.set_ylabel("mean |inventory| across runs (log scale)")
    axis.set_xlim(0, 1)
    axis.legend(loc="best")
    save(fig, args.out)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)

    path = commands.add_parser("path", help="mid, quotes and inventory over time")
    path.add_argument("files", nargs="+", metavar="PATH.csv")
    path.add_argument("--labels", nargs="+", metavar="NAME",
                      help="panel titles, one per file (default: the file names)")
    path.add_argument("--out", default="path.png")
    path.set_defaults(run=plot_path)

    dist = commands.add_parser("dist", help="final P&L across seeds")
    dist.add_argument("files", nargs="+", metavar="SEEDS.csv")
    dist.add_argument("--at", action="append", default=[], metavar="PARAM=VALUE",
                      help="grid value to show for files that swept PARAM")
    dist.add_argument("--out", default="pnl_distribution.png")
    dist.set_defaults(run=plot_dist)

    sweep = commands.add_parser("sweep", help="P&L and inventory against the swept parameter")
    sweep.add_argument("file", metavar="SWEEP.csv")
    sweep.add_argument("--out", default="sweep.png")
    sweep.set_defaults(run=plot_sweep)

    inventory = commands.add_parser("inventory", help="mean absolute inventory through the run")
    inventory.add_argument("directories", nargs="+", metavar="DIR",
                           help="step logs of one strategy, one .csv per seed")
    inventory.add_argument("--out", default="inventory_profile.png")
    inventory.set_defaults(run=plot_inventory)

    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
