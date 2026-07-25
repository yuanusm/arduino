import numpy as np
import matplotlib.pyplot as plt

# cargar solución
data = np.loadtxt(
    "dirac_poisson_output/solution.csv",
    delimiter=",",
    skiprows=1
)

r = data[:,0]

# Ajustar el nombre según tu CSV
F = data[:,1]
G = data[:,2]

# constantes
e = 1.602176634e-19

# densidad radial
rho = -e*(F**2 + G**2)

# integración acumulada
Q = np.zeros_like(r)

for i in range(1,len(r)):
    dr = r[i]-r[i-1]
    Q[i] = Q[i-1] + 4*np.pi*r[i]**2*rho[i]*dr


plt.figure(figsize=(7,5))

plt.plot(
    r*1e12,
    Q/e
)

plt.xlabel("r (pm)")
plt.ylabel("Q(r)/e")

plt.grid()

plt.title(
    "Carga encerrada del electrón"
)

plt.show()