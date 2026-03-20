# Node Panda

Gestor personal de notas enlazadas con grafo visual interactivo. Construido desde cero en C++ con Dear ImGui y OpenGL.

![Node Panda](data/panda.png)

---

## Características

- **Notas en Markdown** con frontmatter YAML (tipo, aliases, tags)
- **Grafo de nodos interactivo** filtrado por la nota seleccionada — solo muestra las notas conectadas a la que estás editando
- **Coloreado por tipo**: proyecto (cyan), concepto (purple), referencia (amber), diario (verde), tarea (coral)
- **Editor con preview** de Markdown renderizado
- **Backlinks automáticos** — ve qué notas enlazan a la actual
- **File Watcher** — detecta cambios en disco en tiempo real
- **Exportar contexto** estructurado de una nota y sus enlaces
- **Hub de bienvenida** con opción de crear nueva nota o importar `.md` / `.txt`
- **Diálogo nativo de Windows** para cargar archivos

---

## Requisitos

| Herramienta | Versión mínima | Descarga |
|---|---|---|
| **Visual Studio** (con BuildTools C++) | 2019 o 2022 | https://visualstudio.microsoft.com/downloads/ |
| **CMake** | 3.14+ | https://cmake.org/download/ |
| **Git** | cualquier versión reciente | https://git-scm.com/ |
| **Windows** | 10 / 11 (64-bit) | — |

> **Nota sobre CMake:** durante la instalación de CMake, marca la opción **"Add CMake to the system PATH for all users"**. Si ya lo tienes instalado y `cmake` no se reconoce en la terminal, reinstálalo marcando esa opción.

---

## Instalación desde cero

### 1. Clonar el repositorio

```bash
git clone https://github.com/charbelochoa/NodePanda.git
cd NodePanda
```

### 2. Crear la carpeta de build

```bash
mkdir build
cd build
```

### 3. Generar el proyecto con CMake

Abre una **Developer PowerShell for VS 2026** — búscala en el menú Inicio — y ejecuta desde dentro de la carpeta `build`:

```powershell
cmake .. -G "Visual Studio 18 2026"
```

> Si usas Visual Studio **2019**, cambia el generador:
> ```powershell
> cmake .. -G "Visual Studio 16 2019"
> ```
> CMake descargará automáticamente GLFW e ImGui via FetchContent. Requiere conexión a internet en el primer build.

### 4. Compilar

```powershell
cmake --build . --config Release
```

La compilación tarda 1–3 minutos la primera vez (descarga + compilación de dependencias).

### 5. Ejecutar

```powershell
cd Release
.\NodePanda.exe
```

El ejecutable se encuentra en `build/Release/NodePanda.exe`.

---

## Estructura del proyecto

```
NodePanda/
├── CMakeLists.txt          # Configuración de build
├── data/
│   ├── panda.png           # Logo de la aplicación
│   └── notas/              # Tus notas se guardan aquí
├── include/                # Headers (.h)
│   ├── app.h
│   ├── note.h
│   ├── note_manager.h
│   ├── graph.h
│   ├── force_layout.h
│   ├── editor_panel.h
│   ├── graph_panel.h
│   ├── explorer_panel.h
│   ├── backlinks_panel.h
│   ├── ai_exporter.h
│   └── ...
├── src/                    # Implementaciones (.cpp)
│   ├── main.cpp
│   ├── app.cpp
│   ├── note.cpp
│   ├── note_manager.cpp
│   ├── graph.cpp
│   ├── force_layout.cpp
│   ├── editor_panel.cpp
│   ├── graph_panel.cpp
│   └── ...
└── third_party/
    └── stb_image.h         # Carga de imágenes (single-header)
```

> Las dependencias GLFW e ImGui se descargan automáticamente por CMake al hacer build. No necesitas instalarlas manualmente.

---

## Uso básico

### Hub de bienvenida
Al abrir la app aparece el hub con tres opciones:
- **+ Crear Nueva Nota** — abre un popup para escribir el nombre de la nota
- **Cargar Archivo (.md/.txt)** — importa un archivo existente a tu vault
- **Acceder a mis Notas** — entra directamente al entorno de notas

### Crear notas
Una vez en el entorno, usa el botón **+ Nota** en el panel izquierdo (Explorador de Notas).

### Enlazar notas
Dentro del contenido de una nota, usa la sintaxis `[[NombreDeLaNota]]` para crear un enlace. El grafo se actualiza automáticamente.

### Frontmatter
Al inicio de una nota puedes agregar metadatos YAML para darle tipo y color en el grafo:

```yaml
---
tipo: proyecto
aliases: MiProyecto, Proyecto1
tags: diseño, web
---
```

Tipos disponibles: `proyecto`, `concepto`, `referencia`, `diario`, `tarea`.

### Grafo de nodos
El panel derecho muestra solo los nodos conectados a la nota que estás editando actualmente. Al seleccionar una nota diferente (en el explorador o haciendo clic en un nodo), el grafo se reconstruye mostrando únicamente su componente conexo.

- **Scroll** → zoom
- **Click medio + arrastrar** → pan
- **Click en un nodo** → selecciona esa nota
- **Botón Reset** → recentra la cámara

---

## Problemas frecuentes

**`cmake` no se reconoce**
Reinstala CMake desde https://cmake.org/download/ marcando *"Add CMake to the system PATH"*, luego cierra y vuelve a abrir la terminal.

**Error de generador al re-hacer cmake**
Si ya existe una carpeta `build` de un build anterior, bórrala y vuelve a crearla:
```powershell
cd ..
Remove-Item -Recurse -Force build
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
```

**`panda.png` no aparece**
Verifica que el archivo `data/panda.png` existe en la raíz del repositorio. Al compilar, CMake lo copia a `build/Release/data/panda.png` automáticamente.

**El grafo no se actualiza**
Haz clic en otra nota y vuelve. Si el problema persiste, ve a **Archivo → Reescanear Notas** en la barra de menú.

---

## Dependencias (gestionadas automáticamente por CMake)

| Librería | Versión | Propósito |
|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | v1.90.1-docking | Interfaz gráfica |
| [GLFW](https://github.com/glfw/glfw) | 3.3.8 | Ventana y contexto OpenGL |
| [stb_image](https://github.com/nothings/stb) | — | Carga de texturas PNG |
| OpenGL | sistema | Renderizado |
| comdlg32 | sistema (Windows) | Diálogo nativo de archivos |

---

## Licencia

Proyecto personal / educativo. Sin licencia formal.
