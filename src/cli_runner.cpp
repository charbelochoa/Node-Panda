#include "cli_runner.h"
#include "app.h"
#include "mobile_bridge.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace nodepanda {

std::string CliRunner::getFlag(const std::vector<std::string>& args, const std::string& flag, const std::string& def) {
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == flag && i + 1 < args.size()) {
            return args[i + 1];
        }
        std::string prefix = flag + "=";
        if (args[i].rfind(prefix, 0) == 0) {
            return args[i].substr(prefix.size());
        }
    }
    return def;
}

bool CliRunner::hasFlag(const std::vector<std::string>& args, const std::string& flag) {
    return std::find(args.begin(), args.end(), flag) != args.end();
}

void CliRunner::printHelp() const {
    std::cout << "Node Panda CLI v2.1.0\n\n";
    std::cout << "Uso: NodePanda [comando] [opciones]\n\n";
    std::cout << "Modos de ejecucion:\n";
    std::cout << "  (sin argumentos)          Autenticacion en terminal y selector GUI / Servidor / Chat\n";
    std::cout << "  --gui                     Iniciar interfaz grafica compilada tras autenticacion\n";
    std::cout << "  --server [--port 8042]    Iniciar servidor REST, WebSocket RTC y Companion PWA\n\n";
    std::cout << "Autenticacion:\n";
    std::cout << "  login                     Iniciar sesion interactiva en terminal\n";
    std::cout << "  register                  Registrar nueva cuenta de usuario\n";
    std::cout << "  logout                    Cerrar sesion local\n\n";
    std::cout << "Agentes de IA y Laboratorio:\n";
    std::cout << "  chat [--name N] [--room R] [--model M]  Consola interactiva de agentes\n";
    std::cout << "  run <prompt> [--model M] [--wait]       Ejecutar mision multi-agente\n";
    std::cout << "  runs                                    Listar misiones\n";
    std::cout << "  models                                  Mostrar catalogo de proveedores y modelos\n";
    std::cout << "  skills                                  Listar skills disponibles\n";
    std::cout << "  recall <query>                          Recuperar contexto del grafo\n";
    std::cout << "  profile <csv>                           Perfilar tabla CSV\n";
    std::cout << "  analyze <csv> [--agg count|sum|avg]     Analizar datos tabulares\n";
    std::cout << "  export [--out file.csv]                 Exportar consumo a CSV\n\n";
    std::cout << "Grafo, Bloques, Tareas y Datalog:\n";
    std::cout << "  status                                  Estado general del motor\n";
    std::cout << "  list <page|task|block|property|asset>   Listar entidades\n";
    std::cout << "  show --page <nombre>                    Mostrar arbol de bloques\n";
    std::cout << "  upsert block --target-page P --content T Crear bloque\n";
    std::cout << "  upsert task --content T [--priority A]  Crear tarea\n";
    std::cout << "  search <texto>                          Buscar en notas y bloques\n";
    std::cout << "  query <datalog|--name nombre>           Ejecutar consulta Datalog\n";
    std::cout << "  backup <list|create|restore|remove>     Gestionar respaldos\n";
    std::cout << "  sync <status|checksum|export|import>    Sincronizacion y E2EE\n";
    std::cout << "  db <status|config|init|sync|query>      Base de datos PostgreSQL / MySQL\n\n";
    std::cout << "Vectores 384D, PDF, Tablas, RTC y Companion Movil:\n";
    std::cout << "  vector <query> [--k 10]                 Busqueda vectorial hibrida 384D\n";
    std::cout << "  rtc status                              Estado del servidor WebSocket RTC (RFC 6455)\n";
    std::cout << "  pdf <demo|open archivo.pdf|list>        Generar o inspeccionar documentos PDF\n";
    std::cout << "  table list                              Listar vistas de tabla y conteo de filas\n";
    std::cout << "  mobile [--port 8042]                    Mostrar URL LAN del Companion Movil PWA\n";
    std::cout << "  doctor                                  Diagnostico del entorno\n";
}

bool CliRunner::ensureAuthenticated(App& app, bool skipPrompt) {
    if (skipPrompt) {
        return true;
    }
    AuthSession existing;
    if (app.getAuthManager().loadSavedCliSession(existing)) {
        for (const auto& u : app.getAuthManager().getUsers()) {
            if (u.id == existing.userId) {
                app.getAuthManager().setActiveUser(u);
                break;
            }
        }
        return true;
    }
    UserAccount user;
    std::string token;
    return app.getAuthManager().interactiveTerminalLogin(user, token);
}

