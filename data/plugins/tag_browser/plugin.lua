local tag_index = {}
local tag_list = {}
local selected_tag = ""
local refresh_counter = 0
local needs_refresh = true

local function indexar_tags()
    tag_index = {}
    tag_list = {}

    local ids = notas.listar()
    for _, id in ipairs(ids) do
        local tags_str = notas.meta(id, "tags")
        if tags_str ~= "" then
            for tag in tags_str:gmatch("[^,]+") do
                tag = tag:match("^%s*(.-)%s*$")
                if tag ~= "" then
                    if not tag_index[tag] then
                        tag_index[tag] = {}
                    end
                    table.insert(tag_index[tag], id)
                end
            end
        end
    end

    for tag, ids_list in pairs(tag_index) do
        table.insert(tag_list, { tag = tag, count = #ids_list })
    end
    table.sort(tag_list, function(a, b) return a.count > b.count end)

    needs_refresh = false
end

function on_load()
    indexar_tags()

    print("[Tag Browser] Indexados " .. #tag_list .. " tags unicos")

    ui.panel("Tag Browser", function()

        if imgui.boton_pequeno("Reindexar Tags") then
            needs_refresh = true
        end

        imgui.misma_linea()
        imgui.texto_disabled(#tag_list .. " tags unicos")

        imgui.separador()

        if imgui.begin_child("##tag_list", 0, 200) then
            for _, entry in ipairs(tag_list) do
                local label = entry.tag .. " (" .. entry.count .. ")"
                local is_selected = (entry.tag == selected_tag)

                if imgui.selectable(label, is_selected) then
                    if selected_tag == entry.tag then
                        selected_tag = ""
                    else
                        selected_tag = entry.tag
                    end
                end
            end
        end
        imgui.end_child()

        imgui.separador()

        if selected_tag ~= "" then
            imgui.texto_color(0.28, 0.82, 0.76, 1.0, "Notas con tag: " .. selected_tag)
            imgui.espacio()

            local filtered = tag_index[selected_tag] or {}
            if imgui.begin_child("##filtered_notes", 0, 0) then
                for _, nota_id in ipairs(filtered) do
                    local titulo = notas.titulo(nota_id)
                    if titulo == "" then titulo = nota_id end

                    if imgui.selectable(titulo, false) then
                        notas.abrir(nota_id)
                    end
                end
            end
            imgui.end_child()
        else
            imgui.texto_disabled("Selecciona un tag para filtrar notas")
        end
    end)

    ui.menu_item("Reindexar Tags", function()
        needs_refresh = true
        ui.notificacion("Tags reindexados: " .. #tag_list .. " tags", "info")
    end)
end

function on_frame()
    refresh_counter = refresh_counter + 1

    if refresh_counter >= 600 then
        refresh_counter = 0
        needs_refresh = true
    end

    if needs_refresh then
        indexar_tags()
    end
end
