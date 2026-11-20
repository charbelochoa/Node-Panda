#include "rtc_panel.h"
#include "app.h"
#include "websocket_rtc.h"
#include "ui_theme.h"
#include "ui_components.h"
#include "imgui.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace nodepanda {

namespace {

bool parseHostPort(const std::string& input, std::string& hostOut, int& portOut) {
    std::string trimmed = input;
    while (!trimmed.empty() && trimmed.front() == ' ') trimmed.erase(trimmed.begin());
    while (!trimmed.empty() && trimmed.back() == ' ') trimmed.pop_back();
    if (trimmed.empty()) return false;

    size_t colon = trimmed.rfind(':');
    if (colon == std::string::npos) {
        hostOut = trimmed;
        portOut = 8043;
        return !hostOut.empty();
    }

    hostOut = trimmed.substr(0, colon);
    std::string portStr = trimmed.substr(colon + 1);
    if (hostOut.empty() || portStr.empty()) return false;
    int p = std::atoi(portStr.c_str());
    if (p <= 0 || p > 65535) return false;
    portOut = p;
    return true;
}

std::string formatAgeSeconds(int64_t unixTs) {
    if (unixTs <= 0) return "-";
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    int64_t diff = now - unixTs;
    if (diff <= 1) return "just now";
    if (diff < 60) return std::to_string(diff) + "s ago";
    return std::to_string(diff / 60) + "m ago";
}

ImVec4 eventTypeColor(const std::string& type, const ThemePalette& pal) {
    if (type == "hello" || type == "connect" || type == "server") return pal.success;
    if (type == "ping" || type == "pong") return pal.aqua;
    if (type == "presence") return pal.proViolet;
    if (type == "tx/batch" || type == "changed") return pal.accent;
    if (type == "disconnect" || type == "error") return pal.danger;
    return pal.textSecondary;
}

}

