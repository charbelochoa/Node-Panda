#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include "mobile_bridge.h"

#include <cstring>
#include <sstream>

namespace nodepanda {

namespace {

bool isPrivateLanIpv4(const std::string& ip) {
    if (ip.rfind("192.168.", 0) == 0) return true;
    if (ip.rfind("10.", 0) == 0) return true;
    if (ip.rfind("172.", 0) == 0) {
        size_t dot = ip.find('.', 4);
        if (dot != std::string::npos) {
            try {
                int second = std::stoi(ip.substr(4, dot - 4));
                if (second >= 16 && second <= 31) return true;
            } catch (...) {}
        }
    }
    return false;
}

std::string detectLocalIpv4() {
    std::string fallback = "127.0.0.1";
    std::string candidate;

#ifdef _WIN32
    WSADATA wsaData;
    bool started = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
    if (started) {
        char host[256] = {};
        if (gethostname(host, sizeof(host) - 1) == 0) {
            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            addrinfo* res = nullptr;
            if (getaddrinfo(host, nullptr, &hints, &res) == 0 && res != nullptr) {
                for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
                    auto* addr = reinterpret_cast<sockaddr_in*>(p->ai_addr);
                    char ipBuf[INET_ADDRSTRLEN] = {};
                    if (inet_ntop(AF_INET, &(addr->sin_addr), ipBuf, sizeof(ipBuf)) != nullptr) {
                        std::string ip(ipBuf);
                        if (ip == "127.0.0.1" || ip.rfind("169.254.", 0) == 0) continue;
                        if (isPrivateLanIpv4(ip)) {
                            candidate = ip;
                            break;
                        }
                        if (candidate.empty()) candidate = ip;
                    }
                }
                freeaddrinfo(res);
            }
        }
        WSACleanup();
    }
#else
    ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0 && ifaddr != nullptr) {
        for (ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            auto* addr = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr);
            char ipBuf[INET_ADDRSTRLEN] = {};
            if (inet_ntop(AF_INET, &(addr->sin_addr), ipBuf, sizeof(ipBuf)) != nullptr) {
                std::string ip(ipBuf);
                if (ip == "127.0.0.1" || ip.rfind("169.254.", 0) == 0) continue;
                if (isPrivateLanIpv4(ip)) {
                    candidate = ip;
                    break;
                }
                if (candidate.empty()) candidate = ip;
            }
        }
        freeifaddrs(ifaddr);
    }
#endif

    return candidate.empty() ? fallback : candidate;
}

}

std::string MobileBridge::getLocalLanHint(int apiPort) {
    int port = apiPort > 0 ? apiPort : 7788;
    std::string ip = detectLocalIpv4();
    return std::string("http:") + "/" + "/" + ip + ":" + std::to_string(port) + "/mobile";
}

std::string MobileBridge::renderManifestJson() {
    return std::string(
        "{\n"
        "  \"name\": \"Node Panda\",\n"
        "  \"short_name\": \"NodePanda\",\n"
        "  \"start_url\": \"/mobile\",\n"
        "  \"scope\": \"/\",\n"
        "  \"display\": \"standalone\",\n"
        "  \"orientation\": \"portrait-primary\",\n"
        "  \"background_color\": \"#151515\",\n"
        "  \"theme_color\": \"#257de0\",\n"
        "  \"icons\": [\n"
        "    {\n"
        "      \"src\": \"/mobile/icon.svg\",\n"
        "      \"sizes\": \"any\",\n"
        "      \"type\": \"image/svg+xml\",\n"
        "      \"purpose\": \"any maskable\"\n"
        "    }\n"
        "  ]\n"
        "}\n"
    );
}