int CliRunner::runInteractiveLauncher(App& app, CliLaunchResult& outResult) {
    std::cout << "\nSelecciona modo de ejecucion:\n";
    std::cout << "  [1] Abrir aplicacion grafica compilada (OpenGL / ImGui)\n";
    std::cout << "  [2] Iniciar servidor local REST + Agentes (puerto " << outResult.serverPort << ")\n";
    std::cout << "  [3] Consola interactiva de agentes (chat)\n";
    std::cout << "  [4] Configurar base de datos (PostgreSQL / MySQL)\n";
    std::cout << "  [0] Salir\n";
    std::cout << "\nOpcion [1]: " << std::flush;

    std::string choice;
    if (!std::getline(std::cin, choice)) {
        outResult.mode = LaunchMode::GuiApp;
        return 0;
    }
    if (choice.empty() || choice == "1") {
        outResult.mode = LaunchMode::GuiApp;
        return 0;
    }
    if (choice == "2") {
        outResult.mode = LaunchMode::HeadlessServer;
        return 0;
    }
    if (choice == "3") {
        outResult.mode = LaunchMode::CliFinished;
        return runChatRepl(app, "atlas", "general", "auto");
    }
    if (choice == "4") {
        std::string engineStr, modeStr, host, portStr, dbName, user;
        std::cout << "Motor (postgresql / mysql / local) [postgresql]: " << std::flush;
        std::getline(std::cin, engineStr);
        if (engineStr.empty()) engineStr = "postgresql";
        std::cout << "Modo (local / server) [local]: " << std::flush;
        std::getline(std::cin, modeStr);
        if (modeStr.empty()) modeStr = "local";
        std::cout << "Host [127.0.0.1]: " << std::flush;
        std::getline(std::cin, host);
        if (host.empty()) host = "127.0.0.1";
        std::string defPort = (DatabaseManager::stringToEngine(engineStr) == DbEngineType::MySQL) ? "3306" : "5432";
        std::cout << "Puerto [" << defPort << "]: " << std::flush;
        std::getline(std::cin, portStr);
        if (portStr.empty()) portStr = defPort;
        std::cout << "Base de datos [nodepanda]: " << std::flush;
        std::getline(std::cin, dbName);
        if (dbName.empty()) dbName = "nodepanda";
        std::cout << "Usuario [nodepanda]: " << std::flush;
        std::getline(std::cin, user);
        if (user.empty()) user = "nodepanda";
        std::string pass = AuthManager::readPasswordMasked("Contrasena DB: ");

        DbConfig cfg;
        cfg.engine = DatabaseManager::stringToEngine(engineStr);
        cfg.deployMode = (modeStr == "server") ? DbDeployMode::Server : DbDeployMode::Local;
        cfg.host = host;
        try { cfg.port = std::stoi(portStr); } catch (...) { cfg.port = std::stoi(defPort); }
        cfg.database = dbName;
        cfg.username = user;
        cfg.password = pass;
        app.getDatabaseManager().configure(cfg);
        std::string details;
        app.getDatabaseManager().testConnection(details);
        std::cout << "Resultado conexion: " << details << "\n";
        outResult.mode = LaunchMode::GuiApp;
        return 0;
    }

    outResult.mode = LaunchMode::CliFinished;
    return 0;
}

int CliRunner::runChatRepl(App& app, const std::string& agentName, const std::string& room, const std::string& model) {
    std::string activeModel = model;
    std::cout << "\nNode Panda Agent REPL | agente=" << agentName << " | sala=" << room << " | modelo=" << activeModel << "\n";
    std::cout << "Comandos: /help /model <id> /message <to> <texto> /messages /recall <q> /db /exit\n\n";

    std::string line;
    while (true) {
        std::cout << agentName << " > " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (line == "/exit" || line == "/quit") break;
        if (line == "/help") {
            std::cout << "/model <id> | /message <destino> <texto> | /messages | /recall <consulta> | /db | /exit\n";
            continue;
        }
        if (line.rfind("/model ", 0) == 0) {
            activeModel = line.substr(7);
            std::cout << "Modelo activo: " << activeModel << "\n";
            continue;
        }
        if (line == "/messages") {
            for (const auto& m : app.getAgentEngine().listMessages(room)) {
                std::cout << "[" << m.from << " -> " << m.to << "] " << m.text << "\n";
            }
            continue;
        }
        if (line.rfind("/message ", 0) == 0) {
            std::istringstream iss(line.substr(9));
            std::string to;
            iss >> to;
            std::string rest;
            std::getline(iss, rest);
            if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
            app.getAgentEngine().postMessage(room, agentName, to, rest);
            std::cout << "Mensaje enviado a " << to << "\n";
            continue;
        }
        if (line.rfind("/recall ", 0) == 0) {
            auto ctx = app.getContextBrain().retrieve(app.getNoteManager(), line.substr(8), 1800);
            std::cout << "Hits: " << ctx.hits.size() << " | Tokens estimados: " << ctx.estimatedTokens << "\n";
            std::cout << ctx.formattedXml << "\n";
            continue;
        }
        if (line == "/db") {
            auto st = app.getDatabaseManager().getStatus();
            std::cout << "DB: " << st.engineName << " (" << st.modeName << ") conectado=" << (st.connected ? "si" : "no") << "\n";
            continue;
        }

        std::string owner = app.getAuthManager().currentUser() ? app.getAuthManager().currentUser()->username : "cli";
        auto run = app.getAgentEngine().createAndStartRun(owner, line, activeModel, "auto", room);
        std::cout << "Mision iniciada: " << run.id << "\n";
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            const auto* cur = app.getAgentEngine().findRun(run.id);
            if (!cur) break;
            if (cur->status != "queued" && cur->status != "running") {
                std::cout << "[" << cur->status << "] " << AiCatalog::formatUsdFromNano(cur->costNano) << " USD\n";
                if (!cur->result.empty()) std::cout << cur->result << "\n";
                if (!cur->error.empty()) std::cerr << "Error: " << cur->error << "\n";
                break;
            }
        }
    }
    return 0;
}