void RtcPanel::render(App& app) {
    const auto& pal = UITheme::palette();
    ImGui::SetNextWindowSize(ImVec2(860, 640), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Realtime WebSocket RTC")) {
        ImGui::End();
        return;
    }

    auto& rtc = app.getWebSocketRtcServer();
    RtcStats st = rtc.stats();

    if (!m_userInitialized) {
        if (const auto* user = app.getAuthManager().currentUser()) {
            if (!user->username.empty()) {
                std::snprintf(m_usernameBuf, sizeof(m_usernameBuf), "%s", user->username.c_str());
            }
        }
        m_port = st.port > 0 ? st.port : 8043;
        m_userInitialized = true;
    }

    std::string activeUser = (m_usernameBuf[0] != '\0') ? std::string(m_usernameBuf) : std::string("local");
    if (app.selectedNoteId != m_lastTrackedNoteId) {
        m_lastTrackedNoteId = app.selectedNoteId;
        if (st.serverRunning || st.connectedPeers > 0) {
            rtc.setLocalPresence(activeUser, app.selectedNoteId, m_blockUuidBuf);
        }
    }

    ui::statusDot(st.serverRunning ? "RTC ONLINE" : "RTC OFFLINE",
                  st.serverRunning ? pal.success : pal.danger,
                  st.serverRunning);
    ImGui::SameLine(0.0f, 12.0f);
    char wsAddrBuf[160];
    std::snprintf(wsAddrBuf, sizeof(wsAddrBuf), "ws:" "/" "/" "%s:%d",
                  rtc.getBindHost().c_str(), st.port);
    ui::badge(wsAddrBuf, pal.accentSoft, pal.accent);

    ImGui::SameLine(0.0f, 12.0f);
    ImGui::BeginDisabled(st.serverRunning);
    ImGui::SetNextItemWidth(76.0f);
    ImGui::InputInt("##rtc_port", &m_port, 0, 0);
    if (m_port < 1) m_port = 8043;
    if (m_port > 65535) m_port = 65535;
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (!st.serverRunning) {
        if (ui::primaryButton("Start WebSocket Server", ImVec2(185.0f, 0.0f))) {
            rtc.start(m_port, m_bindHostBuf);
            if (rtc.isRunning()) {
                m_statusMsg = "WebSocket RTC server started on port " + std::to_string(m_port);
                m_statusIsError = false;
                ui::pushToast(m_statusMsg, "success");
            } else {
                m_statusMsg = "Failed to bind port " + std::to_string(m_port);
                m_statusIsError = true;
                ui::pushToast(m_statusMsg, "error");
            }
        }
    } else {
        if (ui::dangerButton("Stop WebSocket Server", ImVec2(185.0f, 0.0f))) {
            rtc.stop();
            m_statusMsg = "WebSocket RTC server stopped";
            m_statusIsError = false;
            ui::pushToast(m_statusMsg, "info");
        }
    }

    ImGui::Spacing();
    float availW = ImGui::GetContentRegionAvail().x;
    float cardW = (availW - 24.0f) / 4.0f;
    if (cardW < 110.0f) cardW = 110.0f;

    ui::metricCard("##rtc_peers", "PEERS", std::to_string(st.connectedPeers), pal.aqua, cardW);
    ImGui::SameLine();
    ui::metricCard("##rtc_rev", "REVISION T", std::to_string(st.currentRev), pal.accent, cardW);
    ImGui::SameLine();
    ui::metricCard("##rtc_sent", "MSGS SENT", std::to_string(st.messagesSent), pal.success, cardW);
    ImGui::SameLine();
    ui::metricCard("##rtc_recv", "MSGS RECV", std::to_string(st.messagesReceived), pal.proViolet, cardW);

    if (!m_statusMsg.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(m_statusIsError ? pal.danger : pal.success, "%s", m_statusMsg.c_str());
    }

    ImGui::Spacing();
    ui::sectionHeader("Connect Remote Peer");
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##rtc_remote", "host:port", m_remoteAddrBuf, sizeof(m_remoteAddrBuf));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::InputTextWithHint("##rtc_user", "username", m_usernameBuf, sizeof(m_usernameBuf));
    ImGui::SameLine();
    if (ui::primaryButton("Connect Peer", ImVec2(124.0f, 0.0f))) {
        std::string host;
        int remotePort = 8043;
        if (!parseHostPort(m_remoteAddrBuf, host, remotePort)) {
            m_statusMsg = "Invalid host:port format";
            m_statusIsError = true;
            ui::pushToast(m_statusMsg, "error");
        } else {
            std::string err;
            if (rtc.connectRemotePeer(host, remotePort, activeUser, err)) {
                m_statusMsg = "Connected to " + host + ":" + std::to_string(remotePort);
                m_statusIsError = false;
                ui::pushToast(m_statusMsg, "success");
            } else {
                m_statusMsg = err;
                m_statusIsError = true;
                ui::pushToast(err, "error");
            }
        }
    }
    ImGui::SameLine();
    if (ui::subtleButton("Ping", ImVec2(72.0f, 0.0f))) {
        rtc.broadcastMessage("ping", "{}");
        m_statusMsg = "Ping sent to peers";
        m_statusIsError = false;
    }

    ImGui::Spacing();
    ui::sectionHeader("Quick Broadcast");
    ImGui::SetNextItemWidth(availW - 315.0f > 180.0f ? availW - 315.0f : 180.0f);
    ImGui::InputTextWithHint("##rtc_delta", "Quick note update payload...", m_deltaContentBuf, sizeof(m_deltaContentBuf));
    ImGui::SameLine();
    if (ui::primaryButton("Broadcast Note Update", ImVec2(180.0f, 0.0f))) {
        std::string noteId = app.selectedNoteId.empty() ? "workspace" : app.selectedNoteId;
        std::string content = m_deltaContentBuf;
        if (content.empty()) {
            if (Note* note = app.getNoteManager().getNoteById(noteId)) {
                content = note->title.empty() ? note->id : note->title;
                if (!note->content.empty()) {
                    std::string snippet = note->content.substr(0, 120);
                    content += ": " + snippet;
                }
            } else {
                content = "sync.note.update";
            }
        }
        rtc.broadcastDelta(noteId, "note.update", content);
        m_deltaContentBuf[0] = '\0';
        m_statusMsg = "Broadcasted update for " + noteId;
        m_statusIsError = false;
        ui::pushToast(m_statusMsg, "success");
    }
    ImGui::SameLine();
    if (ui::subtleButton("Sync Presence", ImVec2(118.0f, 0.0f))) {
        rtc.setLocalPresence(activeUser, app.selectedNoteId, m_blockUuidBuf);
        m_statusMsg = "Presence broadcasted";
        m_statusIsError = false;
    }

    ImGui::Spacing();
    ui::sectionHeader("Live Presence");
    std::vector<RtcPeer> peers = rtc.listPeers();
    ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                 ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("##rtc_presence_table", 5, tableFlags, ImVec2(0.0f, 145.0f))) {
        ImGui::TableSetupColumn("User", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 84.0f);
        ImGui::TableSetupColumn("Active Note", ImGuiTableColumnFlags_WidthStretch, 1.4f);
        ImGui::TableSetupColumn("Block", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Last Seen", ImGuiTableColumnFlags_WidthFixed, 88.0f);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(pal.accent, "%s (local)", activeUser.c_str());
        ImGui::TableSetColumnIndex(1);
        ImVec4 localCol = st.serverRunning ? pal.success : pal.textMuted;
        ui::badge(st.serverRunning ? "host" : "local",
                  ImVec4(localCol.x, localCol.y, localCol.z, 0.22f), localCol);
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(app.selectedNoteId.empty() ? "-" : app.selectedNoteId.c_str());
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(m_blockUuidBuf[0] != '\0' ? m_blockUuidBuf : "-");
        ImGui::TableSetColumnIndex(4);
        ImGui::TextUnformatted("now");

        for (const auto& p : peers) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(p.username.empty() ? p.peerId.c_str() : p.username.c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Peer ID: %s", p.peerId.c_str());
            }
            ImGui::TableSetColumnIndex(1);
            ImVec4 stCol = (p.status == "editing") ? pal.accent : pal.success;
            std::string stLabel = p.status.empty() ? "online" : p.status;
            ui::badge(stLabel.c_str(), ImVec4(stCol.x, stCol.y, stCol.z, 0.22f), stCol);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(p.editingNoteId.empty() ? "-" : p.editingNoteId.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(p.editingBlockUuid.empty() ? "-" : p.editingBlockUuid.c_str());
            ImGui::TableSetColumnIndex(4);
            std::string age = formatAgeSeconds(p.lastSeenUnix);
            ImGui::TextUnformatted(age.c_str());
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    ui::sectionHeader("Event Stream");
    std::vector<RtcEvent> events = rtc.recentEvents(60);
    if (ImGui::BeginTable("##rtc_events_table", 4, tableFlags, ImVec2(0.0f, 0.0f))) {
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 145.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 88.0f);
        ImGui::TableSetupColumn("Peer", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("Payload", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableHeadersRow();

        for (auto it = events.rbegin(); it != events.rend(); ++it) {
            const auto& ev = *it;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(pal.textMuted, "%s", ev.timestamp.c_str());
            ImGui::TableSetColumnIndex(1);
            ImVec4 col = eventTypeColor(ev.type, pal);
            ui::badge(ev.type.c_str(), ImVec4(col.x, col.y, col.z, 0.20f), col);
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(ev.peer.c_str());
            ImGui::TableSetColumnIndex(3);
            std::string preview = ev.payload.size() > 140 ? ev.payload.substr(0, 140) + "..." : ev.payload;
            ImGui::TextUnformatted(preview.c_str());
            if (ImGui::IsItemHovered() && !ev.payload.empty()) {
                ImGui::SetTooltip("%s", ev.payload.c_str());
            }
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

}
