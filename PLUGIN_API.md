# Node Panda — Plugin API Reference

Guía completa para desarrollar plugins para Node Panda usando Lua 5.4.

---

## Estructura de un Plugin

Cada plugin es una carpeta dentro de `data/plugins/` con dos archivos obligatorios:

```
data/plugins/mi_plugin/
├── manifest.json    -- metadatos del plugin
├── plugin.lua       -- código Lua principal
└── state.json       -- (auto-generado) persistencia de datos
```

### manifest.json

```json
{
  "name":        "Mi Plugin",
  "id":          "mi_plugin",
  "version":     "1.0.0",
  "author":      "Tu Nombre",
  "description": "Breve descripción de lo que hace.",
  "hooks":       ["on_load", "on_frame"],
  "enabled":     true
}
```

| Campo         | Tipo       | Descripción                                    |
|---------------|------------|------------------------------------------------|
| `name`        | string     | Nombre visible en el Gestor de Plugins         |
| `id`          | string     | Identificador único (snake_case)               |
| `version`     | string     | Versión semántica                              |
| `author`      | string     | Autor del plugin                               |
| `description` | string     | Descripción corta                              |
| `hooks`       | string[]   | Hooks del ciclo de vida que el plugin usa       |
| `enabled`     | boolean    | Si el plugin se carga al iniciar                |

### Hooks disponibles

| Hook             | Cuándo se llama                              | Argumento     |
|------------------|----------------------------------------------|---------------|
| `on_load`        | Al cargar el plugin (inicio o al habilitar)  | —             |
| `on_unload`      | Al cerrar la app o deshabilitar el plugin     | —             |
| `on_frame`       | Cada frame de renderizado (~60 veces/seg)     | —             |
| `on_note_open`   | Al seleccionar/abrir una nota                 | `nota_id`     |
| `on_note_save`   | Al guardar una nota                           | `nota_id`     |
| `on_note_create` | Al crear una nota nueva                       | `nota_id`     |

**Ejemplo mínimo — plugin.lua:**

```lua
function on_load()
    print("[Mi Plugin] Cargado correctamente")

    ui.menu_item("Mi Acción", function()
        ui.notificacion("¡Hola desde mi plugin!", "success")
    end)
end
```

---

## API: `notas`

Acceso al sistema de notas de Node Panda.

### notas.contar() → number
Retorna el número total de notas.

### notas.listar() → table
Retorna una tabla con todos los IDs de notas.

```lua
local ids = notas.listar()
for _, id in ipairs(ids) do
    print(id)
end
```

### notas.contenido(id) → string
Retorna el contenido completo de una nota. Vacío si no existe.

### notas.titulo(id) → string
Retorna el título de una nota. Vacío si no existe.

### notas.enlaces(id) → table
Retorna una tabla con los IDs de notas enlazadas desde la nota dada.

### notas.meta(id, key) → string
Lee un valor del frontmatter de la nota. Vacío si la clave no existe.

```lua
local tipo = notas.meta("mi-nota", "tipo")
```

### notas.seleccionada() → string
Retorna el ID de la nota actualmente seleccionada. Vacío si no hay ninguna.

### notas.existe(id) → boolean
Verifica si una nota con ese ID existe.

### notas.crear(nombre [, contenido]) → boolean
Crea una nueva nota. Opcionalmente establece su contenido inicial. Retorna `true` si se creó correctamente.

```lua
notas.crear("diario-2026-03-20", "# Diario\nHoy fue un buen día.")
```

### notas.editar(id, contenido) → boolean
Reemplaza el contenido de una nota existente. Guarda a disco automáticamente.

### notas.abrir(id)
Selecciona y abre una nota en el editor.

### notas.establecer_meta(id, key, value) → boolean
Establece un valor en el frontmatter de una nota.

```lua
notas.establecer_meta("mi-nota", "tipo", "proyecto")
```

---

