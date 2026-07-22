#!/usr/bin/env python3
"""Monitor serial inicial para el detector de metales por desfase.

Uso:
    python3 tools/serial_debug_plot.py /dev/ttyACM0 --mode all
    python3 tools/serial_debug_plot.py COM3 --mode tx

Comandos enviados al Arduino:
    tx      -> READ_TX
    rx      -> READ_RX
    all     -> READ_ALL y RESULT
    status  -> STATUS
    cal     -> CAL
"""

import argparse
import sys
import time
from collections import deque

import matplotlib.pyplot as plt
import serial

NUM_MUESTRAS = 128
COMANDOS = {
    "tx": "READ_TX",
    "rx": "READ_RX",
    "all": "READ_ALL",
    "status": "STATUS",
    "cal": "CAL",
}


def enviar(ser, comando):
    ser.write((comando + "\n").encode("ascii"))
    ser.flush()


def parsear_linea(linea):
    partes = linea.strip().split(",")
    if len(partes) < 2:
        return None
    return partes


def main():
    parser = argparse.ArgumentParser(description="Graficador serial TX/RX para debug inicial")
    parser.add_argument("port", help="Puerto serial, por ejemplo /dev/ttyACM0 o COM3")
    parser.add_argument("--baud", type=int, default=115200, help="Baudrate UART")
    parser.add_argument("--mode", choices=COMANDOS, default="all", help="Modo de lectura")
    parser.add_argument("--period", type=float, default=0.5, help="Segundos entre solicitudes")
    args = parser.parse_args()

    tx = [0] * NUM_MUESTRAS
    rx = [0] * NUM_MUESTRAS
    fases = deque(maxlen=100)

    plt.ion()
    fig, (ax_tx, ax_rx, ax_phase) = plt.subplots(3, 1, sharex=False)
    line_tx, = ax_tx.plot(tx, label="TX shunt")
    line_rx, = ax_rx.plot(rx, label="RX")
    line_phase, = ax_phase.plot([], [], label="Delta fase calibrada")
    ax_tx.set_ylabel("ADC")
    ax_rx.set_ylabel("ADC")
    ax_phase.set_ylabel("grados")
    ax_phase.set_xlabel("lectura")
    for ax in (ax_tx, ax_rx, ax_phase):
        ax.grid(True)
        ax.legend(loc="upper right")

    with serial.Serial(args.port, args.baud, timeout=0.2) as ser:
        time.sleep(2.0)
        ser.reset_input_buffer()
        print(f"Conectado a {args.port} @ {args.baud} bps")

        while True:
            enviar(ser, COMANDOS[args.mode])
            if args.mode == "all":
                enviar(ser, "RESULT")

            limite = time.time() + args.period
            while time.time() < limite:
                raw = ser.readline().decode("ascii", errors="replace").strip()
                if not raw:
                    continue
                print(raw)
                partes = parsear_linea(raw)
                if not partes:
                    continue

                if partes[0] == "TX" and len(partes) == 3:
                    tx[int(partes[1])] = int(partes[2])
                elif partes[0] == "RX" and len(partes) == 3:
                    rx[int(partes[1])] = int(partes[2])
                elif partes[0] == "DATA" and len(partes) == 4:
                    idx = int(partes[1])
                    tx[idx] = int(partes[2])
                    rx[idx] = int(partes[3])
                elif partes[0] == "DELTA_PHASE" and len(partes) >= 4:
                    fases.append(float(partes[3]))

            line_tx.set_ydata(tx)
            line_rx.set_ydata(rx)
            line_phase.set_data(range(len(fases)), list(fases))
            ax_tx.relim(); ax_tx.autoscale_view()
            ax_rx.relim(); ax_rx.autoscale_view()
            ax_phase.relim(); ax_phase.autoscale_view()
            fig.canvas.draw_idle()
            fig.canvas.flush_events()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
