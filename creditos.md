# Créditos y Atribuciones

Este documento concentra los créditos, referencias arquitectónicas, bibliotecas de terceros y sistemas que forman parte de la evolución de **Node Panda** en C++. Ningún archivo de código fuente incluye comentarios o atribuciones embebidas con el fin de mantener el código limpio y estandarizado.

---

## Autoría Principal

- **Charbel Ochoa** — Autor y arquitecto principal de Node Panda.

---

## Referencias Arquitectónicas y Funcionales

### Gestión de Conocimiento y Outliner (Adaptación a C++ Nativo)
- **Logseq** — Referencia de arquitectura para el motor de bloques jerárquicos (Outliner), referencias cruzadas por UUID (`((uuid))`), bloques embebidos (`{{embed}}`), motor de consultas Datalog en memoria, propiedades tipadas (`key:: value`), gestión de tareas (`TODO`, `DOING`, `DONE`, `NOW`, `LATER`, `WAITING`, `CANCELED`), prioridades (`[#A]`, `[#B]`, `[#C]`), diarios automáticos, pizarras visuales (Whiteboards), tarjetas de repetición espaciada con supresión Cloze, respaldos atómicos de grafos y protocolo de sincronización ordenada por transacciones.

### Coordinación de Agentes de IA, Laboratorio de Datos y CLI (Adaptación a C++ Nativo)
- **AERIS** — Referencia para el motor de ejecución de misiones multi-agente (Router, Planificador, Constructor, Especialista y Revisor independiente), políticas de parada por presupuesto en nano-dólares (`costNano`), salas de colaboración compartidas, Cerebro de Contexto léxico-Datalog con presupuesto de tokens, catálogo multi-proveedor (OpenAI, Anthropic, Google Gemini, xAI, DeepSeek, NVIDIA NIM e Inferencia Local), registro contable de consumo (`Usage Ledger`), laboratorio de análisis tabular y motor de evolución de políticas.

### Sistema Visual e Interfaz Gráfica (Adaptación a Dear ImGui / OpenGL en C++)
- **FLOP** — Referencia visual para la paleta cromática de superficies neutras de alto contraste, tipografía limpia, controles segmentados tipo píldora, indicadores de estado dinámicos, tarjetas con elevación sutil y módulo determinista de evaluación analítica CAD de 14 clases.

---

## Bibliotecas y Componentes de Terceros en C++

| Componente | Licencia | Función en el Proyecto |
|---|---|---|
| **Dear ImGui** (Omar Cornut) | MIT | Motor de interfaz gráfica inmediata con soporte de acoplamiento (Docking) |
| **GLFW** | zlib/libpng | Gestión de ventanas multiplataforma, contexto OpenGL 3.3 y entradas |
| **Lua 5.4** (PUC-Rio) | MIT | Máquina virtual de scripting para consola interactiva y sistema de plugins en sandbox |
| **sol2** (ThePhD) | MIT | Enlaces de cabeceras entre C++17 y Lua |
| **stb_image** (Sean Barrett) | MIT / Dominio Público | Decodificación de texturas e imágenes en memoria |
| **OpenGL 3.3** | Estándar Khronos | Renderizado acelerado por hardware para interfaz, grafo 2D/3D y pizarra |
| **WinHTTP / Winsock2 / POSIX Sockets** | Sistema Operativo | Clientes HTTP/HTTPS nativos para APIs de IA, servidor REST local y protocolos TCP de bases de datos |
| **PostgreSQL & MySQL Protocols** | PostgreSQL / GPL | Conectores nativos C++ para persistencia local y en servidor |
