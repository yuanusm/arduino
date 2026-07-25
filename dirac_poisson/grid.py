"""Radial grid helpers."""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class RadialGrid:
    """Uniform radial grid that deliberately avoids r=0."""

    r: np.ndarray
    dr: float
    r_min: float
    r_max: float
    points: int


def make_radial_grid(r_max: float = 1.0e-10, points: int = 800, r_min: float | None = None) -> RadialGrid:
    """Create a radial grid on [r_min, r_max]."""
    if points < 4:
        raise ValueError("points must be at least 4")
    if r_max <= 0.0:
        raise ValueError("r_max must be positive")

    start = r_max / (points * 10.0) if r_min is None else r_min
    if not 0.0 < start < r_max:
        raise ValueError("r_min must satisfy 0 < r_min < r_max")

    r = np.linspace(start, r_max, points)
    return RadialGrid(r=r, dr=float(r[1] - r[0]), r_min=float(start), r_max=float(r_max), points=points)