int CliRunner::handleListCommand(App& app, const std::vector<std::string>& args) {
    std::string entity = args.size() > 1 ? args[1] : "page";
    if (entity == "page") {
        const auto& notes = app.getNoteManager().getAllNotes();
        std::cout << "ID\tTITULO\tENLACES\n";
        for (const auto& [id, note] : notes) {
            std::cout << id << "\t" << note.title << "\t" << note.links.size() << "\n";
        }
        std::cout << "Count: " << notes.size() << "\n";
        return 0;
    }
    if (entity == "task") {
        std::string st = getFlag(args, "--status");
        std::string pr = getFlag(args, "--priority");
        auto tasks = app.getTaskEngine().filterTasks(st, pr, "");
        for (const auto& t : tasks) {
            std::cout << t.uuid.substr(0, 8) << "\t" << TaskEngine::statusToString(t.status)
                      << "\t" << TaskEngine::priorityToString(t.priority) << "\t" << t.title << "\n";
        }
        std::cout << "Count: " << tasks.size() << "\n";
        return 0;
    }
    if (entity == "block") {
        const auto& blocks = app.getOutlinerEngine().getAllBlocks();
        for (const auto& [uuid, b] : blocks) {
            std::cout << uuid.substr(0, 8) << "\t" << b.pageId << "\t" << b.content << "\n";
        }
        std::cout << "Count: " << blocks.size() << "\n";
        return 0;
    }
    if (entity == "property") {
        auto schemas = app.getPropertyEngine().listSchemas();
        for (const auto& s : schemas) {
            std::cout << s.name << "\t" << PropertyEngine::typeToString(s.type) << "\n";
        }
        std::cout << "Count: " << schemas.size() << "\n";
        return 0;
    }
    if (entity == "asset") {
        const auto& assets = app.getAssetManager().getAssets();
        for (const auto& a : assets) {
            std::cout << a.uuid.substr(0, 8) << "\t" << a.filename << "\t" << a.sizeBytes << " B\t"
                      << (a.verified ? "OK" : "MISMATCH") << "\n";
        }
        std::cout << "Count: " << assets.size() << "\n";
        return 0;
    }
    std::cerr << "Entidad desconocida: " << entity << "\n";
    return 1;
}

int CliRunner::handleUpsertCommand(App& app, const std::vector<std::string>& args) {
    std::string target = args.size() > 1 ? args[1] : "block";
    if (target == "block") {
        std::string page = getFlag(args, "--target-page");
        std::string content = getFlag(args, "--content");
        if (content.empty()) {
            std::cerr << "Se requiere --content\n";
            return 1;
        }
        if (page.empty()) page = app.getJournalEngine().ensureTodayJournal(app.getNoteManager());
        std::string uuid = app.getOutlinerEngine().createBlock(app.getNoteManager(), page, content);
        app.rebuildKnowledgeIndices();
        std::cout << "Bloque creado: " << uuid << " en " << page << "\n";
        return 0;
    }
    if (target == "task") {
        std::string page = getFlag(args, "--target-page");
        std::string content = getFlag(args, "--content");
        std::string prio = getFlag(args, "--priority", "medium");
        std::string sched = getFlag(args, "--scheduled");
        std::string dead = getFlag(args, "--deadline");
        if (content.empty()) {
            std::cerr << "Se requiere --content\n";
            return 1;
        }
        if (page.empty()) page = app.getJournalEngine().ensureTodayJournal(app.getNoteManager());
        std::string uuid = app.getTaskEngine().createTask(
            app.getNoteManager(), page, content, TaskStatus::Todo,
            TaskEngine::stringToPriority(prio), sched, dead);
        app.rebuildKnowledgeIndices();
        std::cout << "Tarea creada: " << uuid << "\n";
        return 0;
    }
    if (target == "page") {
        std::string name = getFlag(args, "--page");
        if (name.empty()) {
            std::cerr << "Se requiere --page\n";
            return 1;
        }
        auto* n = app.getNoteManager().getNoteById(name);
        if (!n) {
            n = app.getNoteManager().createNote(name);
            if (n) {
                n->content = "# " + name + "\n\n";
                n->saveToFile();
            }
        }
        app.rebuildKnowledgeIndices();
        std::cout << "Pagina lista: " << name << "\n";
        return 0;
    }
    return 1;
}

int CliRunner::handleShowCommand(App& app, const std::vector<std::string>& args) {
    std::string page = getFlag(args, "--page");
    if (page.empty() && args.size() > 1) page = args[1];
    auto roots = app.getOutlinerEngine().getRootBlocksForPage(page);
    if (roots.empty()) {
        const auto* note = app.getNoteManager().getNoteById(page);
        if (!note) {
            std::cerr << "Pagina no encontrada: " << page << "\n";
            return 1;
        }
        std::cout << note->content << "\n";
        return 0;
    }
    for (const auto& uuid : roots) {
        const auto* blk = app.getOutlinerEngine().getBlock(uuid);
        if (blk) {
            std::cout << blk->uuid.substr(0, 8) << "  "
                      << app.getOutlinerEngine().resolveBlockRefs(blk->content) << "\n";
        }
    }
    return 0;
}

