#include "sync_panel.h"
#include "app.h"
#include "backup_manager.h"
#include "sync_engine.h"
#include "ui_components.h"
#include "ui_theme.h"
#include "imgui.h"

#include <cstdio>
#include <filesystem>
#include <string>

namespace nodepanda {

namespace {

std::string formatBytes(size_t bytes) {
    char buf[64];
    if (bytes >= 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.2f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f KB", static_cast<double>(bytes) / 1024.0);
    }
    return std::string(buf);
}

std::string shortHash(const std::string& h, size_t n = 12) {
    if (h.empty()) return "-";
    return h.size() > n ? h.substr(0, n) : h;
}

}

void SyncPanel::render(App& app) {
    ImGui::SetNextWindowSize(ImVec2(820, 620), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Sync & Backups")) {
        ImGui::End();
        return;
    }

    const auto& pal = UITheme::palette();
    auto& sync = app.getSyncEngine();
    auto& backups = app.getBackupManager();
    auto& nm = app.getNoteManager();

    SyncDiagnostics diag = sync.diagnostics(nm);

    if (!m_transportLoaded) {
        std::snprintf(m_httpBaseBuf, sizeof(m_httpBaseBuf), "%s", diag.httpBase.c_str());
        std::snprintf(m_wsUrlBuf, sizeof(m_wsUrlBuf), "%s", diag.wsUrl.c_str());
        m_transportLoaded = true;
    }

    ui::statusDot(diag.running ? "SYNC ACTIVE" : "SYNC IDLE", diag.running ? pal.success : pal.textMuted, diag.running);
    ImGui::SameLine(0.0f, 18.0f);
    ui::statusDot(diag.inSync ? "IN SYNC" : "PENDING", diag.inSync ? pal.aqua : pal.warning, false);
    ImGui::SameLine(0.0f, 18.0f);
    if (diag.e2eeEnabled) {
        ui::badge("E2EE LOCKED", ImVec4(pal.proViolet.x, pal.proViolet.y, pal.proViolet.z, 0.22f), pal.proViolet);
    } else {
        ui::badge("E2EE OFF", ImVec4(pal.textMuted.x, pal.textMuted.y, pal.textMuted.z, 0.20f), pal.textSecondary);
    }

    ImGui::SameLine();
    if (!diag.running) {
        if (ui::primaryButton("Start Sync")) {
            sync.startSync();
            m_statusMsg = "Sync engine started";
            m_statusIsError = false;
            ui::pushToast(m_statusMsg, "success");
        }
    } else {
        if (ui::dangerButton("Stop Sync")) {
            sync.stopSync();
            m_statusMsg = "Sync engine stopped";
            m_statusIsError = false;
            ui::pushToast(m_statusMsg, "info");
        }
    }

    ImGui::Spacing();
    float availW = ImGui::GetContentRegionAvail().x;
    float cardW = (availW - 24.0f) / 4.0f;
    if (cardW < 110.0f) cardW = 110.0f;

    ui::metricCard("##sync_local_t", "LOCAL T", std::to_string(diag.localT), pal.accent, cardW);
    ImGui::SameLine();
    ui::metricCard("##sync_remote_t", "REMOTE T", std::to_string(diag.remoteT), pal.aqua, cardW);
    ImGui::SameLine();
    ui::metricCard("##sync_pending", "PENDING", std::to_string(diag.pendingBatchCount),
                   diag.pendingBatchCount == 0 ? pal.success : pal.warning, cardW);
    ImGui::SameLine();
    ui::metricCard("##sync_checksum", "STATE HASH", shortHash(diag.localChecksum, 10), pal.proViolet, cardW);
    if (ImGui::IsItemHovered() && !diag.localChecksum.empty()) {
        ImGui::SetTooltip("Local: %s\nRemote: %s", diag.localChecksum.c_str(),
                          diag.remoteChecksum.empty() ? "-" : diag.remoteChecksum.c_str());
    }

    if (!m_statusMsg.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(m_statusIsError ? pal.danger : pal.success, "%s", m_statusMsg.c_str());
    }

    ImGui::Spacing();
    if (ImGui::BeginTabBar("##sync_tabs")) {
        if (ImGui::BeginTabItem("Sync & E2EE")) {
            ui::sectionHeader("Transport");
            ImGui::SetNextItemWidth(320.0f);
            ImGui::InputText("HTTP Base", m_httpBaseBuf, sizeof(m_httpBaseBuf));
            ImGui::SetNextItemWidth(320.0f);
            ImGui::InputText("WebSocket URL", m_wsUrlBuf, sizeof(m_wsUrlBuf));
            if (ui::subtleButton("Save Transport")) {
                sync.configureTransport(m_httpBaseBuf, m_wsUrlBuf);
                m_statusMsg = "Transport configuration saved";
                m_statusIsError = false;
                ui::pushToast(m_statusMsg, "success");
            }

            ImGui::Spacing();
            ui::sectionHeader("End-to-End Encryption");
            ImGui::SetNextItemWidth(240.0f);
            ImGui::InputTextWithHint("##e2ee_pass", "New E2EE password...", m_e2eePasswordBuf, sizeof(m_e2eePasswordBuf), ImGuiInputTextFlags_Password);
            ImGui::SameLine();
            if (ui::primaryButton("Enable / Rotate E2EE")) {
                std::string err;
                if (sync.enableE2EE(m_e2eePasswordBuf, err)) {
                    m_e2eePasswordBuf[0] = '\0';
                    m_statusMsg = "E2EE key verifier updated";
                    m_statusIsError = false;
                    ui::pushToast(m_statusMsg, "success");
                } else {
                    m_statusMsg = err;
                    m_statusIsError = true;
                    ui::pushToast(err, "error");
                }
            }

            ImGui::SetNextItemWidth(240.0f);
            ImGui::InputTextWithHint("##e2ee_verify", "Verify password...", m_e2eeVerifyBuf, sizeof(m_e2eeVerifyBuf), ImGuiInputTextFlags_Password);
            ImGui::SameLine();
            if (ui::subtleButton("Verify Password")) {
                bool ok = sync.verifyE2EEPassword(m_e2eeVerifyBuf);
                m_e2eeVerifyBuf[0] = '\0';
                m_statusMsg = ok ? "E2EE password verified" : "Invalid E2EE password";
                m_statusIsError = !ok;
                ui::pushToast(m_statusMsg, ok ? "success" : "error");
            }

            ImGui::Spacing();
            ui::sectionHeader("Snapshots");
            ImGui::SetNextItemWidth(320.0f);
            ImGui::InputTextWithHint("##snap_path", "Snapshot file path...", m_snapshotPathBuf, sizeof(m_snapshotPathBuf));
            ImGui::SameLine();
            ImGui::Checkbox("Encrypt", &m_encryptSnapshot);
            ImGui::SetNextItemWidth(240.0f);
            ImGui::InputTextWithHint("##snap_pass", "Snapshot password...", m_snapshotPasswordBuf, sizeof(m_snapshotPasswordBuf), ImGuiInputTextFlags_Password);
            ImGui::SameLine();
            if (ui::primaryButton("Export Snapshot")) {
                std::string err;
                if (sync.exportSnapshot(nm, std::filesystem::path(m_snapshotPathBuf), m_encryptSnapshot, m_snapshotPasswordBuf, err)) {
                    m_statusMsg = "Snapshot exported";
                    m_statusIsError = false;
                    ui::pushToast(m_statusMsg, "success");
                } else {
                    m_statusMsg = err;
                    m_statusIsError = true;
                    ui::pushToast(err, "error");
                }
            }
            ImGui::SameLine();
            if (ui::subtleButton("Import Snapshot")) {
                std::string err;
                if (sync.importSnapshot(nm, std::filesystem::path(m_snapshotPathBuf), m_snapshotPasswordBuf, err)) {
                    app.graphNeedsRebuild = true;
                    m_statusMsg = "Snapshot imported";
                    m_statusIsError = false;
                    ui::pushToast(m_statusMsg, "success");
                } else {
                    m_statusMsg = err;
                    m_statusIsError = true;
                    ui::pushToast(err, "error");
                }
            }

            ImGui::Spacing();
            ui::sectionHeader("Transaction Log");
            const auto& txs = sync.transactions();
            ImGuiTableFlags txFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##sync_tx_table", 5, txFlags, ImVec2(0.0f, 180.0f))) {
                ImGui::TableSetupColumn("t", ImGuiTableColumnFlags_WidthFixed, 46.0f);
                ImGui::TableSetupColumn("Op", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Entity", ImGuiTableColumnFlags_WidthStretch, 1.5f);
                ImGui::TableSetupColumn("Hash", ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableHeadersRow();

                for (auto it = txs.rbegin(); it != txs.rend(); ++it) {
                    const auto& tx = *it;
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%lld", static_cast<long long>(tx.t));
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(tx.op.c_str());
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(tx.entityId.c_str());
                    ImGui::TableSetColumnIndex(3);
                    std::string sh = shortHash(tx.payloadHash, 10);
                    ImGui::TextUnformatted(sh.c_str());
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", tx.payloadHash.c_str());
                    }
                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextUnformatted(tx.timestamp.c_str());
                }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Backups")) {
            ui::sectionHeader("Backup Controls");
            ImGui::SetNextItemWidth(240.0f);
            ImGui::InputTextWithHint("##backup_label", "Optional label...", m_backupLabelBuf, sizeof(m_backupLabelBuf));
            ImGui::SameLine();
            if (ui::primaryButton("Create Backup")) {
                auto info = backups.createBackup(nm, m_backupLabelBuf, "manual");
                m_backupLabelBuf[0] = '\0';
                m_statusMsg = "Created backup: " + info.name;
                m_statusIsError = false;
                ui::pushToast(m_statusMsg, "success");
            }
            ImGui::SameLine();
            if (ui::subtleButton("Prune Automatic")) {
                backups.pruneAutomatic(12);
                m_statusMsg = "Automatic backups pruned";
                m_statusIsError = false;
                ui::pushToast(m_statusMsg, "info");
            }

            ImGui::Spacing();
            auto list = backups.listBackups();
            std::string pendingRestore;
            std::string pendingDelete;

            ImGuiTableFlags bFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                     ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##backups_table", 6, bFlags, ImVec2(0.0f, 0.0f))) {
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.0f);
                ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 82.0f);
                ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthFixed, 54.0f);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 84.0f);
                ImGui::TableSetupColumn("Checksum", ImGuiTableColumnFlags_WidthFixed, 108.0f);
                ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableHeadersRow();