## API: `grafo`

Acceso al grafo de conexiones entre notas.

### grafo.nodos() → number
Total de nodos en el grafo.

### grafo.aristas() → number
Total de aristas (enlaces) en el grafo.

### grafo.vecinos(nota_id) → table
Retorna los IDs de notas conectadas directamente (entrantes y salientes).

### grafo.conexiones(nota_id) → number
Retorna el número de conexiones de una nota.

```lua
local n = grafo.conexiones("mi-nota")
print("Esta nota tiene " .. n .. " conexiones")
```

---

## API: `memoria`

Motor de Memoria TF-IDF para búsqueda semántica entre notas.

### memoria.similares(nota_id, topN) → table
Retorna las `topN` notas más similares a la nota dada. Cada entrada tiene:

| Campo    | Tipo   | Descripción             |
|----------|--------|-------------------------|
| `id`     | string | ID de la nota           |
| `titulo` | string | Título de la nota       |
| `score`  | number | Similitud (0.0 a 1.0)  |

```lua
local sims = memoria.similares("mi-nota", 5)
for _, s in ipairs(sims) do
    print(s.titulo .. " → " .. string.format("%.0f%%", s.score * 100))
end
```

### memoria.buscar(query, topN) → table
Busca notas por texto libre. Retorna la misma estructura que `similares`.

```lua
local resultados = memoria.buscar("inteligencia artificial", 10)
```

---

## API: `app`

Funciones generales de la aplicación.

### app.fecha_hoy() → string
Retorna la fecha actual en formato `"YYYY-MM-DD"`.

### app.fps() → number
FPS actual del renderizado.

### app.ram_mb() → number
Uso de memoria RAM en megabytes.

### app.persistir(key, value)
Guarda un par clave-valor en `state.json` del plugin. Solo strings.

```lua
app.persistir("ultimo_uso", app.fecha_hoy())
```

### app.leer_persistido(key) → string
Lee un valor previamente guardado. Vacío si no existe.

```lua
local ultimo = app.leer_persistido("ultimo_uso")
```

### app.guardar_archivo(filename, content) → boolean
Guarda un archivo de texto en `data/`. Útil para exportaciones.

```lua
app.guardar_archivo("export.txt", "contenido del archivo")
-- Crea: data/export.txt
```

---

## API: `ui`

Registro de elementos de interfaz del plugin.

### ui.menu_item(label, callback)
Registra una entrada en el menú "Plugins". El callback se ejecuta al hacer clic.

```lua
ui.menu_item("Mi Acción", function()
    print("Ejecutando acción")
end)
```

### ui.panel(nombre, render_fn)
Registra un panel acoplable (dockable) en la interfaz. La función `render_fn` se llama cada frame para renderizar el contenido usando la API `imgui`.

```lua
ui.panel("Mi Panel", function()
    imgui.texto("Hola desde el panel")
    if imgui.boton("Click") then
        ui.notificacion("¡Click!", "info")
    end
end)
```

### ui.notificacion(text [, type])
Muestra un toast de notificación. Tipos: `"info"`, `"warn"`, `"error"`, `"success"`.

### ui.tema(colors_table)
Aplica un tema de colores a la interfaz. La tabla usa valores hexadecimales de 6 dígitos.

```lua
ui.tema({
    bg           = "282828",
    bg_child     = "1D2021",
    bg_popup     = "3C3836",
    text         = "EBDBB2",
    text_dim     = "928374",
    border       = "504945",
    frame_bg     = "3C3836",
    title_bg     = "282828",
    menu_bg      = "1D2021",
    button       = "504945",
    button_hover = "689D6A",
    header       = "504945",
    header_hover = "689D6A",
    tab          = "1D2021",
    tab_hover    = "689D6A",
    tab_active   = "3C3836",
    accent       = "B8BB26",
    separator    = "504945",
    scrollbar    = "665C54",
})
```

---

