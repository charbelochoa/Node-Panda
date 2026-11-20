#include "asset_panel.h"
#include "app.h"
#include "asset_manager.h"
#include "ui_components.h"
#include "ui_theme.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>

namespace nodepanda {

namespace {

std::string toLowerCopy(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string formatSizeKb(size_t bytes) {
    char buf[64];
    double kb = static_cast<double>(bytes) / 1024.0;
    std::snprintf(buf, sizeof(buf), "%.1f KB", kb);
    return std::string(buf);
}

}

void AssetPanel::render(App& app) {
    ImGui::SetNextWindowSize(ImVec2(760, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Assets")) {
        ImGui::End();
        return;
    }

    const auto& pal = UITheme::palette();
    auto& am = app.getAssetManager();
    const auto& assets = am.getAssets();

    size_t verifiedCount = 0;
    size_t totalBytes = 0;
    for (const auto& a : assets) {
        if (a.verified) ++verifiedCount;
        totalBytes += a.sizeBytes;
    }

    float availW = ImGui::GetContentRegionAvail().x;
    float cardW = (availW - 16.0f) / 3.0f;
    if (cardW < 120.0f) cardW = 120.0f;

    ui::metricCard("##assets_total", "ASSETS", std::to_string(assets.size()), pal.accent, cardW);
    ImGui::SameLine();
    ui::metricCard("##assets_verified", "VERIFIED", std::to_string(verifiedCount) + " / " + std::to_string(assets.size()),
                   verifiedCount == assets.size() ? pal.success : pal.warning, cardW);
    ImGui::SameLine();
    ui::metricCard("##assets_size", "STORAGE", formatSizeKb(totalBytes), pal.aqua, cardW);

    ImGui::Spacing();
    ui::sectionHeader("Import Asset");

    ImGui::SetNextItemWidth(availW - 230.0f > 160.0f ? availW - 230.0f : 160.0f);
    ImGui::InputTextWithHint("##import_asset_path", "Path to file...", m_importPathBuf, sizeof(m_importPathBuf));
    ImGui::SameLine();
    if (ui::primaryButton("Import")) {
        std::string err;
        auto rec = am.importAsset(std::filesystem::path(m_importPathBuf), app.selectedNoteId, err);
        if (!err.empty()) {
            m_statusMsg = err;
            m_statusIsError = true;
            ui::pushToast(err, "error");
        } else {
            m_statusMsg = "Imported " + rec.filename;
            m_statusIsError = false;
            m_importPathBuf[0] = '\0';
            ui::pushToast(m_statusMsg, "success");
        }
    }
    ImGui::SameLine();
    if (ui::subtleButton("Verify All")) {
        bool ok = am.verifyAll();
        m_statusMsg = ok ? "All assets verified" : "Integrity mismatch detected";
        m_statusIsError = !ok;
        ui::pushToast(m_statusMsg, ok ? "success" : "error");
    }
    ImGui::SameLine();
    if (ui::subtleButton("Rescan")) {
        am.scanAssets();
        m_statusMsg = "Assets directory scanned";
        m_statusIsError = false;
    }

    if (!m_statusMsg.empty()) {
        ImGui::TextColored(m_statusIsError ? pal.danger : pal.success, "%s", m_statusMsg.c_str());
    }

    ImGui::Spacing();
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##asset_filter", "Filter by name, note, or hash...", m_filterBuf, sizeof(m_filterBuf));

    ImGui::Spacing();

    std::string pendingVerifyUuid;
    std::string pendingRemoveUuid;
    std::string filterLower = toLowerCopy(m_filterBuf);

    ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                 ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("##assets_table", 6, tableFlags, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn("Filename", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 84.0f);
        ImGui::TableSetupColumn("SHA-256", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 96.0f);
        ImGui::TableSetupColumn("Linked Note", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableHeadersRow();

        for (const auto& rec : assets) {
            if (!filterLower.empty()) {
                bool match = toLowerCopy(rec.filename).find(filterLower) != std::string::npos ||
                             toLowerCopy(rec.linkedNoteId).find(filterLower) != std::string::npos ||
                             toLowerCopy(rec.sha256).find(filterLower) != std::string::npos;
                if (!match) continue;
            }

            ImGui::TableNextRow();
            ImGui::PushID(rec.uuid.c_str());

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(rec.filename.c_str());

            ImGui::TableSetColumnIndex(1);
            std::string szStr = formatSizeKb(rec.sizeBytes);
            ImGui::TextUnformatted(szStr.c_str());

            ImGui::TableSetColumnIndex(2);
            std::string prefix = rec.sha256.size() > 12 ? rec.sha256.substr(0, 12) : rec.sha256;
            ImGui::TextUnformatted(prefix.c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", rec.sha256.c_str());
            }

            ImGui::TableSetColumnIndex(3);
            if (rec.verified) {
                ui::badge("OK", ImVec4(pal.success.x, pal.success.y, pal.success.z, 0.22f), pal.success);
            } else {
                ui::badge("MISMATCH", ImVec4(pal.danger.x, pal.danger.y, pal.danger.z, 0.22f), pal.danger);
            }

            ImGui::TableSetColumnIndex(4);
            if (!rec.linkedNoteId.empty()) {
                if (ui::subtleButton(rec.linkedNoteId.c_str())) {
                    app.selectedNoteId = rec.linkedNoteId;
                    app.currentState = AppState::ENTORNO_NOTAS;
                }
            } else {
                ImGui::TextDisabled("-");
            }

            ImGui::TableSetColumnIndex(5);
            if (ui::subtleButton("Verify")) {
                pendingVerifyUuid = rec.uuid;
            }
            ImGui::SameLine();
            if (ui::dangerButton("Del")) {
                pendingRemoveUuid = rec.uuid;
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    if (!pendingVerifyUuid.empty()) {
        bool ok = am.verifyAsset(pendingVerifyUuid);
        m_statusMsg = ok ? "Asset integrity OK" : "Asset checksum mismatch";
        m_statusIsError = !ok;
        ui::pushToast(m_statusMsg, ok ? "success" : "error");
    }
    if (!pendingRemoveUuid.empty()) {
        if (am.removeAsset(pendingRemoveUuid)) {
            m_statusMsg = "Asset removed";
            m_statusIsError = false;
            ui::pushToast(m_statusMsg, "info");
        }
    }

    ImGui::End();
}

}
