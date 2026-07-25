"""Spherical Poisson solve by Gauss-law integration."""

from __future__ import annotations

import numpy as np

from constants import EPS0, E_CHARGE


def cumulative_trapezoid(y: np.ndarray, dx: float) -> np.ndarray:
    """Cumulative trapezoid integral with the first value fixed to zero."""
    result = np.zeros_like(y, dtype=float)
    result[1:] = np.cumsum(0.5 * (y[1:] + y[:-1]) * dx)
    return result


def integrate_from_outer_boundary(y: np.ndarray, dx: float, outer_value: float = 0.0) -> np.ndarray:
    """Return f(r) = outer_value - integral_r^R y(s) ds on a uniform grid."""
    result = np.empty_like(y, dtype=float)
    result[-1] = outer_value
    for index in range(len(y) - 2, -1, -1):
        result[index] = result[index + 1] - 0.5 * (y[index + 1] + y[index]) * dx
    return result


def charge_density(F: np.ndarray, G: np.ndarray) -> np.ndarray:
    """Electron charge density used by the requested first radial model."""
    return -E_CHARGE * (F**2 + G**2)


def solve_poisson(r: np.ndarray, dr: float, rho: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Solve radial Poisson equation and return potential, electric field and enclosed charge."""
    enclosed_charge = cumulative_trapezoid(4.0 * np.pi * rho * r**2, dr)
    electric_field = enclosed_charge / (4.0 * np.pi * EPS0 * r**2)
    potential = integrate_from_outer_boundary(electric_field, dr, outer_value=0.0)
    return potential, electric_field, enclosed_charge
