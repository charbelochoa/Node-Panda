#include "srs_panel.h"
#include "app.h"
#include "ui_theme.h"
#include "ui_components.h"
#include "imgui.h"
#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>

namespace nodepanda {

namespace {

std::unordered_set<std::string> g_revealedClozeKeys;

}

void SRSPanel::render(App& app) {
    const ThemePalette& pal = UITheme::palette();

    if (!m_initialized) {
        m_engine.buildQueue(app.getNoteManager());
        m_initialized = true;
        m_currentCard = 0;
        m_showAnswer = false;
    }

    ImGui::SetNextWindowSize(ImVec2(660.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Repeticion Espaciada", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    const auto& allNotes = app.getNoteManager().getAllNotes();
    std::vector<ClozeCard> clozeCards = app.getSRSEngine().extractClozeCards(app.getNoteManager());

    float availW = ImGui::GetContentRegionAvail().x;
    float cardW = std::max(100.0f, (availW - 24.0f) * 0.25f);
    ui::metricCard("##srs_due", "Pendientes hoy", std::to_string(m_engine.dueToday()), pal.warning, cardW);
    ImGui::SameLine(0.0f, 8.0f);
    ui::metricCard("##srs_enrolled", "Inscritas SM-2", std::to_string(m_engine.totalEnrolled()), pal.accent, cardW);
    ImGui::SameLine(0.0f, 8.0f);
    ui::metricCard("##srs_cloze", "Tarjetas Cloze", std::to_string(clozeCards.size()), pal.aqua, cardW);
    ImGui::SameLine(0.0f, 8.0f);
    ui::metricCard("##srs_notes", "Total notas", std::to_string(allNotes.size()), pal.textPrimary, cardW);

    ImGui::Spacing();

    std::vector<std::string> tabs = {"Revision SM-2", "Tarjetas Cloze", "Gestionar"};
    if (m_selectedTab < 0 || m_selectedTab >= static_cast<int>(tabs.size())) {
        m_selectedTab = 0;
    }
    ui::segmentedControl("##srs_tabs_seg", &m_selectedTab, tabs, ImGui::GetContentRegionAvail().x);
    ImGui::Spacing();

    if (m_selectedTab == 0) {
        const auto& dueCards = m_engine.getDueCards();
        int dueCount = m_engine.dueToday();

        char queueBadge[64];
        std::snprintf(queueBadge, sizeof(queueBadge), "En cola: %d", dueCount);
        ui::badge(queueBadge, pal.accentSoft, pal.accent);
        ImGui::SameLine(0.0f, 8.0f);
        if (ui::subtleButton("Refrescar cola")) {
            m_engine.buildQueue(app.getNoteManager());
            m_currentCard = 0;
            m_showAnswer = false;
        }
        ImGui::Spacing();

        if (dueCount == 0) {
            if (ui::beginCard("##srs_empty_card", ImVec2(0, 0), false)) {
                ui::statusDot("Revision completada", pal.success, false);
                ImGui::Spacing();
                ImGui::TextColored(pal.textSecondary, "No hay tarjetas pendientes para hoy.");
            }
            ui::endCard();
        } else {
            if (m_currentCard >= static_cast<int>(dueCards.size())) {
                m_currentCard = 0;
            }

            const SRSCard& currentCard = dueCards[m_currentCard];
            Note* note = app.getNoteManager().getNoteById(currentCard.noteId);

            if (note) {
                if (ui::beginCard("##srs_review_card", ImVec2(0, 0), false)) {
                    char progressBuf[32];
                    std::snprintf(progressBuf, sizeof(progressBuf), "%d / %d", m_currentCard + 1, dueCount);
                    ui::badge(progressBuf, pal.bgSurface3, pal.textPrimary);
                    ImGui::SameLine(0.0f, 8.0f);
                    ui::badge(currentCard.noteId.c_str(), pal.accentSoft, pal.accent);

                    ImGui::Spacing();
                    ImGui::TextColored(pal.textPrimary, "Pregunta: %s", currentCard.title.c_str());
                    ImGui::Spacing();

                    if (!m_showAnswer) {
                        ImGui::TextColored(pal.textMuted, "(Respuesta oculta)");
                    }

                    ImGui::Spacing();
                    if (ui::primaryButton(m_showAnswer ? "Ocultar Respuesta" : "Mostrar Respuesta", ImVec2(170.0f, 0.0f))) {
                        m_showAnswer = !m_showAnswer;
                    }

                    if (m_showAnswer) {
                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Text, pal.textSecondary);
                        ImGui::TextWrapped("%s", note->content.c_str());
                        ImGui::PopStyleColor();
                        ImGui::Spacing();
                        ImGui::Separator();
                    }

                    ImGui::Spacing();
                    ImGui::TextColored(pal.textMuted, "Califica tu respuesta:");
                    ImGui::Spacing();

                    bool rated = false;
                    float btnW = std::max(84.0f, (ImGui::GetContentRegionAvail().x - 24.0f) * 0.25f);

                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(pal.danger.x, pal.danger.y, pal.danger.z, 0.24f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.danger);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, pal.danger);
                    if (ImGui::Button("De nuevo##again", ImVec2(btnW, 32.0f))) {
                        m_engine.rateCard(currentCard.noteId, SRSRating::Again, app.getNoteManager());
                        rated = true;
                    }
                    ImGui::PopStyleColor(3);

                    ImGui::SameLine(0.0f, 8.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(pal.warning.x, pal.warning.y, pal.warning.z, 0.24f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.warning);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, pal.warning);
                    if (ImGui::Button("Dificil##hard", ImVec2(btnW, 32.0f))) {
                        m_engine.rateCard(currentCard.noteId, SRSRating::Hard, app.getNoteManager());
                        rated = true;
                    }
                    ImGui::PopStyleColor(3);

                    ImGui::SameLine(0.0f, 8.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(pal.success.x, pal.success.y, pal.success.z, 0.24f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.success);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, pal.success);
                    if (ImGui::Button("Bien##good", ImVec2(btnW, 32.0f))) {
                        m_engine.rateCard(currentCard.noteId, SRSRating::Good, app.getNoteManager());
                        rated = true;
                    }
                    ImGui::PopStyleColor(3);

                    ImGui::SameLine(0.0f, 8.0f);
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(pal.accent.x, pal.accent.y, pal.accent.z, 0.24f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.accent);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, pal.accent);
                    if (ImGui::Button("Facil##easy", ImVec2(btnW, 32.0f))) {
                        m_engine.rateCard(currentCard.noteId, SRSRating::Easy, app.getNoteManager());
                        rated = true;
                    }
                    ImGui::PopStyleColor(3);

                    if (rated) {
                        m_currentCard++;
                        m_showAnswer = false;
                        m_engine.buildQueue(app.getNoteManager());
                        if (m_currentCard >= static_cast<int>(m_engine.getDueCards().size())) {
                            m_currentCard = 0;
                        }
                        ui::pushToast("Revision registrada", "success");
                    }
                }
                ui::endCard();
            }
        }
    } else if (m_selectedTab == 1) {
        if (ui::subtleButton("Mostrar todas")) {
            for (size_t i = 0; i < clozeCards.size(); ++i) {
                g_revealedClozeKeys.insert(std::to_string(i) + ":" + clozeCards[i].noteId);
            }
        }
        ImGui::SameLine(0.0f, 8.0f);
        if (ui::subtleButton("Ocultar todas")) {
            g_revealedClozeKeys.clear();
        }
        ImGui::Spacing();

        if (clozeCards.empty()) {
            if (ui::beginCard("##cloze_empty", ImVec2(0, 0), false)) {
                ImGui::TextColored(pal.textMuted, "No se encontraron tarjetas Cloze.");
                ImGui::TextColored(pal.textSecondary, "Usa {{cloze respuesta}} o #card dentro de tus notas.");
            }
            ui::endCard();
        } else {
            ImGui::BeginChild("##cloze_scroll", ImVec2(0, 0), false);
            for (size_t i = 0; i < clozeCards.size(); ++i) {
                const auto& cc = clozeCards[i];
                std::string key = std::to_string(i) + ":" + cc.noteId;
                bool revealed = (g_revealedClozeKeys.find(key) != g_revealedClozeKeys.end());

                ImGui::PushID(static_cast<int>(i));
                if (ui::beginCard("##cloze_card", ImVec2(0, 0), true)) {
                    char idxBuf[24];
                    std::snprintf(idxBuf, sizeof(idxBuf), "#%d", static_cast<int>(i + 1));
                    ui::badge(idxBuf, pal.bgSurface3, pal.textSecondary);
                    ImGui::SameLine(0.0f, 6.0f);
                    ui::badge(cc.noteId.c_str(), pal.accentSoft, pal.accent);
                    ImGui::SameLine(0.0f, 8.0f);
                    if (ui::subtleButton("Abrir nota")) {
                        app.selectedNoteId = cc.noteId;
                    }

                    ImGui::Spacing();
                    ImGui::TextWrapped("%s", cc.prompt.c_str());
                    ImGui::Spacing();

                    if (ui::subtleButton(revealed ? "Ocultar Respuesta" : "Mostrar Respuesta")) {
                        if (revealed) g_revealedClozeKeys.erase(key);
                        else g_revealedClozeKeys.insert(key);
                    }

                    if (revealed) {
                        ImGui::SameLine(0.0f, 10.0f);
                        ui::badge(cc.answer.c_str(),
                                  ImVec4(pal.success.x, pal.success.y, pal.success.z, 0.20f),
                                  pal.success);
                    }
                }
                ui::endCard();
                ImGui::Spacing();
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
    } else {
        const auto& allCards = m_engine.getAllCards();

        ui::sectionHeader("Notas Inscritas");
        ImGui::BeginChild("EnrolledList", ImVec2(0, 190.0f), true);
        if (allCards.empty()) {
            ImGui::TextColored(pal.textMuted, "Ninguna nota inscrita en SM-2.");
        }
        for (const auto& card : allCards) {
            ImGui::PushID(card.noteId.c_str());
            if (ui::beginCard("##enr_card", ImVec2(0, 0), false)) {
                ImGui::TextColored(pal.textPrimary, "%s", card.title.c_str());
                ImGui::SameLine(0.0f, 8.0f);
                ui::badge(card.noteId.c_str(), pal.accentSoft, pal.accent);

                ImGui::TextColored(pal.textSecondary,
                                   "Intervalo: %d dias  |  Facilidad: %.2f  |  Reps: %d  |  Proximo: %s",
                                   card.interval, card.easeFactor, card.repetitions, card.nextReview.c_str());

                if (ui::dangerButton("Desinscribir")) {
                    m_engine.unenrollNote(card.noteId, app.getNoteManager());
                    m_engine.buildQueue(app.getNoteManager());
                    ui::pushToast("Nota desinscrita de SRS", "info");
                }
            }
            ui::endCard();
            ImGui::Spacing();
            ImGui::PopID();
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ui::sectionHeader("Todas las Notas");
        ImGui::BeginChild("AllNotesList", ImVec2(0, 0), true);
        for (const auto& pair : allNotes) {
            const std::string& noteId = pair.first;
            const Note& note = pair.second;
            bool enrolled = m_engine.isEnrolled(noteId, app.getNoteManager());

            ImGui::PushID(noteId.c_str());
            ImGui::TextColored(pal.textPrimary, "%s", note.title.c_str());
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::TextColored(pal.textMuted, "(%s)", noteId.c_str());
            ImGui::SameLine(0.0f, 10.0f);
            if (!enrolled) {
                if (ui::subtleButton("Inscribir")) {
                    m_engine.enrollNote(noteId, app.getNoteManager());
                    m_engine.buildQueue(app.getNoteManager());
                    ui::pushToast("Nota inscrita en SRS", "success");
                }
            } else {
                ui::badge("Inscrita", ImVec4(pal.success.x, pal.success.y, pal.success.z, 0.20f), pal.success);
            }
            ImGui::Separator();
            ImGui::PopID();
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

}