int CliRunner::handleSearchCommand(App& app, const std::vector<std::string>& args) {
    std::string q = getFlag(args, "--content");
    if (q.empty() && args.size() > 1) q = args[1];
    auto blocks = app.getOutlinerEngine().searchBlocks(q);
    for (const auto* b : blocks) {
        std::cout << b->pageId << " | " << b->uuid.substr(0, 8) << " | " << b->content << "\n";
    }
    std::cout << "Count: " << blocks.size() << "\n";
    return 0;
}

int CliRunner::handleQueryCommand(App& app, const std::vector<std::string>& args) {
    if (args.size() > 1 && args[1] == "list") {
        for (const auto& nq : app.getDatalogEngine().getNamedQueries()) {
            std::cout << nq.name << "\t" << nq.description << "\n";
        }
        return 0;
    }
    std::string named = getFlag(args, "--name");
    std::string err;
    std::vector<DatalogRow> rows;
    if (!named.empty()) {
        rows = app.getDatalogEngine().runNamedQuery(named, getFlag(args, "--arg"), err);
    } else {
        std::string raw = getFlag(args, "--query");
        if (raw.empty() && args.size() > 1) raw = args[1];
        rows = app.getDatalogEngine().executeQuery(raw, err);
    }
    if (!err.empty()) {
        std::cerr << "Error Datalog: " << err << "\n";
        return 1;
    }
    for (const auto& r : rows) {
        bool first = true;
        for (const auto& [k, v] : r.bindings) {
            if (!first) std::cout << " | ";
            first = false;
            std::cout << k << "=" << v;
        }
        std::cout << "\n";
    }
    std::cout << "Count: " << rows.size() << "\n";
    return 0;
}

int CliRunner::handleDbCommand(App& app, const std::vector<std::string>& args) {
    std::string sub = args.size() > 1 ? args[1] : "status";
    auto& db = app.getDatabaseManager();
    if (sub == "status") {
        std::string details;
        bool ok = db.testConnection(details);
        auto st = db.getStatus();
        std::cout << "Motor: " << st.engineName << "\n";
        std::cout << "Modo: " << st.modeName << "\n";
        std::cout << "Conectado: " << (ok ? "si" : "no") << "\n";
        std::cout << "Detalle: " << details << "\n";
        return ok ? 0 : 1;
    }
    if (sub == "config") {
        DbConfig cfg = db.config();
        std::string eng = getFlag(args, "--engine");
        if (!eng.empty()) cfg.engine = DatabaseManager::stringToEngine(eng);
        std::string mode = getFlag(args, "--mode");
        if (!mode.empty()) cfg.deployMode = (mode == "server") ? DbDeployMode::Server : DbDeployMode::Local;
        std::string host = getFlag(args, "--host");
        if (!host.empty()) cfg.host = host;
        std::string port = getFlag(args, "--port");
        if (!port.empty()) { try { cfg.port = std::stoi(port); } catch (...) {} }
        std::string name = getFlag(args, "--database");
        if (!name.empty()) cfg.database = name;
        std::string user = getFlag(args, "--user");
        if (!user.empty()) cfg.username = user;
        std::string pass = getFlag(args, "--password");
        if (!pass.empty()) cfg.password = pass;
        db.configure(cfg);
        std::cout << "Configuracion de base de datos guardada (" << DatabaseManager::engineToString(cfg.engine) << ").\n";
        return 0;
    }
    if (sub == "init") {
        std::string eng = getFlag(args, "--engine");
        if (!eng.empty()) {
            DbConfig cfg = db.config();
            cfg.engine = DatabaseManager::stringToEngine(eng);
            db.configure(cfg);
        }
        std::string err;
        if (!db.initializeSchema(err)) {
            std::cerr << "Error inicializando tablas: " << err << "\n";
            return 1;
        }
        std::cout << "Tablas inicializadas correctamente.\n";
        return 0;
    }
    if (sub == "sync") {
        std::string summary;
        db.syncNotes(app.getNoteManager(), summary);
        std::cout << summary << "\n";
        return 0;
    }
    if (sub == "query") {
        std::string sql = args.size() > 2 ? args[2] : "";
        auto res = db.executeSql(sql);
        if (!res.ok) {
            std::cerr << "Error SQL: " << res.error << "\n";
            return 1;
        }
        for (const auto& row : res.rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) std::cout << "\t";
                std::cout << row[i];
            }
            std::cout << "\n";
        }
        return 0;
    }
    return 1;
}

int CliRunner::handleSyncCommand(App& app, const std::vector<std::string>& args) {
    std::string sub = args.size() > 1 ? args[1] : "status";
    auto& sync = app.getSyncEngine();
    if (sub == "status" || sub == "checksum") {
        auto diag = sync.diagnostics(app.getNoteManager());
        std::cout << "local_t=" << diag.localT << " remote_t=" << diag.remoteT
                  << " e2ee=" << (diag.e2eeEnabled ? "on" : "off")
                  << " checksum=" << diag.localChecksum << "\n";
        return 0;
    }
    if (sub == "export") {
        std::string outPath = getFlag(args, "--file", "data/snapshot.np");
        std::string pass = getFlag(args, "--e2ee-password");
        std::string err;
        if (!sync.exportSnapshot(app.getNoteManager(), outPath, !pass.empty(), pass, err)) {
            std::cerr << "Error exportando snapshot: " << err << "\n";
            return 1;
        }
        std::cout << "Snapshot guardado en " << outPath << "\n";
        return 0;
    }
    if (sub == "import") {
        std::string inPath = getFlag(args, "--file");
        std::string pass = getFlag(args, "--e2ee-password");
        std::string err;
        if (!sync.importSnapshot(app.getNoteManager(), inPath, pass, err)) {
            std::cerr << "Error importando snapshot: " << err << "\n";
            return 1;
        }
        app.rebuildKnowledgeIndices();
        std::cout << "Snapshot importado correctamente.\n";
        return 0;
    }
    return 1;
}

