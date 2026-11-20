---
name: Consultas Datalog
description: Formulacion de consultas declarativas sobre hechos de notas, metadatos y enlaces del vault.
builtin: true
---

# Consultas Datalog

1. Modela las notas como hechos relacionales con atributos de ID, titulo, tipo, etiquetas y enlaces.
2. Usa predicados de filtrado por frontmatter y pertenencia de enlaces para localizar subconjuntos exactos.
3. Acota la recuperacion al presupuesto de tokens del contexto evitando ruido innecesario.
4. Combina coincidencias lexicas con backlinks para priorizar notas de referencia canonica.
5. Devuelve resultados estructurados y ordenados por relevancia decreciente.
