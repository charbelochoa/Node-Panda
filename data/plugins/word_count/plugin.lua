local last_note_id = ""
local stats = {
    words = 0,
    chars = 0,
    lines = 0,
    links = 0,
    read_min = 0,
}

local function actualizar(nota_id)
    if nota_id == "" then
        stats = { words = 0, chars = 0, lines = 0, links = 0, read_min = 0 }
        return
    end

    local contenido = notas.contenido(nota_id)
    if contenido == "" then
        stats = { words = 0, chars = 0, lines = 0, links = 0, read_min = 0 }
        return
    end

    local words = 0
    for _ in contenido:gmatch("%S+") do
        words = words + 1
    end

    local chars = #contenido:gsub("%s", "")

    local lines = 1
    for _ in contenido:gmatch("\n") do
        lines = lines + 1
    end

    local link_count = 0
    for _ in contenido:gmatch("%[%[.-%]%]") do
        link_count = link_count + 1
    end

    local minutes = math.ceil(words / 200)
    if minutes < 1 and words > 0 then minutes = 1 end

    stats.words    = words
    stats.chars    = chars
    stats.lines    = lines
    stats.links    = link_count
    stats.read_min = minutes
end

function on_load()
    print("[Word Count] Plugin activo")

    ui.panel("Word Count", function()
        local nota_id = notas.seleccionada()

        if nota_id == "" then
            imgui.texto_disabled("Ninguna nota seleccionada")
            return
        end

        local titulo = notas.titulo(nota_id)
        if titulo == "" then titulo = nota_id end
        imgui.texto_color(0.28, 0.82, 0.76, 1.0, titulo)
        imgui.separador()
        imgui.espacio()

        imgui.columns(2, "##wc_cols")

        imgui.texto_color(0.45, 0.46, 0.58, 1.0, "Palabras")
        imgui.next_column()
        imgui.texto("" .. stats.words)
        imgui.next_column()

        imgui.texto_color(0.45, 0.46, 0.58, 1.0, "Caracteres")
        imgui.next_column()
        imgui.texto("" .. stats.chars)
        imgui.next_column()

        imgui.texto_color(0.45, 0.46, 0.58, 1.0, "Lineas")
        imgui.next_column()
        imgui.texto("" .. stats.lines)
        imgui.next_column()

        imgui.texto_color(0.45, 0.46, 0.58, 1.0, "Enlaces")
        imgui.next_column()
        imgui.texto("" .. stats.links)
        imgui.next_column()

        imgui.separador()

        imgui.texto_color(0.45, 0.46, 0.58, 1.0, "Lectura")
        imgui.next_column()
        imgui.texto_color(0.36, 0.72, 0.48, 1.0, "~" .. stats.read_min .. " min")
        imgui.next_column()

        imgui.columns_end()

        imgui.espacio()
        local progress = math.min(stats.words / 1000, 1.0)
        imgui.progress_bar(progress, stats.words .. " / 1000 palabras")
    end)
end

function on_frame()
    local nota_id = notas.seleccionada()

    if nota_id ~= last_note_id then
        last_note_id = nota_id
        actualizar(nota_id)
    end
end