int CliRunner::handleBackupCommand(App& app, const std::vector<std::string>& args) {
    std::string sub = args.size() > 1 ? args[1] : "list";
    auto& bm = app.getBackupManager();
    if (sub == "list") {
        auto list = bm.listBackups();
        for (const auto& b : list) {
            std::cout << b.name << "\t" << b.source << "\t" << b.fileCount << " archivos\t" << b.checksum.substr(0, 12) << "\n";
        }
        std::cout << "Count: " << list.size() << "\n";
        return 0;
    }
    if (sub == "create") {
        auto info = bm.createBackup(app.getNoteManager(), getFlag(args, "--name"), "cli");
        std::cout << "Respaldo creado: " << info.name << " (" << info.fileCount << " archivos)\n";
        return 0;
    }
    if (sub == "restore") {
        std::string err;
        if (!bm.restoreBackup(getFlag(args, "--src"), app.getNoteManager(), err)) {
            std::cerr << "Error: " << err << "\n";
            return 1;
        }
        app.rebuildKnowledgeIndices();
        std::cout << "Respaldo restaurado correctamente.\n";
        return 0;
    }
    if (sub == "remove") {
        std::string err;
        if (!bm.removeBackup(getFlag(args, "--src"), err)) {
            std::cerr << "Error: " << err << "\n";
            return 1;
        }
        std::cout << "Respaldo eliminado.\n";
        return 0;
    }
    return 1;
}

int CliRunner::handleRunCommand(App& app, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Uso: NodePanda run <prompt> [--model id] [--room sala] [--wait]\n";
        return 1;
    }
    std::string prompt = args[1];
    std::string model = getFlag(args, "--model", "auto");
    std::string reviewer = getFlag(args, "--reviewer", "auto");
    std::string room = getFlag(args, "--room", "general");
    bool wait = hasFlag(args, "--wait");
    std::string owner = app.getAuthManager().currentUser() ? app.getAuthManager().currentUser()->username : "cli";

    auto run = app.getAgentEngine().createAndStartRun(owner, prompt, model, reviewer, room);
    std::cout << "Mision: " << run.id << " | Sala: " << run.room << " | Modelo: " << run.model << "\n";
    if (!wait) return 0;

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        const auto* cur = app.getAgentEngine().findRun(run.id);
        if (!cur) break;
        if (cur->status != "queued" && cur->status != "running") {
            std::cout << "Estado: " << cur->status << " | Costo: " << AiCatalog::formatUsdFromNano(cur->costNano) << " USD\n";
            if (!cur->result.empty()) std::cout << cur->result << "\n";
            if (!cur->error.empty()) {
                std::cerr << "Error: " << cur->error << "\n";
                return 1;
            }
            return 0;
        }
    }
    return 0;
}

int CliRunner::handleLabCommand(App& app, const std::string& subcmd, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "Se requiere la ruta del archivo.\n";
        return 1;
    }
    auto& lab = app.getDataLab();
    if (subcmd == "profile") {
        auto prof = lab.profileCsv(args[1]);
        if (!prof.ok) {
            std::cerr << "Error: " << prof.error << "\n";
            return 1;
        }
        std::cout << "Archivo: " << prof.sourcePath << " | Filas: " << prof.rowCount
                  << " | Columnas: " << prof.colCount << " | SHA256: " << prof.sha256.substr(0, 16) << "\n";
        for (const auto& col : prof.columns) {
            std::cout << "  - " << col.name << " (" << col.inferredType << ") no-nulos=" << col.nonEmpty << "\n";
        }
        return 0;
    }
    if (subcmd == "analyze") {
        AnalysisPlan plan;
        plan.aggregation = getFlag(args, "--agg", "count");
        plan.metric = getFlag(args, "--metric");
        plan.groupBy = getFlag(args, "--group-by");
        auto art = lab.analyzeCsv(args[1], plan);
        if (!art.ok) {
            std::cerr << "Error: " << art.error << "\n";
            return 1;
        }
        std::cout << "Artefacto: " << art.id << " | Filas: " << art.processedRows << " | Total: " << art.totalValue << "\n";
        for (const auto& pt : art.points) {
            std::cout << "  " << pt.label << ": " << pt.value << "\n";
        }
        std::string chartOut = getFlag(args, "--chart");
        if (!chartOut.empty()) {
            std::ofstream f(chartOut, std::ios::trunc);
            f << lab.renderChartSvg(art, "bar");
            std::cout << "Grafico SVG guardado en " << chartOut << "\n";
        }
        return 0;
    }
    return 1;
}

