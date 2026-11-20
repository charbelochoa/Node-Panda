---
name: Auditoria de Codigo
description: Revision tecnica de seguridad, memoria, concurrencia y contratos de API en C++17 y Lua.
builtin: true
---

# Auditoria de Codigo

1. Verifica gestion de recursos RAII, cierre de descriptores y sincronizacion con mutex en rutas multihilo.
2. Comprueba validacion de entradas externas, limites de buffers y manejo de errores en operaciones de red y disco.
3. Asegura compatibilidad estricta con C++17 sin dependencias no declaradas.
4. Detecta regresiones de contrato en encabezados publicos y verifica consistencia entre declaracion e implementacion.
5. Reporta hallazgos priorizados por severidad con ubicacion exacta y correccion concreta.
