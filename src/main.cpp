#include "app.h"

int main() {
    nodepanda::App app;

    if (!app.init("Node Panda - Asistente de Nodos", 1400, 900)) {
        return -1;
    }

    app.run();
    app.shutdown();

    return 0;
}