int CliRunner::handleVectorCommand(App& app, const std::vector<std::string>& args) {
    std::string kStr = getFlag(args, "--k", "");
    int topK = 10;
    if (!kStr.empty()) {
        try { topK = std::stoi(kStr); } catch (...) {}
    }
    topK = std::max(1, std::min(100, topK));

    std::string query;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--k") {
            ++i;
            continue;
        }
        if (args[i].rfind("--k=", 0) == 0 || args[i] == "--no-auth-prompt") {
            continue;
        }
        if (args[i].rfind("--", 0) == 0) continue;
        if (!query.empty()) query.push_back(' ');
        query += args[i];
    }

    if (query.empty()) {
        std::cerr << "Uso: NodePanda vector <consulta> [--k 10]\n";
        return 1;
    }

    auto hits = app.getVectorEngine().searchHybrid(query, topK);
    std::cout << "Busqueda Vectorial Hibrida (" << app.getVectorEngine().dimension()
              << "D, docs=" << app.getVectorEngine().documentCount() << "): \"" << query << "\"\n";
    std::cout << std::fixed << std::setprecision(4);
    for (size_t i = 0; i < hits.size(); ++i) {
        const auto& h = hits[i];
        std::cout << "  " << (i + 1) << ". [" << h.kind << "] "
                  << (h.title.empty() ? h.pageId : h.title)
                  << " | hybrid=" << h.hybridScore
                  << " vector=" << h.vectorScore
                  << " keyword=" << h.keywordScore << "\n";
        if (!h.snippet.empty()) {
            std::cout << "     " << h.snippet << "\n";
        }
    }
    std::cout << "Count: " << hits.size() << "\n";
    return 0;
}

int CliRunner::handleRtcCommand(App& app, const std::vector<std::string>& args) {
    std::string sub = args.size() > 1 ? args[1] : "status";
    if (sub != "status") {
        std::cerr << "Uso: NodePanda rtc status\n";
        return 1;
    }
    auto& rtc = app.getWebSocketRtcServer();
    auto st = rtc.stats();
    std::cout << "WebSocket RTC (RFC 6455)\n";
    std::cout << "  Estado: " << (st.serverRunning ? "activo" : "detenido") << "\n";
    std::cout << "  Direccion: ws:" << "/" << "/" << rtc.getBindHost() << ":" << st.port << "\n";
    std::cout << "  Puerto RFC 6455: " << st.port << "\n";
    std::cout << "  Peers conectados: " << st.connectedPeers << "\n";
    std::cout << "  Mensajes enviados: " << st.messagesSent << "\n";
    std::cout << "  Mensajes recibidos: " << st.messagesReceived << "\n";
    std::cout << "  Revision actual: " << st.currentRev << "\n";
    return 0;
}

int CliRunner::handlePdfCommand(App& app, const std::vector<std::string>& args) {
    std::string sub = args.size() > 1 ? args[1] : "list";
    auto& pdf = app.getPdfEngine();

    if (sub == "demo") {
        std::filesystem::path baseDir = pdf.dataDir().empty() ? std::filesystem::path("data") : pdf.dataDir();
        std::filesystem::path demoPath = baseDir / "assets" / "demo_nodepanda.pdf";
        std::vector<std::string> paragraphs = {
            "Arquitectura de conocimiento en grafos locales y anotacion visual de documentos PDF.",
            "Node Panda v2.1 integra notas Markdown, outliner por bloques, busqueda vectorial 384D y anotaciones PDF persistentes.",
            "Cada documento abierto calcula su huella SHA-256 para guardar resaltados y comentarios en formato TSV."
        };
        if (!pdf.createSamplePdf(demoPath, "Guia de Anotacion Visual PDF", paragraphs)) {
            std::cerr << "Error generando PDF de demostracion.\n";
            return 1;
        }
        std::string err;
        if (!pdf.openPdf(demoPath, err)) {
            std::cerr << "Error abriendo PDF generado: " << err << "\n";
            return 1;
        }
        const auto* doc = pdf.activeDocument();
        std::cout << "PDF demo generado: " << demoPath.string() << "\n";
        if (doc) {
            std::cout << "  Titulo: " << doc->title << "\n";
            std::cout << "  Paginas: " << doc->pages.size() << "\n";
            std::cout << "  Resaltados: " << doc->highlights.size() << "\n";
            std::cout << "  SHA-256: " << doc->sha256.substr(0, 16) << "\n";
        }
        return 0;
    }

    if (sub == "open") {
        if (args.size() < 3) {
            std::cerr << "Uso: NodePanda pdf open <archivo.pdf>\n";
            return 1;
        }
        std::string err;
        if (!pdf.openPdf(std::filesystem::path(args[2]), err)) {
            std::cerr << "Error abriendo PDF: " << err << "\n";
            return 1;
        }
        const auto* doc = pdf.activeDocument();
        if (doc) {
            std::cout << "Documento PDF: " << doc->title << "\n";
            std::cout << "  Archivo: " << doc->filePath.string() << "\n";
            std::cout << "  SHA-256: " << doc->sha256.substr(0, 16) << "\n";
            std::cout << "  Paginas: " << doc->pages.size() << "\n";
            std::cout << "  Resaltados: " << doc->highlights.size() << "\n";
            for (const auto& page : doc->pages) {
                std::cout << "  - Pagina " << page.pageNumber << ": " << page.textLines.size() << " lineas\n";
            }
            for (const auto& hl : doc->highlights) {
                std::cout << "  * [Pag " << (hl.pageIndex + 1) << "] " << hl.quotedText << "\n";
            }
        }
        return 0;
    }

    if (sub == "list") {
        const auto& docs = pdf.documents();
        for (const auto& d : docs) {
            std::cout << d.id << "\t" << d.title << "\t" << d.pages.size()
                      << " pags\t" << d.highlights.size() << " resaltados\n";
        }
        std::cout << "Count: " << docs.size() << "\n";
        return 0;
    }

    std::cerr << "Uso: NodePanda pdf <demo|open archivo.pdf|list>\n";
    return 1;
}

