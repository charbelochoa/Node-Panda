local function exportar_dot()
    local ids = notas.listar()
    local lines = {}

    table.insert(lines, "digraph NodePanda {")
    table.insert(lines, '  rankdir=LR;')
    table.insert(lines, '  bgcolor="#121219";')
    table.insert(lines, '  node [shape=box, style="filled,rounded", fillcolor="#1C1C2B",')
    table.insert(lines, '        fontcolor="#D0D1E0", fontname="Arial", fontsize=10,')
    table.insert(lines, '        color="#28B2AC"];')
    table.insert(lines, '  edge [color="#4EC9E8", arrowsize=0.6];')
    table.insert(lines, '')

    for _, id in ipairs(ids) do
        local titulo = notas.titulo(id)
        if titulo == "" then titulo = id end
        local tipo = notas.meta(id, "tipo")
        local extra = ""
        if tipo ~= "" then
            extra = ', xlabel="' .. tipo .. '"'
        end
        local conex = grafo.conexiones(id)

        local width = 1.0 + (conex * 0.2)
        table.insert(lines, '  "' .. id .. '" [label="' .. titulo .. '"'
            .. ', width=' .. string.format("%.1f", width) .. extra .. '];')
    end

    table.insert(lines, '')

    local edges_seen = {}
    for _, id in ipairs(ids) do
        local enlaces = notas.enlaces(id)
        for _, target in ipairs(enlaces) do
            local key = id .. "->" .. target
            if not edges_seen[key] then
                edges_seen[key] = true
                table.insert(lines, '  "' .. id .. '" -> "' .. target .. '";')
            end
        end
    end

    table.insert(lines, '}')
    return table.concat(lines, "\n")
end

local function exportar_json()
    local ids = notas.listar()
    local lines = {}

    table.insert(lines, '{')
    table.insert(lines, '  "nodes": [')

    for i, id in ipairs(ids) do
        local titulo = notas.titulo(id)
        if titulo == "" then titulo = id end
        local tipo = notas.meta(id, "tipo")
        local conex = grafo.conexiones(id)
        local comma = (i < #ids) and "," or ""

        table.insert(lines, '    {')
        table.insert(lines, '      "id": "' .. id .. '",')
        table.insert(lines, '      "title": "' .. titulo .. '",')
        table.insert(lines, '      "type": "' .. tipo .. '",')
        table.insert(lines, '      "connections": ' .. conex)
        table.insert(lines, '    }' .. comma)
    end

    table.insert(lines, '  ],')
    table.insert(lines, '  "edges": [')

    local all_edges = {}
    for _, id in ipairs(ids) do
        local enlaces = notas.enlaces(id)
        for _, target in ipairs(enlaces) do
            table.insert(all_edges, { from = id, to = target })
        end
    end

    for i, edge in ipairs(all_edges) do
        local comma = (i < #all_edges) and "," or ""
        table.insert(lines, '    { "from": "' .. edge.from .. '", "to": "' .. edge.to .. '" }' .. comma)
    end

    table.insert(lines, '  ],')
    table.insert(lines, '  "metadata": {')
    table.insert(lines, '    "total_nodes": ' .. grafo.nodos() .. ',')
    table.insert(lines, '    "total_edges": ' .. grafo.aristas() .. ',')
    table.insert(lines, '    "exported_at": "' .. app.fecha_hoy() .. '"')
    table.insert(lines, '  }')
    table.insert(lines, '}')

    return table.concat(lines, "\n")
end

function on_load()
    print("[Graph Exporter] Plugin activo")

    ui.menu_item("Exportar Grafo (.dot)", function()
        local dot = exportar_dot()
        local filename = "grafo_export_" .. app.fecha_hoy() .. ".dot"
        local ok = app.guardar_archivo(filename, dot)
        if ok then
            ui.notificacion("Grafo exportado: data/" .. filename, "success")
            print("[Graph Exporter] Exportado DOT: " .. filename)
        else
            ui.notificacion("Error exportando grafo DOT", "error")
        end
    end)

    ui.menu_item("Exportar Grafo (.json)", function()
        local json = exportar_json()
        local filename = "grafo_export_" .. app.fecha_hoy() .. ".json"
        local ok = app.guardar_archivo(filename, json)
        if ok then
            ui.notificacion("Grafo exportado: data/" .. filename, "success")
            print("[Graph Exporter] Exportado JSON: " .. filename)
        else
            ui.notificacion("Error exportando grafo JSON", "error")
        end
    end)
end
