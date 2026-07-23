"""Abre varias ventanas de navegador con perfiles y cookies independientes.

Uso basico:
    python abrir_perfiles.py --perfiles 5 --url https://example.com

Cada perfil se guarda en ./profiles/profile_XX dentro de esta carpeta.
"""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import subprocess
from pathlib import Path


DEFAULT_URL = "https://example.com"
DEFAULT_PROFILES = 5
PROFILE_ROOT = Path(__file__).resolve().parent / "profiles"


BROWSER_CANDIDATES = {
    "Windows": [
        r"C:\Program Files\Google\Chrome\Application\chrome.exe",
        r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
        r"C:\Program Files\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\BraveSoftware\Brave-Browser\Application\brave.exe",
    ],
    "Darwin": [
        "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
        "/Applications/Microsoft Edge.app/Contents/MacOS/Microsoft Edge",
        "/Applications/Brave Browser.app/Contents/MacOS/Brave Browser",
        "/Applications/Chromium.app/Contents/MacOS/Chromium",
    ],
    "Linux": [
        "google-chrome",
        "google-chrome-stable",
        "chromium",
        "chromium-browser",
        "microsoft-edge",
        "brave-browser",
    ],
}


def executable_exists(candidate: str) -> str | None:
    """Return an executable path if the candidate exists in PATH or as a file."""
    path_candidate = Path(candidate)
    if path_candidate.exists():
        return str(path_candidate)
    return shutil.which(candidate)


def find_browser(explicit_path: str | None = None) -> str:
    """Find a Chromium-based browser without using automation-only browsers."""
    if explicit_path:
        resolved = executable_exists(explicit_path)
        if resolved:
            return resolved
        raise FileNotFoundError(f"No se encontro el navegador indicado: {explicit_path}")

    system = platform.system()
    for candidate in BROWSER_CANDIDATES.get(system, []):
        resolved = executable_exists(candidate)
        if resolved:
            return resolved

    raise FileNotFoundError(
        "No se encontro Chrome, Edge, Brave o Chromium. "
        "Instala uno o usa --browser con la ruta completa del ejecutable."
    )


def build_command(browser: str, profile_dir: Path, url: str, lite: bool) -> list[str]:
    """Build a browser command with isolated storage for cookies and cache."""
    command = [
        browser,
        f"--user-data-dir={profile_dir}",
        "--new-window",
        "--no-first-run",
        "--no-default-browser-check",
    ]

    if lite:
        command.extend(
            [
                "--disable-background-networking",
                "--disable-background-timer-throttling",
                "--disable-client-side-phishing-detection",
                "--disable-component-update",
                "--disable-default-apps",
                "--disable-extensions",
                "--disable-features=MediaRouter,OptimizationHints,Translate",
                "--disable-popup-blocking",
                "--disable-sync",
                "--metrics-recording-only",
                "--mute-audio",
            ]
        )

    command.append(url)
    return command


def open_profiles(count: int, url: str, browser: str | None, lite: bool) -> None:
    executable = find_browser(browser)
    PROFILE_ROOT.mkdir(exist_ok=True)

    for index in range(count):
        profile_dir = PROFILE_ROOT / f"profile_{index:02d}"
        profile_dir.mkdir(exist_ok=True)
        subprocess.Popen(build_command(executable, profile_dir, url, lite))

    print(f"Abiertos {count} perfiles independientes con: {executable}")
    print(f"Carpeta de perfiles: {PROFILE_ROOT}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Abre navegadores reales con cookies separadas por perfil."
    )
    parser.add_argument("--perfiles", type=int, default=DEFAULT_PROFILES, help="Cantidad de perfiles a abrir.")
    parser.add_argument("--url", default=DEFAULT_URL, help="URL inicial para cada ventana.")
    parser.add_argument("--browser", help="Ruta al ejecutable de Chrome, Edge, Brave o Chromium.")
    parser.add_argument(
        "--sin-modo-liviano",
        action="store_true",
        help="No agrega flags para reducir tareas de fondo del navegador.",
    )
    args = parser.parse_args()

    if args.perfiles < 1:
        parser.error("--perfiles debe ser 1 o mayor.")
    return args


if __name__ == "__main__":
    parsed_args = parse_args()
    open_profiles(
        count=parsed_args.perfiles,
        url=parsed_args.url,
        browser=parsed_args.browser or os.environ.get("BROWSER_PATH"),
        lite=not parsed_args.sin_modo_liviano,
    )
