---
name: Arquitectura de Agentes
description: Diseno de flujos multi-agente con router, revisor, presupuesto y politicas de parada.
builtin: true
---

# Arquitectura de Agentes

1. Divide la mision en etapas verificables: recuperacion de contexto, planificacion, ejecucion y revision.
2. Respeta estrictamente el presupuesto USD asignado y detente al alcanzar el puntaje objetivo o detectar meseta de mejora.
3. Prefiere modelos ligeros para enrutamiento y revision rapida, reservando modelos de mayor capacidad para sintesis compleja.
4. Registra cada paso con metricas de tokens, latencia y costo en el ledger de consumo.
5. Comunica decisiones criticas en la sala activa indicando remitente, destinatario y evidencia concreta.
