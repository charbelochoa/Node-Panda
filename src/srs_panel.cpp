#include "srs_panel.h"
#include "app.h"
#include "imgui.h"

namespace nodepanda {

void SRSPanel::render(App& app) {

    if (!m_initialized) {
        m_engine.buildQueue(app.getNoteManager());
        m_initialized = true;
        m_currentCard = 0;
        m_showAnswer = false;
    }

    if (ImGui::Begin("Repeticion Espaciada", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {

        if (ImGui::BeginTabBar("##SRSTabs")) {

            if (ImGui::BeginTabItem("Revision")) {
                const auto& dueCards = m_engine.getDueCards();
                int dueCount = m_engine.dueToday();

                ImGui::Text("Cards en revision: %d", dueCount);
                ImGui::Spacing();

                if (dueCount == 0) {
                    ImGui::Text("Revision completada!");
                    ImGui::Spacing();
                } else {

                    if (m_currentCard >= (int)dueCards.size()) {
                        m_currentCard = 0;
                    }

                    const SRSCard& currentCard = dueCards[m_currentCard];
                    Note* note = app.getNoteManager().getNoteById(currentCard.noteId);

                    if (note) {

                        ImGui::Text("%d / %d", m_currentCard + 1, dueCount);
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Pregunta: %s",
                                         currentCard.title.c_str());
                        ImGui::Spacing();

                        std::string preview = note->content;
                        if (preview.length() > 200) {
                            preview = preview.substr(0, 200) + "...";
                        }
                        ImGui::TextWrapped("(Respuesta oculta)");
                        ImGui::Spacing();

                        if (ImGui::Button("Mostrar respuesta", ImVec2(150, 0))) {
                            m_showAnswer = !m_showAnswer;
                        }

                        if (m_showAnswer) {
                            ImGui::Separator();
                            ImGui::TextWrapped("Respuesta:\n%s", note->content.c_str());
                            ImGui::Separator();
                        }

                        ImGui::Spacing();

                        ImGui::Text("Califica tu respuesta:");
                        ImGui::Spacing();

                        bool rated = false;

                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                           ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                        if (ImGui::Button("De nuevo##again", ImVec2(100, 30))) {
                            m_engine.rateCard(currentCard.noteId, SRSRating::Again, app.getNoteManager());
                            rated = true;
                        }
                        ImGui::PopStyleColor(2);

                        ImGui::SameLine();

                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                           ImVec4(1.0f, 0.8f, 0.3f, 1.0f));
                        if (ImGui::Button("Dificil##hard", ImVec2(100, 30))) {
                            m_engine.rateCard(currentCard.noteId, SRSRating::Hard, app.getNoteManager());
                            rated = true;
                        }
                        ImGui::PopStyleColor(2);

                        ImGui::SameLine();

                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                           ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                        if (ImGui::Button("Bien##good", ImVec2(100, 30))) {
                            m_engine.rateCard(currentCard.noteId, SRSRating::Good, app.getNoteManager());
                            rated = true;
                        }
                        ImGui::PopStyleColor(2);

                        ImGui::SameLine();

                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 1.0f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                           ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
                        if (ImGui::Button("Facil##easy", ImVec2(100, 30))) {
                            m_engine.rateCard(currentCard.noteId, SRSRating::Easy, app.getNoteManager());
                            rated = true;
                        }
                        ImGui::PopStyleColor(2);

                        if (rated) {
                            m_currentCard++;
                            m_showAnswer = false;
                            m_engine.buildQueue(app.getNoteManager());
                            if (m_currentCard >= (int)m_engine.getDueCards().size()) {
                                m_currentCard = 0;
                            }
                        }
                    }
                }

                ImGui::Spacing();
                if (ImGui::Button("Refrescar", ImVec2(100, 0))) {
                    m_engine.buildQueue(app.getNoteManager());
                    m_currentCard = 0;
                    m_showAnswer = false;
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Gestionar")) {
                const auto& allNotes = app.getNoteManager().getAllNotes();
                const auto& allCards = m_engine.getAllCards();

                ImGui::Text("Total notas: %d", (int)allNotes.size());
                ImGui::Text("Inscritas en SRS: %d", m_engine.totalEnrolled());
                ImGui::Text("Debido hoy: %d", m_engine.dueToday());
                ImGui::Separator();

                ImGui::Text("Notas Inscritas:");
                ImGui::BeginChild("EnrolledList", ImVec2(0, 200), true);

                for (const auto& card : allCards) {
                    ImGui::Text("ID: %s", card.noteId.c_str());
                    ImGui::Text("  Titulo: %s", card.title.c_str());
                    ImGui::Text("  Intervalo: %d dias | Facilidad: %.2f | Reps: %d",
                              card.interval, card.easeFactor, card.repetitions);
                    ImGui::Text("  Proximo: %s", card.nextReview.c_str());

                    ImGui::PushID(card.noteId.c_str());
                    if (ImGui::Button("Desinscribir")) {
                        m_engine.unenrollNote(card.noteId, app.getNoteManager());
                        m_engine.buildQueue(app.getNoteManager());
                    }
                    ImGui::PopID();

                    ImGui::Separator();
                }

                ImGui::EndChild();

                ImGui::Spacing();
                ImGui::Text("Todas las Notas:");
                ImGui::BeginChild("AllNotesList", ImVec2(0, 200), true);

                for (const auto& pair : allNotes) {
                    const std::string& noteId = pair.first;
                    const Note& note = pair.second;

                    bool enrolled = m_engine.isEnrolled(noteId, app.getNoteManager());

                    ImGui::Text("ID: %s | %s", noteId.c_str(), note.title.c_str());

                    ImGui::PushID(noteId.c_str());
                    if (!enrolled) {
                        if (ImGui::Button("Inscribir")) {
                            m_engine.enrollNote(noteId, app.getNoteManager());
                            m_engine.buildQueue(app.getNoteManager());
                        }
                    } else {
                        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[Inscrita]");
                    }
                    ImGui::PopID();

                    ImGui::Separator();
                }

                ImGui::EndChild();

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }
}

}
