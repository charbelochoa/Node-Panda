local stats = {
    total_notas    = 0,
    total_palabras = 0,
    total_nodos    = 0,
    total_aristas  = 0,
    nota_mas_conectada = "",
    max_conexiones = 0,
    notas_huerfanas = 0,
    tipos = {},
    last_update = ""
}

local function calcular_stats()
    local ids = notas.listar()
    stats.total_notas = #ids
    stats.total_palabras = 0
    stats.tipos = {}
    stats.notas_huerfanas = 0

    for _, id in ipairs(ids) do

        local contenido = notas.contenido(id)
        local palabras = 0
        for _ in contenido:gmatch("%S+") do
            palabras = palabras + 1
        end
        stats.total_palabras = stats.total_palabras + palabras

        local tipo = notas.meta(id, "tipo")
        if tipo ~= "" then
            stats.tipos[tipo] = (stats.tipos[tipo] or 0) + 1
        end

        local conex = grafo.conexiones(id)
        if conex > stats.max_conexiones then
            stats.max_conexiones = conex
            stats.nota_mas_conectada = id
        end

        if conex == 0 then
            stats.notas_huerfanas = stats.notas_huerfanas + 1
        end
    end

    stats.total_nodos   = grafo.nodos()
    stats.total_aristas = grafo.aristas()
    stats.last_update   = app.fecha_hoy()
end

function on_load()
    calcular_stats()

    print("=== Stats Dashboard ===")
    print("Notas: " .. stats.total_notas)
    print("Palabras totales: " .. stats.total_palabras)
    print("Nodos en grafo: " .. stats.total_nodos)
    print("Aristas: " .. stats.total_aristas)

    if stats.nota_mas_conectada ~= "" then
        print("Nota mas conectada: " .. stats.nota_mas_conectada
              .. " (" .. stats.max_conexiones .. " conexiones)")
    end

    print("Notas huerfanas: " .. stats.notas_huerfanas)

    for tipo, count in pairs(stats.tipos) do
        print("  Tipo '" .. tipo .. "': " .. count)
    end

    ui.menu_item("Refrescar Stats", function()
        calcular_stats()
        ui.notificacion("Stats actualizados: " .. stats.total_notas .. " notas, "
                        .. stats.total_palabras .. " palabras", "info")
        print("Stats refrescados: " .. stats.total_notas .. " notas")
    end)
end

local frame_counter = 0

function on_frame()
    frame_counter = frame_counter + 1

    if frame_counter >= 300 then
        frame_counter = 0
        stats.total_notas   = notas.contar()
        stats.total_nodos   = grafo.nodos()
        stats.total_aristas = grafo.aristas()
    end
end
