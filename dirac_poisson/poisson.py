"""Spherical Poisson solve by Gauss-law integration for radial Dirac spinors."""

from __future__ import annotations

import numpy as np

from constants import EPS0, E_CHARGE


def cumulative_trapezoid(y: np.ndarray, dx: float) -> np.ndarray:
    """Cumulative trapezoid integral with the first value fixed to zero."""
    result = np.zeros_like(y, dtype=float)
    result[1:] = np.cumsum(0.5 * (y[1:] + y[:-1]) * dx)
    return result


def integrate_potential_from_outer_boundary(electric_field: np.ndarray, dx: float, outer_value: float = 0.0) -> np.ndarray:
    """Return phi(r) = phi(R) + integral_r^R E(s) ds on a uniform grid."""
    potential = np.empty_like(electric_field, dtype=float)
    potential[-1] = outer_value
    for index in range(len(electric_field) - 2, -1, -1):
        potential[index] = potential[index + 1] + 0.5 * (electric_field[index + 1] + electric_field[index]) * dx
    return potential


def radial_probability(F: np.ndarray, G: np.ndarray) -> np.ndarray:
    """Return the radial probability density F^2 + G^2 normalized with dr."""
    return F**2 + G**2


def charge_density(r: np.ndarray, F: np.ndarray, G: np.ndarray) -> np.ndarray:
    """Return the physical volume charge density for psi=(F/r, iG/r)."""
    return -E_CHARGE * radial_probability(F, G) / (4.0 * np.pi * r**2)


def enclosed_charge_from_spinor(F: np.ndarray, G: np.ndarray, dr: float) -> np.ndarray:
    """Return Q(r) = -e integral_0^r (F^2 + G^2) dr for radial spinors."""
    return -E_CHARGE * cumulative_trapezoid(radial_probability(F, G), dr)


def solve_poisson_from_spinor(r: np.ndarray, dr: float, F: np.ndarray, G: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """Solve radial Poisson equation from radial spinor components.

    Returns potential, electric field, enclosed charge and physical charge density.
    """
    rho = charge_density(r, F, G)
    enclosed_charge = enclosed_charge_from_spinor(F, G, dr)
    electric_field = enclosed_charge / (4.0 * np.pi * EPS0 * r**2)
    outer_potential = enclosed_charge[-1] / (4.0 * np.pi * EPS0 * r[-1])
    potential = integrate_potential_from_outer_boundary(electric_field, dr, outer_value=outer_potential)
    return potential, electric_field, enclosed_charge, rho
