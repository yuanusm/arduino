"""Self-consistent radial Dirac--Poisson fixed-point iteration."""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np
from scipy.sparse.linalg import eigsh

from constants import MC2
from dirac_matrix import build_hamiltonian
from grid import RadialGrid
from poisson import charge_density, solve_poisson


@dataclass
class SolverConfig:
    """Numerical parameters for the fixed-point loop."""

    alpha: float = 0.2
    tolerance: float = 1.0e-8
    max_iterations: int = 80
    initial_width: float = 2.0e-11
    kappa: int = -1


@dataclass
class SolverResult:
    """Final fields and convergence diagnostics."""

    F: np.ndarray
    G: np.ndarray
    phi: np.ndarray
    electric_field: np.ndarray
    rho: np.ndarray
    energy: float
    delta: float
    iterations: int
    converged: bool
    rms_radius: float


def normalize(F: np.ndarray, G: np.ndarray, dr: float) -> tuple[np.ndarray, np.ndarray]:
    norm = float(np.sqrt(np.sum((F**2 + G**2) * dr)))
    if norm == 0.0:
        raise ValueError("cannot normalize a zero spinor")
    return F / norm, G / norm


def initial_spinor(r: np.ndarray, dr: float, width: float) -> tuple[np.ndarray, np.ndarray]:
    F = np.exp(-(r / width) ** 2)
    G = np.zeros_like(F)
    return normalize(F, G, dr)


def spinor_delta(old_F: np.ndarray, old_G: np.ndarray, new_F: np.ndarray, new_G: np.ndarray, dr: float) -> float:
    return float(np.sqrt(np.sum(((new_F - old_F) ** 2 + (new_G - old_G) ** 2) * dr)))


def rms_radius(r: np.ndarray, F: np.ndarray, G: np.ndarray, dr: float) -> float:
    return float(np.sqrt(np.sum(r**2 * (F**2 + G**2) * dr)))


def solve(grid: RadialGrid, config: SolverConfig) -> SolverResult:
    """Run the self-consistent Dirac--Poisson fixed-point iteration."""
    F, G = initial_spinor(grid.r, grid.dr, config.initial_width)
    energy = float("nan")
    delta = float("inf")
    phi = np.zeros_like(grid.r)
    electric_field = np.zeros_like(grid.r)
    rho = charge_density(F, G)

    for iteration in range(1, config.max_iterations + 1):
        old_F = F.copy()
        old_G = G.copy()

        rho = charge_density(F, G)
        phi, electric_field, _ = solve_poisson(grid.r, grid.dr, rho)
        hamiltonian = build_hamiltonian(grid.r, grid.dr, phi, kappa=config.kappa)

        eigenvalues, eigenvectors = eigsh(hamiltonian, k=1, sigma=MC2, which="LM")
        energy = float(eigenvalues[0])
        vector = np.real(eigenvectors[:, 0])
        candidate_F, candidate_G = normalize(vector[: grid.points], vector[grid.points :], grid.dr)

        if np.dot(candidate_F, F) < 0.0:
            candidate_F *= -1.0
            candidate_G *= -1.0

        mixed_F = (1.0 - config.alpha) * F + config.alpha * candidate_F
        mixed_G = (1.0 - config.alpha) * G + config.alpha * candidate_G
        F, G = normalize(mixed_F, mixed_G, grid.dr)

        delta = spinor_delta(old_F, old_G, F, G, grid.dr)
        if delta < config.tolerance:
            break

    rho = charge_density(F, G)
    phi, electric_field, _ = solve_poisson(grid.r, grid.dr, rho)
    return SolverResult(
        F=F,
        G=G,
        phi=phi,
        electric_field=electric_field,
        rho=rho,
        energy=energy,
        delta=delta,
        iterations=iteration,
        converged=delta < config.tolerance,
        rms_radius=rms_radius(grid.r, F, G, grid.dr),
    )
