# Node Panda

A personal linked-notes manager featuring an interactive visual graph, a knowledge engine, and a local API. Built entirely from scratch in C++ using Dear ImGui and OpenGL.

![Node Panda](data/panda.png)

---

## Changelog

### v1.1.0 — Knowledge Engine Update
- **Query Engine** — SQL-like queries for your vault (`QUERY notas WHERE tipo = "proyecto" AND enlaces > 3`).
- **Git Version Control** — Automatic history tracking for every note, inline diff viewer, and instant version rollback.
- **Graph Analytics** — PageRank, Betweenness Centrality, and community detection running right on your note graph.
- **Spaced Repetition (SRS)** — Built-in SM-2 algorithm directly in your notes for active recall.
- **Local REST API** — HTTP server running on `localhost:8042` so you can hook Node Panda up to your own scripts and external tools.
- **Plugin System** — Lua 5.4 engine with a security sandbox, lifecycle hooks, ImGui bindings, and 8 bundled plugins out of the box.

### v1.0.0 — Initial Release
- Markdown notes with YAML frontmatter.
- Interactive node graph using a Barnes-Hut force layout.
- Editor with live Markdown preview.
- Automatic backlinks.
- TF-IDF Semantic Memory engine.
- Real-time disk file watcher.
- Context export tailored for Claude (XML).
- Integrated Lua scripting console.
- Welcome Hub.

---

## Features Breakdown

### The Core Stuff
- **Markdown Notes** powered by YAML frontmatter (track types, aliases, tags).
- **Interactive Node Graph** running a Barnes-Hut physics simulation.
- **Color-coded by type**: projects (cyan), concepts (purple), references (amber), journal (green), tasks (coral).
- **Split Editor** showing your raw markdown and a rendered preview.
- **Auto-Backlinks** — instantly see which notes link back to what you're reading.
- **TF-IDF Memory Engine** — handles semantic search and smart link suggestions.
- **File Watcher** — edits made outside the app update instantly in the UI.
- **Claude Export** — dumps structured context in XML for easy AI prompting.
- **Lua Scripting** — full vault access right from the built-in console.

### v1.1.0 — The New Stuff

#### Query Engine
Run structured queries across your entire vault using a custom syntax:
` ` `
QUERY notas WHERE tipo = "proyecto" AND enlaces > 3 SORT palabras DESC LIMIT 10
QUERY notas WHERE tags CONTIENE "ia"
QUERY notas WHERE palabras > 500 SORT fecha DESC
` ` `
You can query by: `tipo` (type), `palabras` (word count), `caracteres` (chars), `enlaces` (links), `conexiones` (connections), `fecha` (date), or literally any custom key in your frontmatter.

#### Git Version Control
- Auto-commits with a timestamp every time you save.
- View the complete history of any specific note.
- Inline diff viewer (green = added, red = removed).
- One-click restore to any older version.
- *Note: Requires Git installed on your machine (`git` in your system PATH).*

#### Graph Analytics
- **PageRank** — finds the most heavily referenced/important notes in your vault.
- **Betweenness Centrality** — spots the "bridge" notes connecting different knowledge domains.
- **Community Detection** — automatically groups related notes into thematic clusters using Label Propagation.

#### Spaced Repetition (SRS)
- Powered by the SM-2 algorithm (the exact same one Anki uses).
- Daily review queue built right into your vault.
- Auto-calculates your next review intervals based on difficulty (Easy / Good / Hard / Again).
- Saves the SRS data cleanly into each note's frontmatter (`srs_interval`, `srs_ease`, `srs_next`).

#### Local REST API
A lightweight HTTP server hosted on `localhost:8042` — accessible only from your local machine:

| Endpoint | What it does |
|---|---|
| `GET /api/notas` | Lists all notes + metadata |
| `GET /api/notas/{id}` | Grabs the full content of a specific note |
| `GET /api/grafo` | Returns the graph structure (nodes + edges) |
| `GET /api/memoria/buscar?q=...&n=5` | TF-IDF semantic search |
| `GET /api/stats` | Vault statistics |

#### Plugin System (Lua 5.4)
Easily extend the app. Runs in a secure sandbox. Here's what's included:

| Plugin | What it does |
|---|---|
| `daily_notes` | Auto-creates and opens your daily note |
| `stats_dashboard` | Prints vault stats right into the Lua console |
| `templates` | 5 preset templates (Meeting, Project, Concept, Task, Journal) |
| `tag_browser` | A UI panel to browse and filter notes by tags |
| `word_count` | Live stats for your active note (word count, reading time) |
| `auto_linker` | Recommends links based on TF-IDF similarity |
| `graph_exporter` | Dumps your graph to DOT (Graphviz) or JSON |
| `theme_switcher` | 5 color themes: Default, Nord, Catppuccin, Gruvbox Dark, Dracula |

---

## Requirements

| Tool | Min Version | Get it here |
|---|---|---|
| **Visual Studio** (w/ C++ BuildTools) | 2019 or 2022 | https://visualstudio.microsoft.com/downloads/ |
| **CMake** | 3.14+ | https://cmake.org/download/ |
| **Git** | Any recent version | https://git-scm.com/ |
| **OS** | Windows 10 / 11 (64-bit) | — |