                for (const auto& b : list) {
                    ImGui::TableNextRow();
                    ImGui::PushID(b.name.c_str());

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(b.name.c_str());
                    if (ImGui::IsItemHovered() && !b.createdAt.empty()) {
                        ImGui::SetTooltip("Created: %s", b.createdAt.c_str());
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImVec4 badgeCol = (b.source == "manual") ? pal.accent : pal.aqua;
                    ui::badge(b.source.c_str(), ImVec4(badgeCol.x, badgeCol.y, badgeCol.z, 0.22f), badgeCol);

                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", b.fileCount);

                    ImGui::TableSetColumnIndex(3);
                    std::string sz = formatBytes(b.totalBytes);
                    ImGui::TextUnformatted(sz.c_str());

                    ImGui::TableSetColumnIndex(4);
                    std::string ch = shortHash(b.checksum, 10);
                    ImGui::TextUnformatted(ch.c_str());
                    if (ImGui::IsItemHovered() && !b.checksum.empty()) {
                        ImGui::SetTooltip("%s", b.checksum.c_str());
                    }

                    ImGui::TableSetColumnIndex(5);
                    if (ui::subtleButton("Restore")) {
                        pendingRestore = b.name;
                    }
                    ImGui::SameLine();
                    if (ui::dangerButton("Delete")) {
                        pendingDelete = b.name;
                    }

                    ImGui::PopID();
                }
                ImGui::EndTable();
            }

            if (!pendingRestore.empty()) {
                std::string err;
                if (backups.restoreBackup(pendingRestore, nm, err)) {
                    app.graphNeedsRebuild = true;
                    m_statusMsg = "Restored backup: " + pendingRestore;
                    m_statusIsError = false;
                    ui::pushToast(m_statusMsg, "success");
                } else {
                    m_statusMsg = err;
                    m_statusIsError = true;
                    ui::pushToast(err, "error");
                }
            }
            if (!pendingDelete.empty()) {
                std::string err;
                if (backups.removeBackup(pendingDelete, err)) {
                    m_statusMsg = "Deleted backup: " + pendingDelete;
                    m_statusIsError = false;
                    ui::pushToast(m_statusMsg, "info");
                } else {
                    m_statusMsg = err;
                    m_statusIsError = true;
                    ui::pushToast(err, "error");
                }
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

}
