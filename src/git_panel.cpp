#include "git_panel.h"
#include "app.h"
#include "imgui.h"

namespace nodepanda {

void GitPanel::render(App& app) {
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Control de Versiones")) {
        ImGui::End();
        return;
    }

    auto& gitManager = app.getGitManager();
    auto& noteManager = app.getNoteManager();

    ImVec4 availableColor = gitManager.isAvailable() ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f)
                                                      : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    ImGui::TextColored(availableColor, gitManager.isAvailable() ? "Git: DISPONIBLE" : "Git: NO DISPONIBLE");
    ImGui::SameLine();
    ImGui::TextDisabled("(requiere git instalado)");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!gitManager.isAvailable()) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Instale git para usar control de versiones");
        ImGui::End();
        return;
    }

    auto status = gitManager.getStatus();
    ImGui::Text("Estado del Repositorio:");
    ImGui::SameLine();

    if (status.modified > 0) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Modificadas: %d", status.modified);
        ImGui::SameLine();
    }
    if (status.added > 0) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Agregadas: %d", status.added);
        ImGui::SameLine();
    }
    if (status.deleted > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Eliminadas: %d", status.deleted);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Checkbox("Auto-commit cada 5 minutos", &m_autoCommit);
    ImGui::SameLine();
    ImGui::TextDisabled("(si hay cambios)");

    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 1.0f, 1.0f));
    if (ImGui::Button("Guardar Version (Commit)", ImVec2(250, 0))) {
        std::string message;
        int noteCount = noteManager.getAllNotes().size();
        if (m_cachedNoteCount != noteCount) {
            message = "Cambios en notas";
            m_cachedNoteCount = noteCount;
        } else {
            message = "Actualización manual";
        }
        gitManager.autoCommit(message);
        m_history = gitManager.getHistory("", 30);
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();

    if (m_autoCommit) {
        m_frameCounter++;
        if (m_frameCounter >= 18000) {
            m_frameCounter = 0;
            if (gitManager.hasChanges()) {
                gitManager.autoCommit("Auto-commit: cambios automáticos");
                m_history = gitManager.getHistory("", 30);
            }
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Historial de Commits:");
    ImGui::Spacing();

    if (m_history.empty()) {
        m_history = gitManager.getHistory("", 30);
    }

    if (ImGui::BeginChild("##commit_history", ImVec2(0, 200), true)) {
        for (const auto& commit : m_history) {
            bool selected = (m_selectedCommit == commit.hash);
            ImGui::PushID(commit.hash.c_str());

            if (ImGui::Selectable(commit.message.c_str(), selected)) {
                m_selectedCommit = commit.hash;
                m_showDiff = true;
                m_diff.clear();
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\n%s\nAutor: %s\nFecha: %s\nArchivos: %d",
                                  commit.hash.c_str(),
                                  commit.message.c_str(),
                                  commit.author.c_str(),
                                  commit.date.c_str(),
                                  commit.filesChanged);
            }

            ImGui::PopID();
        }
        ImGui::EndChild();
    }

    ImGui::Spacing();

    if (!m_selectedCommit.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.5f, 0.0f, 1.0f));
        if (ImGui::Button("Restaurar desde Commit Seleccionado", ImVec2(300, 0))) {

            for (const auto& commit : m_history) {
                if (commit.hash == m_selectedCommit) {
                    ImGui::OpenPopup("restore_confirm");
                    break;
                }
            }
        }
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (m_showDiff && !m_selectedCommit.empty()) {
        ImGui::Text("Vista de Cambios - Commit: %s", m_selectedCommit.c_str());
        ImGui::Spacing();

        if (ImGui::BeginChild("##diff_viewer", ImVec2(0, 200), true)) {
            for (const auto& line : m_diff) {
                if (line.type == 1) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "+ %s", line.text.c_str());
                } else if (line.type == -1) {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "- %s", line.text.c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "  %s", line.text.c_str());
                }
            }
            ImGui::EndChild();
        }
    }

    if (ImGui::BeginPopupModal("restore_confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Restaurar archivos desde este commit?");
        ImGui::Text("Esto puede descartar cambios recientes.");
        ImGui::Spacing();

        if (ImGui::Button("Cancelar", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        if (ImGui::Button("Restaurar", ImVec2(120, 0))) {

            auto& allNotes = noteManager.getAllNotes();
            for (const auto& [id, note] : allNotes) {
                gitManager.restoreFile(m_selectedCommit, id + ".md");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();

        ImGui::EndPopup();
    }

    ImGui::End();
}

}
