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

### Subsistemas Nativos de Node Panda v2.1 (C++17 Puro)
- **Anotador PDF Nativo** (`pdf_engine`, `pdf_panel`) — Parser y generador PDF 1.4, cálculo de huella SHA-256, resaltados persistentes en coordenadas normalizadas almacenados en TSV y exportación de citas bidireccionales hacia notas Markdown.
- **Colaboración en Tiempo Real WebSocket RTC** (`websocket_rtc`, `rtc_panel`) — Implementación nativa del protocolo WebSocket RFC 6455 sobre TCP en el puerto `8043`, difusión de presencia local/remota y transmisión de deltas en tiempo real.
- **Vistas de Tabla y Base de Datos de Notas** (`table_view_engine`, `table_view_panel`) — Motor tabular interactivo con filtros combinados por etiqueta, tipo y namespace, ordenamiento, agregaciones por columna, edición inline y exportación a CSV y Markdown.
- **Motor de Búsqueda Vectorial Híbrida 384D** (`vector_engine`, `vector_panel`) — Embeddings locales deterministas de 384 dimensiones con soporte opcional de API remota de embeddings y ranking híbrido (similitud coseno + relevancia léxica).
- **Renderizador Matemático LaTeX y Companion Móvil PWA** (`math_renderer`, `math_panel`, `mobile_bridge`) — Parseo y renderizado vectorial de expresiones LaTeX (fracciones, raíces, subíndices, superíndices y símbolos griegos/operadores) directamente sobre `ImDrawList`, junto con una aplicación web progresiva (PWA) servida por `HttpServer` para acceso móvil en red local.

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
