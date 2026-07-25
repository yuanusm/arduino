"""Plotting helpers for solver output."""

from __future__ import annotations

from pathlib import Path

import numpy as np


def save_csv(path: Path, r: np.ndarray, F: np.ndarray, G: np.ndarray, phi: np.ndarray, electric_field: np.ndarray, rho: np.ndarray) -> None:
    """Save radial arrays to CSV without requiring plotting dependencies."""
    path.parent.mkdir(parents=True, exist_ok=True)
    data = np.column_stack([r, F, G, phi, electric_field, rho])
    header = "r_m,F,G,phi_V,electric_field_V_per_m,rho_C_per_m3"
    np.savetxt(path, data, delimiter=",", header=header, comments="")


def save_plots(path: Path, r: np.ndarray, F: np.ndarray, G: np.ndarray, phi: np.ndarray, rho: np.ndarray) -> None:
    """Save diagnostic plots when matplotlib is installed."""
    import matplotlib.pyplot as plt

    path.parent.mkdir(parents=True, exist_ok=True)
    figure, axes = plt.subplots(3, 1, figsize=(8, 10), sharex=True)
    axes[0].plot(r, F, label="F")
    axes[0].plot(r, G, label="G")
    axes[0].set_ylabel("spinor radial")
    axes[0].legend()
    axes[1].plot(r, phi)
    axes[1].set_ylabel("phi [V]")
    axes[2].plot(r, rho)
    axes[2].set_ylabel("rho [C/m^3]")
    axes[2].set_xlabel("r [m]")
    figure.tight_layout()
    figure.savefig(path)
    plt.close(figure)
