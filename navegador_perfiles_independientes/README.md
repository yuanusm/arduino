# Navegadores con cookies independientes

Esta carpeta contiene un sistema simple para abrir varias ventanas de un navegador real basado en Chromium, cada una con su propio directorio de perfil. Al usar `--user-data-dir` distinto por ventana, las cookies, sesiones, caché, extensiones y almacenamiento local quedan separados entre perfiles.

## Objetivo

- Usar un navegador legítimo instalado en el equipo: Chrome, Edge, Brave o Chromium.
- Mantener perfiles totalmente independientes en `profiles/profile_XX`.
- Reducir carga del computador desactivando funciones no esenciales en segundo plano.
- Evitar navegadores automatizados o técnicas de evasión: es un lanzador de perfiles locales.

## Uso rápido

```bash
python abrir_perfiles.py --perfiles 5 --url https://example.com
```

En Windows, si Chrome no está en la ruta habitual, indica el ejecutable:

```bash
python abrir_perfiles.py --perfiles 5 --url https://example.com --browser "C:\Program Files\Google\Chrome\Application\chrome.exe"
```

También puedes definir la variable de entorno `BROWSER_PATH` para no repetir la ruta:

```bash
set BROWSER_PATH=C:\Program Files\Google\Chrome\Application\chrome.exe
python abrir_perfiles.py --perfiles 5 --url https://example.com
```

## Modo liviano

Por defecto el script agrega flags para reducir actividad de fondo, sincronización, extensiones y componentes no esenciales. Si necesitas el comportamiento normal del navegador, usa:

```bash
python abrir_perfiles.py --perfiles 5 --sin-modo-liviano
```

## Recomendaciones para no sobrecargar el computador

- Empieza con pocos perfiles, por ejemplo `--perfiles 3`, y sube gradualmente.
- Cierra ventanas que no estés usando.
- Usa Edge, Brave o Chromium si en tu equipo consumen menos memoria que Chrome.
- No instales extensiones en todos los perfiles salvo que sean necesarias.
- Borra la carpeta `profiles` cuando ya no necesites conservar sesiones o cookies.

## Notas

- Cada perfil es persistente: si vuelves a abrir `profile_00`, conservará sus cookies anteriores.
- Para reiniciar todo desde cero, cierra los navegadores y elimina la carpeta `profiles`.
- La separación de cookies depende de usar directorios de perfil distintos; no uses la misma carpeta de perfil en dos procesos a la vez.