int CliRunner::handleTableCommand(App& app, const std::vector<std::string>& args) {
    std::string sub = args.size() > 1 ? args[1] : "list";
    if (sub != "list") {
        std::cerr << "Uso: NodePanda table list\n";
        return 1;
    }
    const auto& views = app.getTableViewEngine().views();
    std::cout << "ID\tNOMBRE\tCOLUMNAS\tFILAS\n";
    for (const auto& v : views) {
        auto rows = app.getTableViewEngine().queryRows(app.getNoteManager(), v);
        std::cout << v.id << "\t" << v.name << "\t" << v.columns.size() << "\t" << rows.size() << "\n";
    }
    std::cout << "Count: " << views.size() << "\n";
    return 0;
}

int CliRunner::handleMobileCommand(App& app, const std::vector<std::string>& args, int defaultPort) {
    (void)app;
    int port = defaultPort;
    std::string pStr = getFlag(args, "--port", "");
    if (!pStr.empty()) {
        try { port = std::stoi(pStr); } catch (...) {}
    }
    std::cout << "Companion Movil PWA URL: " << MobileBridge::getLocalLanHint(port) << "\n";
    std::cout << "Inicia el servidor con: NodePanda --server --port " << port << "\n";
    return 0;
}

int CliRunner::handleDoctorCommand(App& app) {
    auto rtcStats = app.getWebSocketRtcServer().stats();
    std::cout << "Diagnostico Node Panda v2.1.0\n";
    std::cout << "  Notas cargadas: " << app.getNoteManager().getAllNotes().size() << "\n";
    std::cout << "  Bloques indexados: " << app.getOutlinerEngine().getAllBlocks().size() << "\n";
    std::cout << "  Tareas activas: " << app.getTaskEngine().getTasks().size() << "\n";
    std::cout << "  Datoms en memoria: " << app.getDatalogEngine().datomCount() << "\n";
    std::cout << "  Skills cargadas: " << app.getSkillManager().getSkills().size() << "\n";
    std::cout << "  Vectores 384D: " << app.getVectorEngine().documentCount()
              << " (" << app.getVectorEngine().dimension() << "D)\n";
    std::cout << "  Vistas de Tabla: " << app.getTableViewEngine().views().size() << "\n";
    std::cout << "  Documentos PDF: " << app.getPdfEngine().documents().size() << "\n";
    std::cout << "  WebSocket RTC: " << (rtcStats.serverRunning ? "activo" : "detenido")
              << " (puerto " << rtcStats.port << ", peers=" << rtcStats.connectedPeers << ")\n";
    std::string dbDetails;
    bool dbOk = app.getDatabaseManager().testConnection(dbDetails);
    std::cout << "  Base de datos: " << (dbOk ? "OK" : "ADVERTENCIA") << " (" << dbDetails << ")\n";
    return 0;
}

int CliRunner::handleGraphCommand(App& app, const std::vector<std::string>& args) {
    std::string sub = args.size() > 1 ? args[1] : "info";
    if (sub == "info") {
        std::cout << "Directorio: " << app.getNoteManager().getNotesDirectory().string() << "\n";
        std::cout << "Notas: " << app.getNoteManager().getAllNotes().size() << "\n";
        std::cout << "Nodos: " << app.getGraph().getNodes().size() << "\n";
        std::cout << "Aristas: " << app.getGraph().getEdges().size() << "\n";
        return 0;
    }
    if (sub == "backup") {
        std::vector<std::string> subArgs(args.begin() + 1, args.end());
        return handleBackupCommand(app, subArgs);
    }
    return 0;
}

