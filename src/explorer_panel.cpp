

#include "explorer_panel.h"
#include "imgui.h"
#include <algorithm>
#include <map>
#include <cstring>

namespace nodepanda {

static ImVec4 noteTypeColor(const std::string& tipo) {
    if (tipo == "proyecto") return ImVec4(0.35f, 0.75f, 1.00f, 1.0f);
    if (tipo == "concepto") return ImVec4(0.55f, 0.90f, 0.55f, 1.0f); 
    if (tipo == "agente")   return ImVec4(0.90f, 0.65f, 0.25f, 1.0f); 
    return                         ImVec4(0.60f, 0.62f, 0.72f, 1.0f); 
}

static const char* noteTypeTag(const std::string& tipo) {
    if (tipo == "proyecto") return "[P]";
    if (tipo == "concepto") return "[C]";
    if (tipo == "agente")   return "[A]";
    return                         " - ";
}


void ExplorerPanel::render(App& app) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    ImGui::Begin("Explorador de Notas", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();

    auto& nm = app.getNoteManager();

    float btnW = (ImGui::GetContentRegionAvail().x - 4.0f) * 0.5f;

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.32f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.16f, 0.45f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.10f, 0.25f, 0.14f, 1.0f));
    if (ImGui::Button("+ Nota", ImVec2(btnW, 26.0f))) {
        m_showNewNoteDialog = true;
        memset(m_newNoteName,   0, sizeof(m_newNoteName));
        memset(m_newNoteFolder, 0, sizeof(m_newNoteFolder));
    }
    ImGui::PopStyleColor(3);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Crear una nueva nota Markdown (.md)");

    ImGui::SameLine(0.0f, 4.0f);

    if (ImGui::Button("+ Carpeta", ImVec2(btnW, 26.0f))) {
        m_showNewFolderDialog = true;
        memset(m_newFolderName, 0, sizeof(m_newFolderName));
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Crear una subcarpeta dentro del directorio de notas");

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.16f, 1.0f));
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##search", "  Buscar notas...",
                             m_searchBuffer, sizeof(m_searchBuffer));
    ImGui::PopStyleColor();

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.18f, 0.20f, 0.30f, 0.80f));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    
    auto results = nm.searchNotes(std::string(m_searchBuffer));

    std::map<std::string, std::vector<Note*>> byFolder;
    for (auto* n : results) {
        auto rel = std::filesystem::relative(n->filepath.parent_path(),
                                             nm.getNotesDirectory());
        std::string folder = rel.string();
        if (folder == ".") folder = "";
        byFolder[folder].push_back(n);
    }

    for (auto& [f, notes] : byFolder)
        std::sort(notes.begin(), notes.end(),
                  [](const Note* a, const Note* b){ return a->id < b->id; });

    
    ImGui::BeginChild("##note_list", ImVec2(0, -1), false,
                      ImGuiWindowFlags_HorizontalScrollbar);

    for (auto& [folder, notes] : byFolder) {
        bool inRoot   = folder.empty();
        bool treeOpen = inRoot; 

        if (!inRoot) {
            
            ImGui::PushStyleColor(ImGuiCol_Text,        ImVec4(0.55f, 0.58f, 0.72f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Header,      ImVec4(0.10f, 0.12f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered,ImVec4(0.14f, 0.18f, 0.28f, 1.0f));
            treeOpen = ImGui::TreeNodeEx(
                ("  [/] " + folder).c_str(),
                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth);
            ImGui::PopStyleColor(3);
        }

        if (treeOpen) {
            for (auto* note : notes) {
                ImGui::PushID(note->id.c_str());

                const std::string& tipo = note->getFrontmatter("tipo");
                bool isSelected         = (app.selectedNoteId == note->id);
                ImVec4 typeCol          = noteTypeColor(tipo);
                const char* tag         = noteTypeTag(tipo);

                
                ImVec2 p0 = ImGui::GetCursorScreenPos();
                p0.x += inRoot ? 4.0f : 18.0f;
                p0.y += 4.0f;
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(p0,
                    ImVec2(p0.x + 3.0f, p0.y + 10.0f),
                    ImGui::ColorConvertFloat4ToU32(typeCol), 2.0f);

                
                std::string label = std::string("    ") + tag + " ";
                if (note->dirty) label = "* " + label;
                label += note->id;

                
                ImVec4 textCol = isSelected
                    ? ImVec4(0.28f, 0.88f, 0.82f, 1.0f)   
                    : ImVec4(0.78f, 0.80f, 0.88f, 1.0f);  
                if (note->dirty)
                    textCol = ImVec4(0.88f, 0.75f, 0.28f, 1.0f); 

                ImGui::PushStyleColor(ImGuiCol_Text, textCol);
                ImGui::PushStyleColor(ImGuiCol_Header,
                    isSelected ? ImVec4(0.12f, 0.28f, 0.45f, 1.0f)
                               : ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                    ImVec4(0.10f, 0.20f, 0.35f, 0.70f));

                if (ImGui::Selectable(label.c_str(), isSelected,
                                      ImGuiSelectableFlags_SpanAllColumns,
                                      ImVec2(0, 20))) {
                    app.selectedNoteId = note->id;
                }
                ImGui::PopStyleColor(3);

                
                if (ImGui::BeginPopupContextItem("##ctx")) {
                    if (ImGui::MenuItem("Eliminar nota...")) {
                        m_deleteTargetId  = note->id;
                        m_showDeleteConfirm = true;
                    }
                    ImGui::EndPopup();
                }

                
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    ImGui::BeginTooltip();
                    ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f),
                                       "%s", note->title.c_str());
                    if (!tipo.empty())
                        ImGui::TextColored(typeCol, "Tipo: %s", tipo.c_str());
                    ImGui::Text("Enlaces: %d", (int)note->links.size());
                    if (!note->frontmatter.empty()) {
                        ImGui::Separator();
                        for (const auto& [k, v] : note->frontmatter) {
                            ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.72f, 1.0f),
                                               "%s:", k.c_str());
                            ImGui::SameLine();
                            ImGui::Text("%s", v.c_str());
                        }
                    }
                    if (note->dirty) {
                        ImGui::Separator();
                        ImGui::TextColored(ImVec4(0.88f, 0.75f, 0.28f, 1.0f),
                                           "* Cambios sin guardar");
                    }
                    ImGui::EndTooltip();
                }

                ImGui::PopID();
            }

            if (!inRoot) ImGui::TreePop();
        }
    }

    
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.36f, 0.46f, 1.0f));
    ImGui::Text("  %d nota(s)", (int)results.size());
    ImGui::PopStyleColor();

    ImGui::EndChild(); 

    
    ImVec2 mc = ImGui::GetMainViewport()->GetCenter();

    
    if (m_showNewNoteDialog) ImGui::OpenPopup("Nueva Nota");
    ImGui::SetNextWindowPos(mc, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Nueva Nota", &m_showNewNoteDialog,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f), "Crear nueva nota");
        ImGui::Spacing();
        ImGui::Text("Nombre:");
        ImGui::SetNextItemWidth(280.0f);
        ImGui::InputText("##nn", m_newNoteName, sizeof(m_newNoteName));
        ImGui::Text("Subcarpeta (opcional):");
        ImGui::SetNextItemWidth(280.0f);
        ImGui::InputText("##nf", m_newNoteFolder, sizeof(m_newNoteFolder));
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(0.12f, 0.32f, 0.18f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.45f, 0.24f, 1.0f));
        if (ImGui::Button("Crear", ImVec2(130, 0))) {
            std::string name(m_newNoteName);
            if (!name.empty()) {
                nm.createNote(name, std::string(m_newNoteFolder));
                app.selectedNoteId    = name;
                app.graphNeedsRebuild = true;
            }
            m_showNewNoteDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(130, 0))) {
            m_showNewNoteDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    
    if (m_showNewFolderDialog) ImGui::OpenPopup("Nueva Carpeta");
    ImGui::SetNextWindowPos(mc, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Nueva Carpeta", &m_showNewFolderDialog,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.28f, 0.88f, 0.82f, 1.0f), "Crear subcarpeta");
        ImGui::Spacing();
        ImGui::Text("Nombre:");
        ImGui::SetNextItemWidth(280.0f);
        ImGui::InputText("##fn", m_newFolderName, sizeof(m_newFolderName));
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(0.12f, 0.20f, 0.38f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.28f, 0.52f, 1.0f));
        if (ImGui::Button("Crear", ImVec2(130, 0))) {
            std::string name(m_newFolderName);
            if (!name.empty()) nm.createSubfolder(name);
            m_showNewFolderDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(130, 0))) {
            m_showNewFolderDialog = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (m_showDeleteConfirm) ImGui::OpenPopup("Confirmar Eliminacion");
    ImGui::SetNextWindowPos(mc, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Confirmar Eliminacion", &m_showDeleteConfirm,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(0.90f, 0.40f, 0.35f, 1.0f),
                           "Eliminar la nota '%s'?", m_deleteTargetId.c_str());
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.65f, 1.0f),
                           "Esta accion no se puede deshacer.");
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button,       ImVec4(0.38f, 0.12f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.16f, 0.14f, 1.0f));
        if (ImGui::Button("Eliminar", ImVec2(130, 0))) {
            if (app.selectedNoteId == m_deleteTargetId)
                app.selectedNoteId.clear();
            nm.deleteNote(m_deleteTargetId);
            app.graphNeedsRebuild = true;
            m_showDeleteConfirm   = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(130, 0))) {
            m_showDeleteConfirm = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} 
