function on_load()
    local hoy = app.fecha_hoy()
    local id  = "diario-" .. hoy

    if not notas.existe(id) then

        local total = notas.contar()

        local contenido = "---\n"
            .. "tipo: diario\n"
            .. "fecha: " .. hoy .. "\n"
            .. "tags: diario, daily\n"
            .. "---\n\n"
            .. "# Diario " .. hoy .. "\n\n"
            .. "## Notas en el vault: " .. total .. "\n\n"
            .. "## Tareas del dia\n\n"
            .. "- [ ] \n\n"
            .. "## Ideas y reflexiones\n\n"
            .. "\n\n"
            .. "## Enlaces rapidos\n\n"
            .. "- Nota anterior: buscar en el explorador\n"

        local ok = notas.crear(id, contenido)
        if ok then
            print("Daily Notes: Creada nota " .. id)
            ui.notificacion("Nota diaria creada: " .. hoy, "success")
        else
            print("Daily Notes: Error creando nota " .. id)
        end
    else
        print("Daily Notes: Nota de hoy ya existe (" .. id .. ")")
    end

    notas.abrir(id)

    ui.menu_item("Abrir nota de hoy", function()
        notas.abrir("diario-" .. app.fecha_hoy())
    end)
end
