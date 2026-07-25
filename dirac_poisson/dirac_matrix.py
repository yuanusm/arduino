"""Sparse radial Dirac Hamiltonian construction."""

from __future__ import annotations

import numpy as np
from scipy import sparse

from constants import C, E_CHARGE, HBAR, M_E


def derivative_matrix(points: int, dr: float) -> sparse.csr_matrix:
    """Central-difference derivative matrix with one-sided boundary rows."""
    if points < 4:
        raise ValueError("points must be at least 4")

    matrix = sparse.lil_matrix((points, points), dtype=float)
    for index in range(1, points - 1):
        matrix[index, index + 1] = 0.5 / dr
        matrix[index, index - 1] = -0.5 / dr

    matrix[0, 0] = -1.0 / dr
    matrix[0, 1] = 1.0 / dr
    matrix[-1, -2] = -1.0 / dr
    matrix[-1, -1] = 1.0 / dr
    return matrix.tocsr()


def build_hamiltonian(r: np.ndarray, dr: float, phi: np.ndarray, kappa: int = -1) -> sparse.csr_matrix:
    """Build the 2N x 2N radial Dirac Hamiltonian for a fixed potential."""
    points = len(r)
    derivative = derivative_matrix(points, dr)
    kappa_over_r = sparse.diags(kappa / r, format="csr")
    identity = sparse.identity(points, format="csr")

    h11 = sparse.diags(M_E * C**2 + E_CHARGE * phi, format="csr")
    h22 = sparse.diags(-M_E * C**2 + E_CHARGE * phi, format="csr")
    h12 = C * HBAR * (-derivative + kappa_over_r)
    h21 = C * HBAR * (derivative + kappa_over_r)

    # The identity term is kept explicit to make the block dimensions obvious.
    return sparse.bmat([[h11 @ identity, h12], [h21, h22 @ identity]], format="csr")
