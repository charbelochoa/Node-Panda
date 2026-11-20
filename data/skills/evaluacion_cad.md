---
name: Evaluacion CAD
description: Interpretacion determinista de probabilidades multietiqueta de 14 clases toracicas y calculo de ROC-AUC.
builtin: true
---

# Evaluacion CAD

1. Convierte los logits de las 14 patologias mediante funcion sigmoide y aplica el umbral operativo seleccionado.
2. Reporta cada hallazgo con su probabilidad porcentual y clasificacion binaria positiva o negativa.
3. Calcula el estimador ROC-AUC para cuantificar la separacion entre clases positivas y negativas.
4. Destaca cambios de sensibilidad y especificidad cuando el operador ajusta el umbral de decision.
5. Presenta los resultados como soporte analitico cuantitativo sin sustituir validacion clinica profesional.
