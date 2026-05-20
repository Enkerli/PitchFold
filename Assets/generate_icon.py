#!/usr/bin/env python3
"""
Generate PitchFold app icon (1024×1024 PNG).

Concept: 12-node chromatic wheel on warm paper, an active PCS (pentatonic
minor) shown as filled ink dots connected by an amber polygon.  The root
node (C) has an amber ring.  Typeset "PF" monogram in the centre.

Usage:
    python3 Assets/generate_icon.py

Requires matplotlib:
    pip install matplotlib
"""

import math, os, sys

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    import numpy as np
except ImportError:
    print("ERROR: matplotlib not found.  Install with: pip install matplotlib")
    sys.exit(1)

# ── Palette (matches plugin warm-paper theme) ─────────────────────────────────
BG    = "#F5F0E8"   # paper bg
CARD  = "#FAF8F4"   # card / node bg
INK   = "#2D2620"   # ink
INK50 = "#6B5E55"   # secondary text
AMBER = "#C4873A"   # root + active polygon
RULE  = "#D4CAB8"   # inactive node stroke

SIZE_PX = 1024
DPI     = 128       # figure = 8 × 8 inches

fig, ax = plt.subplots(figsize=(SIZE_PX / DPI, SIZE_PX / DPI), dpi=DPI)
fig.patch.set_facecolor(BG)
ax.set_facecolor(BG)
ax.set_xlim(-1, 1)
ax.set_ylim(-1, 1)
ax.set_aspect("equal")
ax.axis("off")
plt.subplots_adjust(left=0, right=1, top=1, bottom=0)

R = 0.70    # wheel radius
r = 0.085   # node radius

# ── Active PCS: pentatonic minor (C D♭ F G A♭) ───────────────────────────────
# bitmask 0x0952 = 0 3 5 7 10 → pitch classes 0, 3, 5, 7, 10
active_pcs = {0, 3, 5, 7, 10}
root_pc    = 0

# ── Guide ring ────────────────────────────────────────────────────────────────
guide = plt.Circle((0, 0), R, color=RULE, fill=False, linewidth=1.0, zorder=1)
ax.add_patch(guide)

# ── Spokes ────────────────────────────────────────────────────────────────────
for pc in range(12):
    angle = math.radians(pc * 30 - 90)
    x, y  = math.cos(angle) * R, math.sin(angle) * R
    ax.plot([0, x * 0.80], [0, y * 0.80],
            color=RULE, linewidth=0.4, zorder=1, alpha=0.5)

# ── Active-PCS polygon ────────────────────────────────────────────────────────
poly_pts = []
for pc in sorted(active_pcs, key=lambda p: p * 30):
    angle = math.radians(pc * 30 - 90)
    poly_pts.append((math.cos(angle) * R, math.sin(angle) * R))
# Close the polygon
if poly_pts:
    xs, ys = zip(*poly_pts)
    xs = list(xs) + [xs[0]]
    ys = list(ys) + [ys[0]]
    ax.fill(xs, ys, color=AMBER, alpha=0.18, zorder=2)
    ax.plot(xs, ys, color=AMBER, linewidth=1.8, zorder=3,
            solid_capstyle="round", solid_joinstyle="round")

# ── Nodes ─────────────────────────────────────────────────────────────────────
for pc in range(12):
    angle  = math.radians(pc * 30 - 90)
    cx, cy = math.cos(angle) * R, math.sin(angle) * R
    active = pc in active_pcs
    is_root= pc == root_pc

    if is_root:
        # Amber outer ring
        ring = plt.Circle((cx, cy), r + 0.018, color=AMBER, fill=False,
                           linewidth=2.2, zorder=4)
        ax.add_patch(ring)

    fill_col  = INK   if active else CARD
    edge_col  = AMBER if is_root else (INK if active else RULE)
    lw        = 2.0   if active else 1.0

    node = plt.Circle((cx, cy), r, color=fill_col, zorder=5,
                       linewidth=lw)
    node.set_edgecolor(edge_col)
    ax.add_patch(node)

# ── Centre monogram ───────────────────────────────────────────────────────────
ax.text(0, 0.06, "P", ha="center", va="center",
        fontsize=34, fontweight="bold",
        color=INK, fontfamily="sans-serif", zorder=6)
ax.text(0, -0.12, "F", ha="center", va="center",
        fontsize=34, fontweight="bold",
        color=AMBER, fontfamily="sans-serif", zorder=6)

# ── Rounded-rect background clip (iOS-style icon) ────────────────────────────
from matplotlib.patches import FancyBboxPatch
bg_rect = FancyBboxPatch((-1, -1), 2, 2,
                          boxstyle="round,pad=0.0,rounding_size=0.22",
                          facecolor=BG, edgecolor="none", zorder=0,
                          clip_on=False)
ax.add_patch(bg_rect)

# ── Output ────────────────────────────────────────────────────────────────────
out_path = os.path.join(os.path.dirname(__file__), "icon_1024.png")
plt.savefig(out_path, dpi=DPI, bbox_inches="tight",
            facecolor=BG, edgecolor="none")
plt.close(fig)
print(f"Generated {out_path}")