CliLaunchResult CliRunner::run(int argc, char** argv, App& app) {
    CliLaunchResult result;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    std::string portStr = getFlag(args, "--port", "");
    if (portStr.empty()) {
        const char* envPort = std::getenv("NODEPANDA_PORT");
        if (envPort) portStr = envPort;
    }
    if (!portStr.empty()) {
        try { result.serverPort = std::stoi(portStr); } catch (...) {}
    }

    if (hasFlag(args, "--help") || hasFlag(args, "-h")) {
        printHelp();
        result.mode = LaunchMode::CliFinished;
        return result;
    }
    if (hasFlag(args, "--version") || hasFlag(args, "-v")) {
        std::cout << "Node Panda 2.1.0\n";
        result.mode = LaunchMode::CliFinished;
        return result;
    }

    bool skipPrompt = hasFlag(args, "--no-auth-prompt");

    if (!args.empty() && args[0] == "login") {
        UserAccount u;
        std::string token;
        bool ok = app.getAuthManager().interactiveTerminalLogin(u, token);
        result.mode = LaunchMode::CliFinished;
        result.exitCode = ok ? 0 : 1;
        return result;
    }

    if (!args.empty() && args[0] == "logout") {
        app.getAuthManager().clearCliToken();
        std::cout << "Sesion cerrada.\n";
        result.mode = LaunchMode::CliFinished;
        return result;
    }

    if (!args.empty() && args[0] == "register") {
        std::string user, email;
        std::cout << "Usuario: " << std::flush;
        std::getline(std::cin, user);
        std::cout << "Correo: " << std::flush;
        std::getline(std::cin, email);
        std::string pass = AuthManager::readPasswordMasked("Contrasena: ");
        std::string err;
        if (!app.getAuthManager().registerUser(user, email, pass, err)) {
            std::cerr << "Error: " << err << "\n";
            result.exitCode = 1;
        } else {
            std::cout << "Usuario registrado correctamente.\n";
        }
        result.mode = LaunchMode::CliFinished;
        return result;
    }

    if (!ensureAuthenticated(app, skipPrompt)) {
        std::cerr << "Autenticacion requerida para ejecutar Node Panda.\n";
        result.mode = LaunchMode::CliFinished;
        result.exitCode = 1;
        return result;
    }

    if (app.getAuthManager().currentUser()) {
        result.authenticatedUser = app.getAuthManager().currentUser()->username;
    }

    if (args.empty()) {
        result.exitCode = runInteractiveLauncher(app, result);
        return result;
    }

    if (hasFlag(args, "--gui") || args[0] == "gui") {
        result.mode = LaunchMode::GuiApp;
        return result;
    }

    if (hasFlag(args, "--server") || args[0] == "server") {
        result.mode = LaunchMode::HeadlessServer;
        return result;
    }

    const std::string& cmd = args[0];
    result.mode = LaunchMode::CliFinished;

    if (cmd == "chat") {
        result.exitCode = runChatRepl(
            app,
            getFlag(args, "--name", "atlas"),
            getFlag(args, "--room", "general"),
            getFlag(args, "--model", "auto"));
    } else if (cmd == "run") {
        result.exitCode = handleRunCommand(app, args);
    } else if (cmd == "runs") {
        for (const auto& r : app.getAgentEngine().listRuns()) {
            std::cout << r.id.substr(0, 8) << "\t" << r.status << "\t" << r.model << "\t"
                      << AiCatalog::formatUsdFromNano(r.costNano) << "\t" << r.title << "\n";
        }
    } else if (cmd == "models") {
        for (const auto& p : app.getAiCatalog().providers()) {
            std::cout << p.asciiBadge << " " << p.name << " (" << (app.getAiCatalog().hasApiKey(p.id) ? "API OK" : "sin clave") << ")\n";
            for (const auto& m : app.getAiCatalog().models()) {
                if (m.provider == p.id) {
                    std::cout << "    " << m.id << "  $" << m.inputPricePer1M << " / $" << m.outputPricePer1M << " por 1M\n";
                }
            }
        }
    } else if (cmd == "skills") {
        for (const auto& s : app.getSkillManager().getSkills()) {
            std::cout << (s.enabled ? "[ON]  " : "[OFF] ") << s.name << " - " << s.description << "\n";
        }
    } else if (cmd == "recall") {
        std::string q = args.size() > 1 ? args[1] : "";
        auto res = app.getContextBrain().retrieve(app.getNoteManager(), q, 1800);
        std::cout << res.formattedXml << "\n";
    } else if (cmd == "profile" || cmd == "analyze") {
        result.exitCode = handleLabCommand(app, cmd, args);
    } else if (cmd == "export") {
        std::string out = getFlag(args, "--out", "nodepanda-metrics.csv");
        bool ok = app.getUsageLedger().exportCsv(out);
        std::cout << (ok ? ("Exportado: " + out) : "Error al exportar CSV") << "\n";
        result.exitCode = ok ? 0 : 1;
    } else if (cmd == "status") {
        result.exitCode = handleDoctorCommand(app);
    } else if (cmd == "doctor") {
        result.exitCode = handleDoctorCommand(app);
    } else if (cmd == "graph") {
        result.exitCode = handleGraphCommand(app, args);
    } else if (cmd == "list") {
        result.exitCode = handleListCommand(app, args);
    } else if (cmd == "upsert") {
        result.exitCode = handleUpsertCommand(app, args);
    } else if (cmd == "show") {
        result.exitCode = handleShowCommand(app, args);
    } else if (cmd == "search") {
        result.exitCode = handleSearchCommand(app, args);
    } else if (cmd == "query") {
        result.exitCode = handleQueryCommand(app, args);
    } else if (cmd == "db") {
        result.exitCode = handleDbCommand(app, args);
    } else if (cmd == "sync") {
        result.exitCode = handleSyncCommand(app, args);
    } else if (cmd == "backup") {
        result.exitCode = handleBackupCommand(app, args);
    } else if (cmd == "vector") {
        result.exitCode = handleVectorCommand(app, args);
    } else if (cmd == "rtc") {
        result.exitCode = handleRtcCommand(app, args);
    } else if (cmd == "pdf") {
        result.exitCode = handlePdfCommand(app, args);
    } else if (cmd == "table") {
        result.exitCode = handleTableCommand(app, args);
    } else if (cmd == "mobile") {
        result.exitCode = handleMobileCommand(app, args, result.serverPort);
    } else {
        std::cerr << "Comando desconocido: " << cmd << "\n";
        printHelp();
        result.exitCode = 1;
    }

    return result;
}

}