std::string MobileBridge::renderServiceWorkerJs() {
    return std::string(R"SW(const CACHE_NAME = 'nodepanda-mobile-v1';
const SHELL_URLS = ['/mobile', '/manifest.json'];

self.addEventListener('install', function(event) {
  event.waitUntil(
    caches.open(CACHE_NAME).then(function(cache) {
      return cache.addAll(SHELL_URLS).catch(function() { return null; });
    })
  );
  self.skipWaiting();
});

self.addEventListener('activate', function(event) {
  event.waitUntil(
    caches.keys().then(function(keys) {
      return Promise.all(
        keys.filter(function(k) { return k !== CACHE_NAME; }).map(function(k) {
          return caches.delete(k);
        })
      );
    })
  );
  self.clients.claim();
});

self.addEventListener('fetch', function(event) {
  const req = event.request;
  if (req.method !== 'GET') return;
  const url = new URL(req.url);
  if (url.pathname.indexOf('/api/') === 0) {
    event.respondWith(
      fetch(req).catch(function() {
        return new Response(JSON.stringify({ error: 'offline' }), {
          status: 503,
          headers: { 'Content-Type': 'application/json' }
        });
      })
    );
    return;
  }
  event.respondWith(
    fetch(req).then(function(resp) {
      if (resp && resp.ok) {
        const clone = resp.clone();
        caches.open(CACHE_NAME).then(function(cache) { cache.put(req, clone); });
      }
      return resp;
    }).catch(function() {
      return caches.match(req);
    })
  );
});
)SW");
}

