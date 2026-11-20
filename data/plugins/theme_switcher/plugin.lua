local themes = {}

themes["Node Panda (Default)"] = {
    bg          = "121219",
    bg_child    = "14141C",
    bg_popup    = "17171F",
    text        = "DBDDE9",
    text_dim    = "686A80",
    border      = "2E2E42",
    frame_bg    = "1C1C2B",
    title_bg    = "171722",
    menu_bg     = "0F0F17",
    button      = "1E3358",
    button_hover= "2B5485",
    header      = "213B5C",
    header_hover= "2E4E80",
    tab         = "171926",
    tab_hover   = "2E5285",
    tab_active  = "23406C",
    accent      = "47D1C2",
    separator   = "28293D",
    scrollbar   = "373A52",
}

themes["Nord"] = {
    bg          = "2E3440",
    bg_child    = "3B4252",
    bg_popup    = "434C5E",
    text        = "ECEFF4",
    text_dim    = "7B88A1",
    border      = "4C566A",
    frame_bg    = "3B4252",
    title_bg    = "2E3440",
    menu_bg     = "2E3440",
    button      = "5E81AC",
    button_hover= "81A1C1",
    header      = "5E81AC",
    header_hover= "81A1C1",
    tab         = "3B4252",
    tab_hover   = "81A1C1",
    tab_active  = "5E81AC",
    accent      = "88C0D0",
    separator   = "4C566A",
    scrollbar   = "4C566A",
}

themes["Catppuccin Mocha"] = {
    bg          = "1E1E2E",
    bg_child    = "181825",
    bg_popup    = "313244",
    text        = "CDD6F4",
    text_dim    = "6C7086",
    border      = "45475A",
    frame_bg    = "313244",
    title_bg    = "1E1E2E",
    menu_bg     = "181825",
    button      = "585B70",
    button_hover= "74C7EC",
    header      = "45475A",
    header_hover= "585B70",
    tab         = "181825",
    tab_hover   = "74C7EC",
    tab_active  = "45475A",
    accent      = "CBA6F7",
    separator   = "45475A",
    scrollbar   = "585B70",
}

themes["Gruvbox Dark"] = {
    bg          = "282828",
    bg_child    = "1D2021",
    bg_popup    = "3C3836",
    text        = "EBDBB2",
    text_dim    = "928374",
    border      = "504945",
    frame_bg    = "3C3836",
    title_bg    = "282828",
    menu_bg     = "1D2021",
    button      = "504945",
    button_hover= "689D6A",
    header      = "504945",
    header_hover= "689D6A",
    tab         = "1D2021",
    tab_hover   = "689D6A",
    tab_active  = "3C3836",
    accent      = "B8BB26",
    separator   = "504945",
    scrollbar   = "665C54",
}

themes["Dracula"] = {
    bg          = "282A36",
    bg_child    = "21222C",
    bg_popup    = "44475A",
    text        = "F8F8F2",
    text_dim    = "6272A4",
    border      = "44475A",
    frame_bg    = "44475A",
    title_bg    = "282A36",
    menu_bg     = "21222C",
    button      = "44475A",
    button_hover= "6272A4",
    header      = "44475A",
    header_hover= "6272A4",
    tab         = "21222C",
    tab_hover   = "BD93F9",
    tab_active  = "44475A",
    accent      = "FF79C6",
    separator   = "44475A",
    scrollbar   = "6272A4",
}

local function aplicar_tema(nombre)
    local tema = themes[nombre]
    if not tema then
        ui.notificacion("Tema no encontrado: " .. nombre, "error")
        return
    end

    ui.tema(tema)
    app.persistir("tema_actual", nombre)
    ui.notificacion("Tema aplicado: " .. nombre, "success")
    print("[Theme Switcher] Tema cambiado a: " .. nombre)
end

function on_load()
    print("[Theme Switcher] Plugin activo con " .. 5 .. " temas")

    local ultimo = app.leer_persistido("tema_actual")
    if ultimo ~= "" and themes[ultimo] then
        ui.tema(themes[ultimo])
        print("[Theme Switcher] Tema restaurado: " .. ultimo)
    end

    for nombre, _ in pairs(themes) do
        ui.menu_item("Tema: " .. nombre, function()
            aplicar_tema(nombre)
        end)
    end
end
