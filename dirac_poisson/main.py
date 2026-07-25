"""Command-line entry point for the radial Dirac--Poisson experiment."""

from __future__ import annotations

import argparse
from pathlib import Path

from constants import MC2
from grid import make_radial_grid
from plots import save_csv, save_plots
from solver import SolverConfig, solve


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Solver autoconsistente Dirac--Poisson radial con masa fija m_e.")
    parser.add_argument("--rmax", type=float, default=1.0e-10, help="Radio maximo de la malla en metros.")
    parser.add_argument("--points", type=int, default=800, help="Numero de puntos radiales.")
    parser.add_argument("--alpha", type=float, default=0.2, help="Factor de mezcla para estabilidad.")
    parser.add_argument("--tol", type=float, default=1.0e-8, help="Tolerancia de convergencia para el espinor.")
    parser.add_argument("--max-iter", type=int, default=80, help="Iteraciones maximas del punto fijo.")
    parser.add_argument("--width", type=float, default=2.0e-11, help="Ancho inicial gaussiano en metros.")
    parser.add_argument("--output", type=Path, default=Path("dirac_poisson_output"), help="Carpeta de salida.")
    parser.add_argument("--plot", action="store_true", help="Guardar PNG si matplotlib esta disponible.")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    grid = make_radial_grid(r_max=args.rmax, points=args.points)
    config = SolverConfig(alpha=args.alpha, tolerance=args.tol, max_iterations=args.max_iter, initial_width=args.width)
    result = solve(grid, config)

    args.output.mkdir(parents=True, exist_ok=True)
    save_csv(args.output / "solution.csv", grid.r, result.F, result.G, result.phi, result.electric_field, result.rho)
    if args.plot:
        save_plots(args.output / "diagnostics.png", grid.r, result.F, result.G, result.phi, result.rho)

    print(f"converged={result.converged}")
    print(f"iterations={result.iterations}")
    print(f"delta={result.delta:.6e}")
    print(f"energy_J={result.energy:.12e}")
    print(f"mc2_J={MC2:.12e}")
    print(f"energy_minus_mc2_J={result.energy - MC2:.12e}")
    print(f"rms_radius_m={result.rms_radius:.12e}")
    print(f"csv={args.output / 'solution.csv'}")


if __name__ == "__main__":
    main()
