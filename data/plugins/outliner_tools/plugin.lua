local current_id = ""
local outline_items = {}
local bullet_count = 0

local function parse_outline(note_id)
    outline_items = {}
    bullet_count = 0
    if note_id == "" then return end

    local content = notas.contenido(note_id)
    local line_num = 0
    for line in content:gmatch("[^\r\n]+") do
        line_num = line_num + 1
        local hashes, heading = line:match("^(#+)%s+(.+)$")
        if hashes and heading then
            table.insert(outline_items, {
                level = #hashes,
                text = heading,
                line = line_num
            })
        elseif line:match("^%s*[%-%*]%s+") then
            bullet_count = bullet_count + 1
        end
    end
end

local function append_section(note_id)
    if note_id == "" then return end
    local content = notas.contenido(note_id)
    local addition = "\n\n## Nueva Seccion (" .. app.fecha_hoy() .. ")\n- Punto clave\n"
    notas.editar(note_id, content .. addition)
    parse_outline(note_id)
    ui.notificacion("Seccion insertada en " .. note_id, "info")
end

function on_load()
    current_id = notas.seleccionada()
    parse_outline(current_id)

    ui.panel("Outliner Tools", function()
        local sel = notas.seleccionada()
        if sel ~= current_id then
            current_id = sel
            parse_outline(current_id)
        end

        if current_id == "" then
            imgui.texto_disabled("Abre una nota para ver su esquema.")
            return
        end

        imgui.texto("Esquema: " .. current_id)
        imgui.texto_disabled("Encabezados: " .. tostring(#outline_items) .. " | Elementos de lista: " .. tostring(bullet_count))
        if imgui.boton_pequeno("Agregar Seccion") then
            append_section(current_id)
        end
        imgui.misma_linea()
        if imgui.boton_pequeno("Refrescar") then
            parse_outline(current_id)
        end

        imgui.separador()
        if #outline_items == 0 then
            imgui.texto_disabled("Sin encabezados en esta nota.")
            return
        end

        for _, item in ipairs(outline_items) do
            local pad = (item.level - 1) * 14
            if pad > 0 then imgui.indent(pad) end
            imgui.texto("L" .. tostring(item.line) .. "  " .. item.text)
            if pad > 0 then imgui.unindent(pad) end
        end
    end)

    ui.menu_item("Outliner: Insertar Seccion", function()
        append_section(notas.seleccionada())
    end)
end

function on_note_open(note_id)
    current_id = note_id
    parse_outline(note_id)
end
