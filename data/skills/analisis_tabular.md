---
name: Analisis Tabular
description: Perfilado estadistico de CSV, deteccion de tipos, agregaciones y generacion de graficos SVG.
builtin: true
---

# Analisis Tabular

1. Perfila el archivo CSV antes de agregar: valida numero de filas, columnas, valores nulos y rango numerico.
2. Verifica la integridad del archivo fuente mediante su hash SHA-256.
3. Selecciona la funcion de agregacion adecuada (`count`, `sum`, `avg`, `min`, `max`) segun la pregunta del usuario.
4. Agrupa por dimensiones categoricas limpias y ordena los resultados de mayor a menor relevancia.
5. Presenta los totales verificados y genera una visualizacion SVG reproducible cuando se solicite reporte grafico.
