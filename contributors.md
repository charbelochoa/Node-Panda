# Guía para Contribuidores (Contributors)

Gracias por tu interés en contribuir a **Node Panda**. Este proyecto está desarrollado íntegramente en **C++17 puro** con renderizado nativo OpenGL, interfaz interactiva sobre Dear ImGui, motor de bloques Outliner, consultas Datalog, coordinación de agentes de IA (locales y por API), interfaz de línea de comandos (CLI) con autenticación en terminal y soporte de bases de datos MySQL y PostgreSQL.

---

## Principios del Código

1. **C++17 Puro y Rendimiento Nativo**
   - Todo componente nuevo debe implementarse en C++17 estándar priorizando baja latencia, uso eficiente de memoria y compilación limpia en Windows (MSVC / MinGW-w64 UCRT64) y Linux (GCC / Clang / Docker).
2. **Cero Comentarios en el Código Fuente**
   - El código debe ser autoexplicativo mediante nombres claros de clases, métodos, estructuras y variables. No se aceptan bloques de comentarios ni créditos incrustados dentro de archivos `.h`, `.cpp`, `.lua`, `CMakeLists.txt` o scripts de base de datos.
   - Cualquier atribución o referencia externa debe registrarse exclusivamente en `creditos.md`.
3. **Interfaz Limpia y Directa**
   - Evita subtítulos irrelevantes o textos decorativos innecesarios en los paneles de la aplicación.
   - Respeta el sistema visual de superficies, píldoras segmentadas, indicadores de estado e interacciones definidas en `include/ui_theme.h` e `include/ui_components.h`.
4. **Manejo de Errores Fail-Fast**
   - Valida entradas en las fronteras del sistema (CLI, API HTTP, conectores MySQL/PostgreSQL, respuestas de proveedores de IA y plugins Lua en sandbox) y reporta errores explícitos sin ocultar estados inválidos.

---

## Estructura del Repositorio

```text
Node-Panda/
├── CMakeLists.txt
├── Dockerfile
├── docker-compose.yml
├── creditos.md
├── contributors.md
├── PLUGIN_API.md
├── include/
│   ├── app.h
│   ├── auth_manager.h
│   ├── crypto_engine.h
│   ├── database_manager.h
│   ├── database_panel.h
│   ├── ui_theme.h
│   ├── ui_components.h
│   ├── outliner_engine.h
│   ├── outliner_panel.h
│   ├── task_engine.h
│   ├── task_panel.h
│   ├── journal_engine.h
│   ├── journal_panel.h
│   ├── whiteboard_engine.h
│   ├── whiteboard_panel.h
│   ├── datalog_engine.h
│   ├── property_engine.h
│   ├── asset_manager.h
│   ├── asset_panel.h
│   ├── backup_manager.h
│   ├── sync_engine.h
│   ├── sync_panel.h
│   ├── command_palette.h
│   ├── ai_catalog.h
│   ├── ai_provider.h
│   ├── context_brain.h
│   ├── skill_manager.h
│   ├── usage_ledger.h
│   ├── data_lab.h
│   ├── evolution_engine.h
│   ├── agent_engine.h
│   ├── agent_panel.h
│   ├── lab_panel.h
│   ├── cli_runner.h
│   └── ...
├── src/
│   └── (implementaciones C++ correspondientes)
├── data/
│   ├── plugins/
│   ├── skills/
│   └── sql/
├── resources/
│   └── app.rc
└── third_party/
    └── stb_image.h
```

---

## Compilación y Ejecución

### Compilación con CMake

```powershell
mkdir build
cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

También es compatible con Visual Studio (`cmake .. -G "Visual Studio 17 2022"`).

### Modos de Uso

1. **Autenticación en Terminal y Lanzamiento Interactivo**:
   ```powershell
   .\NodePanda.exe
   ```
   Solicita credenciales de usuario en la terminal antes de iniciar la interfaz gráfica compilada o el servidor local.

2. **Modo Servidor Local**:
   ```powershell
   .\NodePanda.exe --server --port 8042
   ```

3. **Comandos CLI**:
   ```powershell
   .\NodePanda.exe login
   .\NodePanda.exe status
   .\NodePanda.exe chat --name atlas --room equipo
   .\NodePanda.exe run "Analiza la arquitectura del grafo" --model auto --wait
   .\NodePanda.exe list page
   .\NodePanda.exe list task --status TODO
   .\NodePanda.exe upsert block --target-page Bienvenida --content "- Nuevo bloque"
   .\NodePanda.exe query --name task-search
   .\NodePanda.exe db status
   .\NodePanda.exe db init --engine postgres
   ```

4. **Despliegue con Docker**:
   ```bash
   docker compose up --build -d
   ```

---

## Flujo de Trabajo para Pull Requests

1. Crea una rama descriptiva para tu funcionalidad o corrección.
2. Haz commits atómicos con mensajes imperativos y concisos (`feat:`, `fix:`, `enhance:`, `refactor:`, `chore:`).
3. Verifica que el proyecto compile sin errores y que no se hayan introducido comentarios en el código fuente.
4. Abre un Pull Request describiendo los cambios técnicos y las pruebas realizadas.
