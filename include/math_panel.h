#pragma once

#include <string>

namespace nodepanda {

class App;

class MathPanel {
public:
    MathPanel();
    void render(App& app);

private:
    void setFormula(const std::string& latex);
    void appendToken(const std::string& token);
    void insertIntoSelectedNote(App& app);

    char m_formulaBuf[2048] = {};
    float m_fontSize = 24.0f;
    int m_insertMode = 0;
};

}