## API: `imgui`

Bindings de Dear ImGui para renderizar UI dentro de paneles y callbacks. Estas funciones solo deben usarse dentro de `ui.panel()` o callbacks de menú.

### Texto

| Función                                      | Descripción                           |
|----------------------------------------------|---------------------------------------|
| `imgui.texto(text)`                          | Texto simple                          |
| `imgui.texto_color(r,g,b,a, text)`          | Texto con color RGBA (0.0–1.0)       |
| `imgui.texto_wrapped(text)`                  | Texto con ajuste de línea            |
| `imgui.texto_disabled(text)`                 | Texto atenuado (gris)                |
| `imgui.bullet_text(text)`                    | Texto con viñeta                     |

### Botones e Interacción

| Función                                      | Retorno  | Descripción                     |
|----------------------------------------------|----------|---------------------------------|
| `imgui.boton(label)`                         | boolean  | Botón estándar                  |
| `imgui.boton_pequeno(label)`                 | boolean  | Botón pequeño (inline)          |
| `imgui.selectable(label, selected)`          | boolean  | Elemento seleccionable en lista |

### Layout

| Función                                      | Descripción                                |
|----------------------------------------------|--------------------------------------------|
| `imgui.separador()`                          | Línea horizontal separadora                |
| `imgui.espacio()`                            | Espacio vertical                           |
| `imgui.misma_linea([offset])`               | Siguiente elemento en la misma línea       |
| `imgui.nueva_linea()`                        | Forzar nueva línea                         |
| `imgui.indent([width])`                      | Aumentar indentación                       |
| `imgui.unindent([width])`                    | Reducir indentación                        |
| `imgui.bullet()`                             | Viñeta (sin texto)                         |
| `imgui.ancho_disponible()`                   | Ancho disponible en píxeles (float)        |
| `imgui.alto_disponible()`                    | Alto disponible en píxeles (float)         |

### Contenedores

| Función                                      | Retorno  | Descripción                        |
|----------------------------------------------|----------|------------------------------------|
| `imgui.begin_child(id [,w] [,h])`           | boolean  | Inicia una región hijo scrollable  |
| `imgui.end_child()`                          | —        | Cierra región hijo                 |
| `imgui.tree_node(label)`                     | boolean  | Nodo expandible de árbol           |
| `imgui.tree_pop()`                           | —        | Cierra nodo de árbol               |
| `imgui.collapsing_header(label)`             | boolean  | Encabezado colapsable              |

### Columnas

| Función                                      | Descripción                      |
|----------------------------------------------|----------------------------------|
| `imgui.columns(count [, id])`               | Iniciar layout de columnas       |
| `imgui.next_column()`                        | Avanzar a la siguiente columna   |
| `imgui.columns_end()`                        | Finalizar layout de columnas     |

### Tabs

| Función                                      | Retorno  | Descripción                 |
|----------------------------------------------|----------|-----------------------------|
| `imgui.tab_bar_begin(id)`                    | boolean  | Iniciar barra de tabs       |
| `imgui.tab_item_begin(label)`                | boolean  | Iniciar un tab              |
| `imgui.tab_item_end()`                       | —        | Cerrar un tab               |
| `imgui.tab_bar_end()`                        | —        | Cerrar barra de tabs        |

### Visual

| Función                                      | Descripción                              |
|----------------------------------------------|------------------------------------------|
| `imgui.progress_bar(fraction [, overlay])`   | Barra de progreso (0.0–1.0)             |
| `imgui.tooltip(text)`                        | Tooltip al pasar el mouse sobre el ítem previo |
| `imgui.push_color(idx, r, g, b, a)`         | Cambiar color del estilo (ver ImGuiCol_) |
| `imgui.pop_color([count])`                   | Restaurar color(es) del estilo           |

**Nota sobre `push_color`/`pop_color`:** El parámetro `idx` corresponde a los índices de ImGuiCol. Siempre llama `pop_color` después de `push_color`.

