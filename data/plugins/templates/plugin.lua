local templates = {}

templates["Reunion"] = {
    frontmatter = "tipo: reunion\ntags: reunion, meeting\nfecha: %FECHA%\nasistentes: ",
    body = [[
# Reunion - %TITULO%

## Asistentes
-

## Agenda
1.
2.
3.

## Notas
-

## Decisiones
- [ ]

## Proximos pasos
- [ ]
- [ ]
]]
}

templates["Proyecto"] = {
    frontmatter = "tipo: proyecto\ntags: proyecto\nfecha: %FECHA%\nestado: activo",
    body = [[
# Proyecto: %TITULO%

## Objetivo

## Contexto y motivacion

## Alcance
- Incluye:
- Excluye:

## Tareas principales
- [ ] Fase 1:
- [ ] Fase 2:
- [ ] Fase 3:

## Recursos
-

## Notas relacionadas
- [[]]
]]
}

templates["Concepto"] = {
    frontmatter = "tipo: concepto\ntags: concepto, conocimiento\nfecha: %FECHA%",
    body = [[
# %TITULO%

## Definicion

## Contexto

## Puntos clave
-
-
-

## Ejemplos

## Relacion con otros conceptos
- [[]]

## Referencias
-
]]
}

templates["Tarea"] = {
    frontmatter = "tipo: tarea\ntags: tarea, todo\nfecha: %FECHA%\nprioridad: media\nestado: pendiente\nfecha_limite: ",
    body = [[
# Tarea: %TITULO%

## Descripcion

## Criterios de completado
- [ ]
- [ ]

## Dependencias
-

## Notas de progreso

### %FECHA%
- Creada
]]
}

templates["Diario Detallado"] = {
    frontmatter = "tipo: diario\ntags: diario, reflexion\nfecha: %FECHA%\nhumor: ",
    body = [[
# Diario - %FECHA%

## Lo mas importante de hoy

## Logros
-

## Desafios
-

## Ideas y reflexiones

## Gratitud
-

## Plan para manana
- [ ]
- [ ]
]]
}

local function aplicar_plantilla(nombre_template, nota_id)
    local tmpl = templates[nombre_template]
    if not tmpl then
        ui.notificacion("Plantilla no encontrada: " .. nombre_template, "error")
        return false
    end

    local hoy = app.fecha_hoy()
    local titulo = nota_id

    local fm = tmpl.frontmatter
    fm = fm:gsub("%%FECHA%%", hoy)
    fm = fm:gsub("%%TITULO%%", titulo)

    local body = tmpl.body
    body = body:gsub("%%FECHA%%", hoy)
    body = body:gsub("%%TITULO%%", titulo)

    local contenido = "---\n" .. fm .. "\n---\n\n" .. body

    local ok = notas.editar(nota_id, contenido)
    if ok then
        ui.notificacion("Plantilla '" .. nombre_template .. "' aplicada a " .. nota_id, "success")
        print("[Templates] Aplicada '" .. nombre_template .. "' a " .. nota_id)
    end
    return ok
end

function on_load()
    print("[Templates] Plugin cargado con " .. 5 .. " plantillas")

    for nombre, _ in pairs(templates) do
        ui.menu_item("Nueva: " .. nombre, function()
            local id = notas.seleccionada()
            if id == "" then
                ui.notificacion("Selecciona una nota primero", "warn")
                return
            end
            aplicar_plantilla(nombre, id)
        end)
    end
end

function on_note_create(nota_id)
    print("[Templates] Nota creada: " .. nota_id .. " — usa menu Plugins para aplicar plantilla")
end