std::string MobileBridge::renderMobileAppHtml(int apiPort) {
    int port = apiPort > 0 ? apiPort : 7788;
    std::ostringstream html;
    html << R"HTML1(<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#257de0">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<link rel="manifest" href="/manifest.json">
<title>Node Panda</title>
<style>
:root{--bg:#151515;--s1:#191919;--s2:#1f1f1f;--s3:#282828;--b1:rgba(255,255,255,.08);--b2:rgba(255,255,255,.14);--tx:#f2f2f2;--tx2:#b3b3b3;--tx3:#7d7d7d;--ac:#257de0;--ach:#338ced;--acs:rgba(37,125,224,.18);--ok:#10a122;--wn:#fbb315;--dn:#dc3c3d;--r:10px;--nav:62px;--hdr:54px}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;padding:0;background:var(--bg);color:var(--tx);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Inter,sans-serif;font-size:15px;line-height:1.45;height:100%}
body{display:flex;flex-direction:column;overflow:hidden}
header{height:var(--hdr);background:var(--s1);border-bottom:1px solid var(--b1);display:flex;align-items:center;justify-content:space-between;padding:0 14px;flex-shrink:0}
.brand{display:flex;align-items:center;gap:9px;font-weight:700;font-size:16px;letter-spacing:-.01em}
.brand-dot{width:10px;height:10px;border-radius:50%;background:var(--ac);box-shadow:0 0 10px rgba(37,125,224,.65)}
.hdr-right{display:flex;align-items:center;gap:8px}
.pill{display:inline-flex;align-items:center;gap:6px;padding:4px 10px;border-radius:999px;background:var(--s2);border:1px solid var(--b1);font-size:12px;color:var(--tx2)}
.dot{width:7px;height:7px;border-radius:50%;background:var(--tx3)}
.dot.ok{background:var(--ok)}
.dot.err{background:var(--dn)}
.icon-btn{background:var(--s2);color:var(--tx);border:1px solid var(--b1);border-radius:8px;height:34px;padding:0 11px;font-size:13px;font-weight:600;cursor:pointer}
.icon-btn:active{background:var(--s3)}
main{flex:1;overflow-y:auto;-webkit-overflow-scrolling:touch;padding:12px 12px calc(var(--nav) + 18px)}
.wrap{max-width:780px;margin:0 auto}
.view{display:none}
.view.active{display:block}
.card{background:var(--s1);border:1px solid var(--b1);border-radius:var(--r);padding:12px;margin-bottom:10px}
.row{display:flex;align-items:center;gap:8px}
.row-between{display:flex;align-items:center;justify-content:space-between;gap:8px}
.col{display:flex;flex-direction:column;gap:8px}
input[type=text],textarea,select{width:100%;background:var(--s2);color:var(--tx);border:1px solid var(--b2);border-radius:8px;padding:10px 12px;font-size:14px;font-family:inherit;outline:none}
input[type=text]:focus,textarea:focus{border-color:var(--ac)}
textarea{min-height:84px;resize:vertical}
.mono{font-family:ui-monospace,SFMono-Regular,Consolas,monospace;font-size:13px}
.btn{background:var(--ac);color:#fff;border:none;border-radius:8px;padding:10px 14px;font-size:14px;font-weight:600;cursor:pointer;min-height:40px}
.btn:active{background:var(--ach)}
.btn-sub{background:var(--s3);color:var(--tx);border:1px solid var(--b1);border-radius:8px;padding:8px 12px;font-size:13px;font-weight:600;cursor:pointer;min-height:36px}
.btn-sub.active{background:var(--acs);color:#7cb8ff;border-color:var(--ac)}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin-bottom:10px}
.chip{background:var(--s2);color:var(--tx2);border:1px solid var(--b1);border-radius:999px;padding:6px 11px;font-size:12px;cursor:pointer}
.chip.active{background:var(--acs);color:#8ac0ff;border-color:var(--ac)}
.list-item{background:var(--s1);border:1px solid var(--b1);border-radius:var(--r);padding:12px;margin-bottom:8px;cursor:pointer}
.list-item:active{background:var(--s2)}
.title{font-weight:600;font-size:15px;color:var(--tx)}
.meta{font-size:12px;color:var(--tx3);margin-top:3px}
.badge{display:inline-block;padding:2px 8px;border-radius:999px;font-size:11px;font-weight:600;background:var(--s3);color:var(--tx2)}
.badge.ac{background:var(--acs);color:#8ac0ff}
.badge.ok{background:rgba(16,161,34,.18);color:#5fe06f}
.badge.wn{background:rgba(251,179,21,.18);color:#ffd15c}
.note-body{white-space:pre-wrap;word-break:break-word;font-size:14px;color:var(--tx);background:var(--s2);border:1px solid var(--b1);border-radius:8px;padding:12px;margin-top:10px;max-height:60vh;overflow-y:auto}
.task-row{display:flex;align-items:flex-start;gap:10px;background:var(--s1);border:1px solid var(--b1);border-radius:var(--r);padding:11px 12px;margin-bottom:8px}
.chk{width:22px;height:22px;border-radius:6px;border:2px solid var(--b2);background:var(--s2);flex-shrink:0;display:flex;align-items:center;justify-content:center;cursor:pointer;margin-top:1px;font-size:13px;font-weight:700;color:#fff}
.chk.done{background:var(--ok);border-color:var(--ok)}
.task-title.done{text-decoration:line-through;color:var(--tx3)}
table{width:100%;border-collapse:collapse;font-size:13px}
th,td{text-align:left;padding:8px 9px;border-bottom:1px solid var(--b1);vertical-align:top}
th{color:var(--tx2);background:var(--s2);font-weight:600}
.tbl-wrap{overflow-x:auto;border:1px solid var(--b1);border-radius:8px;background:var(--s1)}
.msg{background:var(--s2);border:1px solid var(--b1);border-radius:10px;padding:9px 11px;margin-bottom:8px}
.msg.me{border-color:rgba(37,125,224,.45);background:rgba(37,125,224,.10)}
.msg-head{display:flex;justify-content:space-between;font-size:12px;color:var(--tx2);margin-bottom:4px}
.msg-text{white-space:pre-wrap;word-break:break-word;font-size:14px}
nav{position:fixed;left:0;right:0;bottom:0;height:var(--nav);background:var(--s1);border-top:1px solid var(--b1);display:grid;grid-template-columns:repeat(5,1fr);padding-bottom:env(safe-area-inset-bottom);z-index:20}
.nav-btn{background:none;border:none;color:var(--tx3);display:flex;flex-direction:column;align-items:center;justify-content:center;gap:3px;font-size:11px;font-weight:600;cursor:pointer}
.nav-btn.active{color:var(--ac)}
.nav-ico{font-size:16px;line-height:1}
#toast{position:fixed;left:50%;bottom:calc(var(--nav) + 14px);transform:translateX(-50%);background:var(--s3);color:var(--tx);border:1px solid var(--b2);padding:9px 14px;border-radius:999px;font-size:13px;opacity:0;pointer-events:none;transition:opacity .2s;z-index:30;max-width:90vw;text-align:center}
#toast.show{opacity:1}
.empty{text-align:center;color:var(--tx3);padding:26px 12px;font-size:14px}
</style>
</head>
<body>
<header>
  <div class="brand"><span class="brand-dot"></span><span>Node Panda</span></div>
  <div class="hdr-right">
    <span class="pill" id="dbPill"><span class="dot" id="dbDot"></span><span id="dbLabel">DB</span></span>
    <button class="icon-btn" id="btnRefresh" type="button">Sync</button>
  </div>
</header>
<main>
  <div class="wrap">
    <section id="view-notas" class="view active">
      <div id="noteListPane">
        <div class="card">
          <input type="text" id="noteSearch" placeholder="Buscar notas...">
        </div>
        <div id="notesList"></div>
      </div>
      <div id="noteDetailPane" style="display:none">
        <div class="card">
          <div class="row-between">
            <button class="btn-sub" id="btnBackNotes" type="button">&larr; Volver</button>
            <span class="badge ac" id="noteDetailId"></span>
          </div>
          <div class="title" id="noteDetailTitle" style="margin-top:10px;font-size:17px"></div>
          <div class="chips" id="noteDetailLinks" style="margin-top:8px"></div>
          <div class="note-body" id="noteDetailContent"></div>
        </div>
      </div>
    </section>

    <section id="view-diario" class="view">
      <div class="card col">
        <input type="text" id="blockPage" placeholder="Pagina destino (vacio = diario de hoy)">
        <textarea id="blockContent" placeholder="Capturar bloque o idea rapida..."></textarea>
        <div class="row-between">
          <span class="meta" id="blocksCount">0 bloques</span>
          <button class="btn" id="btnAddBlock" type="button">Guardar Bloque</button>
        </div>
      </div>
      <div id="blocksList"></div>
    </section>

    <section id="view-tareas" class="view">
      <div class="card col">
        <div class="row">
          <input type="text" id="newTaskInput" placeholder="Nueva tarea...">
          <button class="btn" id="btnAddTask" type="button">Crear</button>
        </div>
        <div class="chips" style="margin-bottom:0">
          <button class="chip active" data-tfilter="all" type="button">Todas</button>
          <button class="chip" data-tfilter="open" type="button">Pendientes</button>
          <button class="chip" data-tfilter="done" type="button">Completadas</button>
        </div>
      </div>
      <div id="tasksList"></div>
    </section>

    <section id="view-datalog" class="view">
      <div class="card col">
        <div class="chips" style="margin-bottom:0">
          <button class="chip" data-dl="[:find ?p ?t :where [?p :page/title ?t]]" type="button">Paginas</button>
          <button class="chip" data-dl="[:find ?b ?c :where [?b :block/content ?c]]" type="button">Bloques</button>
          <button class="chip" data-dl="[:find ?a ?b :where [?a :link/to ?b]]" type="button">Enlaces</button>
        </div>
        <textarea id="datalogInput" class="mono">[:find ?p ?t :where [?p :page/title ?t]]</textarea>
        <div class="row-between">
          <span class="meta" id="datalogMeta"></span>
          <button class="btn" id="btnRunDatalog" type="button">Ejecutar</button>
        </div>
      </div>
      <div id="datalogOutput"></div>
    </section>

    <section id="view-agentes" class="view">
      <div class="card col">
        <div class="row-between">
          <span class="title">Sala de Agentes</span>
          <input type="text" id="roomInput" value="general" style="width:120px;padding:6px 9px">
        </div>
        <div id="messagesList" style="max-height:36vh;overflow-y:auto"></div>
        <div class="row">
          <input type="text" id="msgInput" placeholder="Mensaje o instruccion...">
          <button class="btn" id="btnSendMsg" type="button">Enviar</button>
        </div>
        <button class="btn-sub" id="btnLaunchRun" type="button">Lanzar Mision IA</button>
      </div>
      <div class="card">
        <div class="row-between" style="margin-bottom:8px">
          <span class="title">Ejecuciones</span>
          <span class="meta" id="runsCount">0</span>
        </div>
        <div id="runsList"></div>
      </div>
    </section>
  </div>
</main>

<nav>
  <button class="nav-btn active" data-tab="notas" type="button"><span class="nav-ico">&#9776;</span><span>Notas</span></button>
  <button class="nav-btn" data-tab="diario" type="button"><span class="nav-ico">&#9998;</span><span>Diario</span></button>
  <button class="nav-btn" data-tab="tareas" type="button"><span class="nav-ico">&#9745;</span><span>Tareas</span></button>
  <button class="nav-btn" data-tab="datalog" type="button"><span class="nav-ico">&#9670;</span><span>Datalog</span></button>
  <button class="nav-btn" data-tab="agentes" type="button"><span class="nav-ico">&#9889;</span><span>Agentes</span></button>
</nav>
<div id="toast"></div>

<script>
(function(){
  const DEFAULT_PORT = )HTML1" << port << R"HTML2(;
  const API_BASE = (window.location.protocol === 'file:')
    ? ('http:' + '\x2f\x2f127.0.0.1:' + DEFAULT_PORT)
    : '';

  const state = {
    tab: 'notas',
    notes: [],
    blocks: [],
    tasks: [],
    toggledTasks: {},
    taskFilter: 'all',
    runs: [],
    messages: []
  };

  function el(id){ return document.getElementById(id); }

  function esc(s){
    return String(s == null ? '' : s)
      .replace(/&/g,'&amp;')
      .replace(/</g,'&lt;')
      .replace(/>/g,'&gt;')
      .replace(/"/g,'&quot;');
  }

  let toastTimer = null;
  function toast(msg){
    const t = el('toast');
    t.textContent = msg;
    t.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(function(){ t.classList.remove('show'); }, 2400);
  }

  async function apiGet(path){
    const res = await fetch(API_BASE + path);
    if (!res.ok) throw new Error('HTTP ' + res.status);
    return res.json();
  }

  async function apiPost(path, payload){
    const res = await fetch(API_BASE + path, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    const data = await res.json();
    if (!res.ok) throw new Error(data.error || ('HTTP ' + res.status));
    return data;
  }

  async function loadDbStatus(){
    try {
      const db = await apiGet('/api/db/status');
      const dot = el('dbDot');
      dot.className = 'dot ' + (db.connected ? 'ok' : 'err');
      el('dbLabel').textContent = (db.engine || 'Local') + (db.connected ? '' : ' (off)');
    } catch (e) {
      el('dbDot').className = 'dot err';
      el('dbLabel').textContent = 'Offline';
    }
  }

  function renderNotes(){
    const q = el('noteSearch').value.trim().toLowerCase();
    const filtered = state.notes.filter(function(n){
      if (!q) return true;
      return (n.id || '').toLowerCase().indexOf(q) >= 0 ||
             (n.titulo || '').toLowerCase().indexOf(q) >= 0;
    });
    if (!filtered.length){
      el('notesList').innerHTML = '<div class="empty">Sin notas disponibles</div>';
      return;
    }
    el('notesList').innerHTML = filtered.map(function(n){
      return '<div class="list-item" data-note-id="' + esc(n.id) + '">' +
        '<div class="row-between"><span class="title">' + esc(n.titulo || n.id) + '</span>' +
        '<span class="badge ac">' + (n.enlaces_count || 0) + ' enlaces</span></div>' +
        '<div class="meta">' + esc(n.id) + ' &middot; ' + (n.palabras || 0) + ' chars</div>' +
        '</div>';
    }).join('');
    el('notesList').querySelectorAll('.list-item').forEach(function(item){
      item.addEventListener('click', function(){
        openNote(item.getAttribute('data-note-id'));
      });
    });
  }

  async function loadNotes(){
    try {
      state.notes = await apiGet('/api/notas');
      renderNotes();
    } catch (e) {
      el('notesList').innerHTML = '<div class="empty">Error cargando notas</div>';
    }
  }

  async function openNote(id){
    try {
      const note = await apiGet('/api/notas/' + encodeURIComponent(id));
      el('noteDetailId').textContent = note.id || id;
      el('noteDetailTitle').textContent = note.titulo || id;
      el('noteDetailContent').textContent = note.contenido || '';
      const links = Array.isArray(note.enlaces) ? note.enlaces : [];
      el('noteDetailLinks').innerHTML = links.map(function(l){
        return '<button class="chip" data-open-link="' + esc(l) + '" type="button">[[' + esc(l) + ']]</button>';
      }).join('');
      el('noteDetailLinks').querySelectorAll('[data-open-link]').forEach(function(b){
        b.addEventListener('click', function(){ openNote(b.getAttribute('data-open-link')); });
      });
      el('noteListPane').style.display = 'none';
      el('noteDetailPane').style.display = 'block';
    } catch (e) {
      toast('No se pudo abrir la nota');
    }
  }

  function renderBlocks(){
    el('blocksCount').textContent = state.blocks.length + ' bloques';
    if (!state.blocks.length){
      el('blocksList').innerHTML = '<div class="empty">Sin bloques registrados</div>';
      return;
    }
    const items = state.blocks.slice(-40).reverse();
    el('blocksList').innerHTML = items.map(function(b){
      return '<div class="card">' +
        '<div class="row-between"><span class="badge ac">' + esc(b.page || 'diario') + '</span>' +
        '<span class="meta mono">' + esc((b.uuid || '').slice(0, 8)) + '</span></div>' +
        '<div style="margin-top:6px;white-space:pre-wrap;word-break:break-word">' + esc(b.content || '') + '</div>' +
        '</div>';
    }).join('');
  }

  async function loadBlocks(){
    try {
      state.blocks = await apiGet('/api/v1/blocks');
      renderBlocks();
    } catch (e) {
      el('blocksList').innerHTML = '<div class="empty">Error cargando bloques</div>';
    }
  }

  async function addBlock(){
    const content = el('blockContent').value.trim();
    const page = el('blockPage').value.trim();
    if (!content){ toast('Escribe contenido primero'); return; }
    try {
      await apiPost('/api/v1/blocks', { page: page, content: content });
      el('blockContent').value = '';
      toast('Bloque guardado');
      await loadBlocks();
    } catch (e) {
      toast('Error: ' + e.message);
    }
  }

  function effectiveTaskStatus(t){
    if (state.toggledTasks[t.uuid]) return state.toggledTasks[t.uuid];
    return t.status || 'TODO';
  }

  function renderTasks(){
    const filtered = state.tasks.filter(function(t){
      const st = effectiveTaskStatus(t);
      const isDone = (st === 'DONE' || st === 'CANCELED');
      if (state.taskFilter === 'open') return !isDone;
      if (state.taskFilter === 'done') return isDone;
      return true;
    });
    if (!filtered.length){
      el('tasksList').innerHTML = '<div class="empty">Sin tareas en esta vista</div>';
      return;
    }
    el('tasksList').innerHTML = filtered.map(function(t){
      const st = effectiveTaskStatus(t);
      const done = (st === 'DONE');
      const badgeCls = done ? 'ok' : (st === 'DOING' || st === 'NOW' ? 'wn' : 'ac');
      return '<div class="task-row">' +
        '<div class="chk ' + (done ? 'done' : '') + '" data-toggle-task="' + esc(t.uuid) + '">' + (done ? '&#10003;' : '') + '</div>' +
        '<div style="flex:1;min-width:0">' +
          '<div class="title task-title ' + (done ? 'done' : '') + '">' + esc(t.title || '') + '</div>' +
          '<div class="meta"><span class="badge ' + badgeCls + '">' + esc(st) + '</span> ' +
          (t.priority ? '<span class="badge">' + esc(t.priority) + '</span> ' : '') +
          esc(t.page || '') + '</div>' +
        '</div>' +
      '</div>';
    }).join('');
    el('tasksList').querySelectorAll('[data-toggle-task]').forEach(function(btn){
      btn.addEventListener('click', function(){
        toggleTask(btn.getAttribute('data-toggle-task'));
      });
    });
  }

  async function loadTasks(){
    try {
      state.tasks = await apiGet('/api/v1/tasks');
      renderTasks();
    } catch (e) {
      el('tasksList').innerHTML = '<div class="empty">Error cargando tareas</div>';
    }
  }

  async function toggleTask(uuid){
    const task = state.tasks.find(function(x){ return x.uuid === uuid; });
    if (!task) return;
    const cur = effectiveTaskStatus(task);
    const next = (cur === 'DONE') ? 'TODO' : 'DONE';
    state.toggledTasks[uuid] = next;
    renderTasks();
    try {
      await apiPost('/api/v1/blocks', { page: task.page || '', content: next + ' ' + task.title });
      toast('Tarea marcada como ' + next);
    } catch (e) {
      toast('Cambio local guardado');
    }
  }

  async function addTask(){
    const text = el('newTaskInput').value.trim();
    if (!text) return;
    try {
      await apiPost('/api/v1/blocks', { page: '', content: 'TODO ' + text });
      el('newTaskInput').value = '';
      toast('Tarea creada');
      await loadTasks();
    } catch (e) {
      toast('Error: ' + e.message);
    }
  }

  async function runDatalog(){
    const query = el('datalogInput').value.trim();
    if (!query) return;
    try {
      const rows = await apiPost('/api/v1/datalog', { query: query });
      el('datalogMeta').textContent = rows.length + ' filas';
      if (!rows.length){
        el('datalogOutput').innerHTML = '<div class="empty">Consulta sin resultados</div>';
        return;
      }
      const cols = Object.keys(rows[0]);
      let tbl = '<div class="tbl-wrap"><table><thead><tr>' +
        cols.map(function(c){ return '<th>' + esc(c) + '</th>'; }).join('') +
        '</tr></thead><tbody>';
      tbl += rows.map(function(r){
        return '<tr>' + cols.map(function(c){ return '<td class="mono">' + esc(r[c]) + '</td>'; }).join('') + '</tr>';
      }).join('');
      tbl += '</tbody></table></div>';
      el('datalogOutput').innerHTML = tbl;
    } catch (e) {
      el('datalogMeta').textContent = 'Error';
      el('datalogOutput').innerHTML = '<div class="card" style="color:#ff7b7c">' + esc(e.message) + '</div>';
    }
  }

  async function loadAgents(){
    const room = el('roomInput').value.trim() || 'general';
    try {
      const [msgs, runs] = await Promise.all([
        apiGet('/api/messages?room=' + encodeURIComponent(room)),
        apiGet('/api/runs')
      ]);
      state.messages = msgs;
      state.runs = runs;
      if (!msgs.length){
        el('messagesList').innerHTML = '<div class="empty">Sin mensajes en ' + esc(room) + '</div>';
      } else {
        el('messagesList').innerHTML = msgs.map(function(m){
          const mine = (m.from === 'movil' || m.from === 'usuario');
          return '<div class="msg ' + (mine ? 'me' : '') + '">' +
            '<div class="msg-head"><strong>' + esc(m.from) + '</strong><span>&rarr; ' + esc(m.to) + '</span></div>' +
            '<div class="msg-text">' + esc(m.text) + '</div></div>';
        }).join('');
        el('messagesList').scrollTop = el('messagesList').scrollHeight;
      }
      el('runsCount').textContent = runs.length + ' runs';
      if (!runs.length){
        el('runsList').innerHTML = '<div class="empty">Sin ejecuciones activas</div>';
      } else {
        el('runsList').innerHTML = runs.slice().reverse().map(function(r){
          return '<div class="list-item" style="cursor:default">' +
            '<div class="row-between"><span class="title">' + esc(r.title || r.id) + '</span>' +
            '<span class="badge ac">' + esc(r.status) + '</span></div>' +
            '<div class="meta">' + esc(r.model) + ' &middot; ' + esc(r.cost) + '</div></div>';
        }).join('');
      }
    } catch (e) {
      toast('Error cargando agentes');
    }
  }

  async function sendMessage(){
    const text = el('msgInput').value.trim();
    const room = el('roomInput').value.trim() || 'general';
    if (!text) return;
    try {
      await apiPost('/api/messages', { room: room, from: 'movil', to: 'todos', text: text });
      el('msgInput').value = '';
      await loadAgents();
    } catch (e) {
      toast('Error: ' + e.message);
    }
  }

  async function launchRun(){
    const prompt = el('msgInput').value.trim();
    const room = el('roomInput').value.trim() || 'general';
    if (!prompt){ toast('Escribe una instruccion primero'); return; }
    try {
      await apiPost('/api/runs', { prompt: prompt, model: 'auto', room: room });
      el('msgInput').value = '';
      toast('Mision lanzada');
      await loadAgents();
    } catch (e) {
      toast('Error: ' + e.message);
    }
  }

  function switchTab(tab){
    state.tab = tab;
    document.querySelectorAll('.view').forEach(function(v){ v.classList.remove('active'); });
    document.querySelectorAll('.nav-btn').forEach(function(b){ b.classList.remove('active'); });
    const target = el('view-' + tab);
    if (target) target.classList.add('active');
    const navBtn = document.querySelector('.nav-btn[data-tab="' + tab + '"]');
    if (navBtn) navBtn.classList.add('active');
    refreshCurrent();
  }

  function refreshCurrent(){
    loadDbStatus();
    if (state.tab === 'notas') loadNotes();
    else if (state.tab === 'diario') loadBlocks();
    else if (state.tab === 'tareas') loadTasks();
    else if (state.tab === 'agentes') loadAgents();
  }

  document.querySelectorAll('.nav-btn').forEach(function(b){
    b.addEventListener('click', function(){ switchTab(b.getAttribute('data-tab')); });
  });
  el('btnRefresh').addEventListener('click', function(){ refreshCurrent(); toast('Actualizado'); });
  el('noteSearch').addEventListener('input', renderNotes);
  el('btnBackNotes').addEventListener('click', function(){
    el('noteDetailPane').style.display = 'none';
    el('noteListPane').style.display = 'block';
  });
  el('btnAddBlock').addEventListener('click', addBlock);
  el('btnAddTask').addEventListener('click', addTask);
  el('newTaskInput').addEventListener('keydown', function(ev){ if (ev.key === 'Enter') addTask(); });
  document.querySelectorAll('[data-tfilter]').forEach(function(c){
    c.addEventListener('click', function(){
      document.querySelectorAll('[data-tfilter]').forEach(function(x){ x.classList.remove('active'); });
      c.classList.add('active');
      state.taskFilter = c.getAttribute('data-tfilter');
      renderTasks();
    });
  });
  document.querySelectorAll('[data-dl]').forEach(function(c){
    c.addEventListener('click', function(){
      el('datalogInput').value = c.getAttribute('data-dl');
      runDatalog();
    });
  });
  el('btnRunDatalog').addEventListener('click', runDatalog);
  el('btnSendMsg').addEventListener('click', sendMessage);
  el('msgInput').addEventListener('keydown', function(ev){ if (ev.key === 'Enter') sendMessage(); });
  el('btnLaunchRun').addEventListener('click', launchRun);
  el('roomInput').addEventListener('change', loadAgents);

  if ('serviceWorker' in navigator && window.location.protocol !== 'file:'){
    navigator.serviceWorker.register('/sw.js').catch(function(){});
  }

  refreshCurrent();
})();
</script>
</body>
</html>
)HTML2";
    return html.str();
}

}