---

## Sandbox y Seguridad

Los plugins se ejecutan en un entorno Lua aislado con las siguientes restricciones:

**Funciones deshabilitadas:** `dofile`, `loadfile`, `rawget`, `rawset`, `rawequal`, `rawlen`, `collectgarbage`, `require`.

**Módulos deshabilitados:** `os`, `io`, `package`.

**Módulos disponibles:** `base` (sin las funciones peligrosas), `string`, `table`, `math`, `utf8`.

**Aislamiento:** Cada plugin tiene su propio `lua_State` independiente. Un plugin no puede acceder al estado de otro.

**Auto-desactivación:** Si un plugin genera 3 errores consecutivos en `on_frame`, se desactiva automáticamente y se muestra una notificación.

---

## Ejemplos Completos

### Plugin: Contador de palabras simple

```lua
function on_load()
    ui.panel("Palabras", function()
        local id = notas.seleccionada()
        if id == "" then
            imgui.texto_disabled("Selecciona una nota")
            return
        end
        local contenido = notas.contenido(id)
        local _, words = contenido:gsub("%S+", "")
        imgui.texto("Palabras: " .. words)
    end)
end
```

### Plugin: Exportar lista de notas

```lua
function on_load()
    ui.menu_item("Exportar Lista de Notas", function()
        local ids = notas.listar()
        local lines = {}
        for _, id in ipairs(ids) do
            local t = notas.titulo(id)
            table.insert(lines, (t ~= "" and t or id))
        end
        local ok = app.guardar_archivo(
            "lista_notas_" .. app.fecha_hoy() .. ".txt",
            table.concat(lines, "\n")
        )
        if ok then
            ui.notificacion("Lista exportada", "success")
        end
    end)
end
```

### Plugin: Nota diaria automática

```lua
function on_load()
    local hoy = "diario-" .. app.fecha_hoy()
    if not notas.existe(hoy) then
        notas.crear(hoy, "# Diario — " .. app.fecha_hoy() .. "\n\n")
        ui.notificacion("Nota diaria creada", "info")
    end
    notas.abrir(hoy)
end
```

---

## Plugins Incluidos

| Plugin            | Descripción                                           |
|-------------------|-------------------------------------------------------|
| `daily_notes`     | Crea y abre nota diaria automáticamente               |
| `stats_dashboard` | Estadísticas del vault en consola Lua                  |
| `templates`       | 5 plantillas predefinidas (Reunión, Proyecto, etc.)   |
| `tag_browser`     | Panel para explorar y filtrar notas por tags           |
| `word_count`      | Panel con estadísticas de la nota activa               |
| `auto_linker`     | Sugiere links basados en similitud TF-IDF              |
| `graph_exporter`  | Exporta grafo a DOT (Graphviz) y JSON                  |
| `theme_switcher`  | 5 temas de colores: Default, Nord, Catppuccin, Gruvbox, Dracula |
| `ai_assistant`    | Lanzador de misiones IA y panel de estado de agentes   |
| `outliner_tools`  | Operaciones rápidas sobre bloques jerárquicos          |
| `task_radar`      | Monitor de tareas pendientes y vencidas                |
| `db_monitor`      | Estado y sincronización rápida con PostgreSQL / MySQL  |

---

## APIs Adicionales v2.0

### `bloques`
- `bloques.contar() -> number`
- `bloques.crear(page_id, contenido) -> string`

### `tareas`
- `tareas.contar() -> number`
- `tareas.pendientes() -> number`
- `tareas.vencidas() -> number`
- `tareas.crear(page_id, titulo) -> string`

### `agentes`
- `agentes.misiones() -> number`
- `agentes.ejecutar(prompt [, model]) -> string`

### `db`
- `db.motor() -> string`
- `db.conectado() -> boolean`
- `db.sincronizar() -> string`
