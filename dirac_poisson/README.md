# Solver radial Dirac--Poisson con masa fija `m_e`

Esta carpeta es independiente del resto del repositorio y contiene una primera versión programable del experimento autoconsistente Dirac--Poisson radial para un estado `1s_{1/2}` con `kappa = -1` y masa fija igual a la masa del electrón.

El objetivo inicial no es demostrar un modelo físico completo del electrón, sino tener un experimento numérico reproducible para preguntar si existe un punto fijo estable:

```text
H_D(phi) psi = E psi
nabla^2 phi = -rho / epsilon_0
m = m_e
```

## Estructura

```text
dirac_poisson/
├── main.py
├── constants.py
├── grid.py
├── poisson.py
├── dirac_matrix.py
├── solver.py
├── plots.py
└── README.md
```

## Dependencias

```bash
python -m pip install numpy scipy matplotlib
```

`matplotlib` solo es necesario si se usa `--plot`.

## Ejecución rápida

Para una prueba liviana:

```bash
python dirac_poisson/main.py --points 200 --max-iter 5
```

Para acercarse a la malla propuesta inicialmente:

```bash
python dirac_poisson/main.py --rmax 5e-10 --points 2000 --max-iter 80 --alpha 0.2 --tol 1e-8 --plot
```

Los resultados se guardan por defecto en `dirac_poisson_output/solution.csv`. Si se usa `--plot`, también se guarda `dirac_poisson_output/diagnostics.png`.

## Decisiones numéricas de esta primera versión

- La malla radial evita exactamente `r = 0` para prevenir divisiones por cero.
- El espinor radial se representa con dos componentes reales `F(r)` y `G(r)`.
- La normalización usada es `integral (F^2 + G^2) dr = 1`.
- La densidad física se calcula como `rho(r) = -e (F^2 + G^2) / (4 pi r^2)`, consistente con `psi = (F/r, iG/r)`.
- La carga encerrada se calcula directamente como `Q(r) = -e integral_0^r (F^2 + G^2) dr`, por lo que `Q(Rmax)` debe aproximarse a `-e` cuando el dominio contiene todo el espinor.
- La condición inicial usa `F(r) = A exp(-r^2 / a^2)` y `G(r) = 0`.
- Poisson se resuelve por integración radial usando la ley de Gauss y la carga encerrada radial corregida; la condición de borde externa usa la cola Coulombiana `phi(Rmax) = Q(Rmax)/(4 pi epsilon_0 Rmax)`.
- El Hamiltoniano radial se arma como matriz dispersa y se resuelve con `scipy.sparse.linalg.eigsh` cerca de `m_e c^2`.
- La iteración autoconsistente usa mezcla lineal para reducir oscilaciones.

## Salidas principales

El programa imprime:

- si convergió;
- número de iteraciones;
- `delta` entre espinores consecutivos;
- energía del autovalor;
- comparación con `m_e c^2`;
- radio RMS;
- carga total encerrada al final de la malla;
- ruta del CSV generado.

El CSV contiene columnas:

```text
r_m,F,G,phi_V,electric_field_V_per_m,rho_C_per_m3,enclosed_charge_C
```

## Siguientes pasos recomendados

Si converge, revisar que `Q(Rmax)` sea cercano a `-e`, además del radio efectivo, energía y comportamiento Coulombiano lejano. Si no converge, estudiar primero sensibilidad numérica: tamaño de malla, `Rmax`, `alpha`, condición inicial y regularización cerca del origen antes de concluir que el sistema no tiene solución estacionaria.
