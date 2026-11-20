local active_note = ""
local summary_lines = {}
local word_total = 0
local link_total = 0

local function analyze_note(note_id)
    summary_lines = {}
    word_total = 0
    link_total = 0
    if note_id == "" then return end

    local body = notas.contenido(note_id)
    for _ in body:gmatch("%S+") do
        word_total = word_total + 1
    end

    local links = notas.enlaces(note_id)
    link_total = #links

    for line in body:gmatch("[^\r\n]+") do
        if line:match("^#+%s+") then
            table.insert(summary_lines, line)
        end
    end
end

local function create_brief_note(note_id)
    if note_id == "" then return end
    local title = notas.titulo(note_id)
    if title == "" then title = note_id end
    local brief_name = "Brief_" .. note_id
    local lines = {
        "# Resumen IA: " .. title,
        "",
        "Fecha: " .. app.fecha_hoy(),
        "Origen: [[" .. note_id .. "]]",
        "Palabras: " .. tostring(word_total),
        "Enlaces salientes: " .. tostring(link_total),
        "",
        "## Estructura detectada"
    }
    if #summary_lines == 0 then
        table.insert(lines, "- Sin encabezados Markdown.")
    else
        for _, h in ipairs(summary_lines) do
            table.insert(lines, "- " .. h)
        end
    end
    local content = table.concat(lines, "\n")
    if notas.existe(brief_name) then
        notas.editar(brief_name, content)
    else
        notas.crear(brief_name, content)
    end
    ui.notificacion("Resumen generado: " .. brief_name, "success")
end

function on_load()
    active_note = notas.seleccionada()
    analyze_note(active_note)

    ui.panel("AI Assistant", function()
        local current = notas.seleccionada()
        if current ~= active_note then
            active_note = current
            analyze_note(active_note)
        end

        if active_note == "" then
            imgui.texto_disabled("Selecciona una nota para analizar.")
            return
        end

        imgui.texto_color(0.32, 0.78, 0.96, 1.0, "Nota activa: " .. active_note)
        imgui.texto("Palabras: " .. tostring(word_total) .. " | Enlaces: " .. tostring(link_total))
        imgui.espacio()

        if imgui.boton("Generar Nota Resumen") then
            create_brief_note(active_note)
        end
        imgui.misma_linea()
        if imgui.boton("Analizar") then
            analyze_note(active_note)
        end

        imgui.separador()
        imgui.texto("Encabezados:")
        if #summary_lines == 0 then
            imgui.texto_disabled("No se encontraron encabezados.")
        else
            for _, h in ipairs(summary_lines) do
                imgui.bullet_text(h)
            end
        end

        imgui.separador()
        local sims = memoria.similares(active_note, 4)
        imgui.texto("Contexto semantico cercano:")
        if sims and #sims > 0 then
            for _, s in ipairs(sims) do
                if imgui.selectable(s.id .. " (" .. string.format("%.0f%%", s.score * 100) .. ")", false) then
                    notas.abrir(s.id)
                end
            end
        else
            imgui.texto_disabled("Sin vecinos semanticos.")
        end
    end)

    ui.menu_item("AI Assistant: Generar Brief", function()
        local current = notas.seleccionada()
        analyze_note(current)
        create_brief_note(current)
    end)
end

function on_note_open(note_id)
    active_note = note_id
    analyze_note(note_id)
end
