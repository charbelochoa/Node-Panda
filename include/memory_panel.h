#pragma once
// ============================================================================
// MemoryPanel — Panel de Memoria Semántica
//
// Muestra tres secciones:
//   1. Notas Relacionadas: similares a la nota seleccionada (por TF-IDF coseno)
//   2. Búsqueda Semántica: búsqueda libre con ranking por relevancia
//   3. Sugerencias de Enlace: notas similares no enlazadas aún
// ============================================================================

#include <string>

namespace nodepanda {

class App;

class MemoryPanel {
public:
    void render(App& app);

private:
    // Búsqueda semántica
    static constexpr size_t SEARCH_SIZE = 512;
    char m_searchBuffer[SEARCH_SIZE] = {};
    bool m_searchTriggered = false;

    // Cache para evitar recalcular cada frame
    std::string m_cachedNoteId;
    int         m_cachedNoteCount = -1; // forzar primer cálculo
};

} // namespace nodepanda
