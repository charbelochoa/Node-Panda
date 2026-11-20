local snap = {
    notes = 0,
    nodes = 0,
    edges = 0,
    fps = 0,
    ram = 0,
    density = 0,
    checked_at = ""
}
local tick = 0

local function refresh_metrics()
    snap.notes = notas.contar()
    snap.nodes = grafo.nodos()
    snap.edges = grafo.aristas()
    snap.fps = app.fps()
    snap.ram = app.ram_mb()
    if snap.nodes > 1 then
        snap.density = (2.0 * snap.edges) / (snap.nodes * (snap.nodes - 1))
    else
        snap.density = 0.0
    end
    snap.checked_at = app.fecha_hoy()
end

function on_load()
    refresh_metrics()

    ui.panel("DB Monitor", function()
        imgui.texto_color(0.34, 0.86, 0.68, 1.0, "Telemetria del Workspace")
        imgui.separador()
        imgui.texto("Notas indexadas: " .. tostring(snap.notes))
        imgui.texto("Nodos de grafo: " .. tostring(snap.nodes))
        imgui.texto("Aristas de grafo: " .. tostring(snap.edges))
        imgui.texto(string.format("Densidad: %.4f", snap.density))
        imgui.separador()
        imgui.texto(string.format("FPS: %.1f", snap.fps))
        imgui.texto(string.format("Memoria RAM: %.1f MB", snap.ram))
        imgui.texto_disabled("Fecha: " .. snap.checked_at)
        imgui.espacio()

        if imgui.boton("Actualizar") then
            refresh_metrics()
        end
        imgui.misma_linea()
        if imgui.boton("Guardar Snapshot") then
            refresh_metrics()
            app.persistir("last_notes", tostring(snap.notes))
            app.persistir("last_edges", tostring(snap.edges))
            ui.notificacion("Snapshot guardado (" .. tostring(snap.notes) .. " notas)", "success")
        end
    end)

    ui.menu_item("DB Monitor: Snapshot", function()
        refresh_metrics()
        app.persistir("last_notes", tostring(snap.notes))
        ui.notificacion("Snapshot persistido", "info")
    end)
end

function on_frame()
    tick = tick + 1
    if tick >= 180 then
        tick = 0
        refresh_metrics()
    end
end