> **Heads up on Git:** For the Version Control features to work, Git needs to be globally accessible in your terminal (running `git --version` should print the version). If it doesn't, reinstall Git and make sure to check the *"Add Git to the system PATH"* box.

---

## Building from Scratch

### 1. Clone the repo

```bash
git clone [https://github.com/charbelochoa/NodePanda.git](https://github.com/charbelochoa/NodePanda.git)
cd NodePanda
```

### 2. Set up the build directory

```bash
mkdir build
cd build
```

### 3. Generate the CMake project

Open up a **Developer PowerShell for VS 2022** (just search for it in your Windows Start menu), make sure you are inside the `build` folder, and run:

```powershell
cmake .. -G "Visual Studio 17 2022"
```

> If you are still rocking Visual Studio **2019**, run this instead:
> ```powershell
> cmake .. -G "Visual Studio 16 2019"
> ```
> *Note: CMake will automatically download GLFW, ImGui, and Lua via FetchContent. You'll need internet access for this first run.*

### 4. Compile

```powershell
cmake --build . --config Release
```

Give it about 1–3 minutes the first time you compile.

### 5. Run it

```powershell
cd Release
.\NodePanda.exe
```

---

## Project Structure

```text
NodePanda/
├── CMakeLists.txt
├── PLUGIN_API.md               # Full docs for the Plugin API
├── data/
│   ├── panda.png
│   ├── notas/                  # Your actual notes (Markdown)
│   └── plugins/                # Lua Plugins live here
│       ├── daily_notes/
│       ├── templates/
│       ├── tag_browser/
│       ├── word_count/
│       ├── auto_linker/
│       ├── graph_exporter/
│       └── theme_switcher/
├── include/                    # Header files
│   ├── app.h
│   ├── query_engine.h          # Handles the SQL-like queries
│   ├── graph_analytics.h       # PageRank & clustering logic
│   ├── srs_engine.h            # Spaced repetition math (SM-2)
│   ├── git_manager.h           # Git bindings
│   ├── http_server.h           # Local REST API
│   ├── plugin_manager.h        # Lua integration
│   └── ...
├── src/                        # C++ Source implementations
└── third_party/
    └── stb_image.h
```

---

## How to use it

### Creating and Linking Notes
Hit the **+ Nota** button in the Explorer. Link notes together using bracket syntax `[[NameOfYourNote]]`. The graph updates in real-time as you type.

### Frontmatter
Add metadata to the top of your markdown files like this:
```yaml
---
tipo: proyecto
aliases: MyProject
tags: design, web
---
```
Available types out of the box: `proyecto`, `concepto`, `referencia`, `diario`, `tarea`.

### Query Engine
Open the "Motor de Consultas" (Query Engine) tab at the bottom and try running:
```text
QUERY notas WHERE tipo = "proyecto" AND palabras > 200
QUERY notas WHERE enlaces = 0
```

### Version Control
History is tracked automatically behind the scenes. Just pop open the "Control de Versiones" panel to diff changes or rollback if you mess something up.

### REST API
While the app is running, you can hit these from your browser or terminal:
```text
http://localhost:8042/api/stats
http://localhost:8042/api/notas
```

### Plugins
Drop plugins into `data/plugins/`. Every plugin needs its own folder containing a `manifest.json` and a `plugin.lua` file. Check out [PLUGIN_API.md](PLUGIN_API.md) to see how to build your own.

### Navigating the Graph
- **Scroll wheel** → Zoom
- **Middle-click + drag** → Pan around
- **Left-click a node** → Opens that note in the editor
- **Tools → Recalculate Analytics** → Re-runs PageRank and community clustering

---

## Troubleshooting

**`cmake` is not recognized**
You need to install CMake from https://cmake.org/download/ and check the *"Add CMake to the system PATH"* box during installation.

**Generator error when re-running cmake**
If you already have an old `build` folder and things are getting tangled, just nuke it and start fresh:
```powershell
cd ..
Remove-Item -Recurse -Force build
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
```

**Version Control says it's inactive**
Make sure `git --version` actually works in your terminal. If it throws an error, install Git from https://git-scm.com and make sure you add it to your PATH.

**The API server isn't responding**
Check the "API Server" panel inside the app and make sure the status is "Active". If it didn't boot up automatically, go to **Tools → Start API Server**.

**The graph isn't updating**
Sometimes it needs a manual kick. Go to **File → Rescan Notes**.

---

## Dependencies (Handled automatically by CMake)

| Library | Version | What it does |
|---|---|---|
| [Dear ImGui](https://github.com/ocornut/imgui) | v1.90.1-docking | The whole UI |
| [GLFW](https://github.com/glfw/glfw) | 3.3.8 | Window management & OpenGL context |
| [Lua](https://www.lua.org) | 5.4.6 | Scripting and plugins |
| [sol2](https://github.com/ThePhD/sol2) | v3.3.0 | Binds the C++ backend to Lua |
| [stb_image](https://github.com/nothings/stb) | — | Loading PNG textures |
| OpenGL | System | Rendering backend |
| comdlg32 + ws2_32 | System (Win) | Native file dialogs + network sockets |

---

## License

Copyright (c) 2026 Charbel Ochoa.  
All rights reserved.

This is a personal/educational project.  
Use, copying, modification, or distribution of this code without explicit permission from the author is strictly prohibited.
