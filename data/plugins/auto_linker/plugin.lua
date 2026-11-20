local suggestions = {}
local current_note = ""
local needs_update = false
local MIN_SCORE = 0.05

local function actualizar_sugerencias(nota_id)
    suggestions = {}
    if nota_id == "" then return end

    local sims = memoria.similares(nota_id, 8)
    if not sims then return end

    local links_existentes = {}
    local enlaces = notas.enlaces(nota_id)
    for _, link in ipairs(enlaces) do
        links_existentes[link] = true
    end

    for _, entry in ipairs(sims) do
        if entry.score >= MIN_SCORE and not links_existentes[entry.id] then
            table.insert(suggestions, {
                id     = entry.id,
                titulo = entry.titulo,
                score  = entry.score,
            })
        end
    end

    needs_update = false
end

local function insertar_link(nota_id, target_id)
    local contenido = notas.contenido(nota_id)
    if contenido == "" then return end

    local nuevo = contenido .. "\n- [[" .. target_id .. "]]"
    notas.editar(nota_id, nuevo)
    ui.notificacion("Link insertado: [[" .. target_id .. "]]", "success")
    print("[Auto-Linker] Link insertado: " .. nota_id .. " -> " .. target_id)

    needs_update = true
end

local function score_color(score)
    if score >= 0.3 then
        return 0.36, 0.88, 0.55, 1.0
    elseif score >= 0.15 then
        return 0.90, 0.72, 0.42, 1.0
    else
        return 0.55, 0.56, 0.68, 1.0
    end
end

function on_load()
    print("[Auto-Linker] Plugin activo — sugiere links basados en TF-IDF")

    ui.panel("Sugerencias de Links", function()
        local nota_id = notas.seleccionada()

        if nota_id == "" then
            imgui.texto_disabled("Selecciona una nota para ver sugerencias")
            return
        end

        local titulo = notas.titulo(nota_id)
        if titulo == "" then titulo = nota_id end
        imgui.texto_color(0.28, 0.82, 0.76, 1.0, "Links sugeridos para:")
        imgui.texto(titulo)

        imgui.misma_linea()
        if imgui.boton_pequeno("Refrescar") then
            needs_update = true
        end

        imgui.separador()

        if #suggestions == 0 then
            imgui.espacio()
            imgui.texto_disabled("No hay sugerencias disponibles.")
            imgui.texto_disabled("Puede que la nota no tenga suficiente")
            imgui.texto_disabled("contenido o ya este bien enlazada.")
            return
        end

        for i, sug in ipairs(suggestions) do
            local r, g, b, a = score_color(sug.score)

            local score_str = string.format("%.0f%%", sug.score * 100)
            imgui.texto_color(r, g, b, a, score_str)

            imgui.misma_linea(60)

            if imgui.selectable(sug.titulo ~= "" and sug.titulo or sug.id, false) then
                notas.abrir(sug.id)
            end

            imgui.tooltip("Nota: " .. sug.id)

            imgui.misma_linea(imgui.ancho_disponible() - 50)
            if imgui.boton_pequeno("+ Link##" .. sug.id) then
                insertar_link(nota_id, sug.id)
            end
        end
    end)

    ui.menu_item("Refrescar sugerencias", function()
        needs_update = true
        ui.notificacion("Sugerencias actualizadas", "info")
    end)
end

function on_note_open(nota_id)
    current_note = nota_id
    needs_update = true
end

function on_frame()
    local nota_id = notas.seleccionada()
    if nota_id ~= current_note then
        current_note = nota_id
        needs_update = true
    end

    if needs_update then
        actualizar_sugerencias(current_note)
    end
end
