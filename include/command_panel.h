#pragma once
// ============================================================================
// CommandPanel — Centro de Mando (Dashboard)
//
// Panel de control con estadísticas del workspace, indicadores de salud,
// acciones rápidas, y actividad reciente. Proporciona una vista panorámica
// del estado de todas las notas y conexiones.
// ============================================================================

#include <string>
#include <vector>
#include <chrono>

namespace nodepanda {

class App;

class CommandPanel {
public:
    void render(App& app);

private:
    // Cache de estadísticas (recalcular solo cuando cambie algo)
    struct Stats {
        int totalNotes      = 0;
        int totalLinks      = 0;
        int orphanNotes     = 0;    // sin enlaces entrantes ni salientes
        int brokenLinks     = 0;    // enlaces a notas que no existen
        int typeCount       = 0;    // tipos únicos de notas
        float avgLinksPerNote = 0.0f;
        float graphDensity   = 0.0f; // aristas / aristas posibles
        std::vector<std::pair<std::string, int>> topConnected; // top 5 más conectadas
        std::vector<std::pair<std::string, int>> typeDistribution; // tipo → conteo
        std::vector<std::string> orphanList;
        std::vector<std::string> brokenLinkList;
        std::vector<std::pair<std::string, std::string>> recentNotes; // id, title
    };

    Stats m_stats;
    int   m_cachedNoteCount = -1;
    bool  m_showOrphans     = false;
    bool  m_showBroken      = false;

    void recalcStats(App& app);
};

} // namespace nodepanda
