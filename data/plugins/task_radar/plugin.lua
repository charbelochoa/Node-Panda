local open_tasks = {}
local done_count = 0
local frame_tick = 0

local function scan_tasks()
    open_tasks = {}
    done_count = 0
    local ids = notas.listar()
    for _, id in ipairs(ids) do
        local content = notas.contenido(id)
        for line in content:gmatch("[^\r\n]+") do
            local pending = line:match("^%s*[%-%*]%s+%[%s%]%s+(.+)$")
            if pending then
                table.insert(open_tasks, { note = id, text = pending })
            elseif line:match("^%s*[%-%*]%s+%[[xX]%]%s+") then
                done_count = done_count + 1
            end
        end
    end
end

function on_load()
    scan_tasks()

    ui.panel("Task Radar", function()
        local total = #open_tasks + done_count
        local ratio = 0.0
        if total > 0 then
            ratio = done_count / total
        end

        imgui.texto("Pendientes: " .. tostring(#open_tasks) .. " | Completadas: " .. tostring(done_count))
        imgui.progress_bar(ratio, string.format("%.0f%% completado", ratio * 100))

        if imgui.boton_pequeno("Escanear Vault") then
            scan_tasks()
            ui.notificacion("Task Radar actualizado: " .. tostring(#open_tasks) .. " pendientes", "info")
        end

        imgui.separador()
        if #open_tasks == 0 then
            imgui.texto_disabled("No hay tareas pendientes.")
            return
        end

        for i, task in ipairs(open_tasks) do
            if i > 60 then break end
            imgui.texto_color(0.95, 0.72, 0.30, 1.0, "[" .. task.note .. "]")
            imgui.misma_linea()
            if imgui.selectable(task.text .. "##t" .. tostring(i), false) then
                notas.abrir(task.note)
            end
        end
    end)

    ui.menu_item("Task Radar: Escanear", function()
        scan_tasks()
        ui.notificacion("Tareas pendientes: " .. tostring(#open_tasks), "info")
    end)
end

function on_frame()
    frame_tick = frame_tick + 1
    if frame_tick >= 420 then
        frame_tick = 0
        scan_tasks()
    end
end
