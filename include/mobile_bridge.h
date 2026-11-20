#pragma once

#include <string>

namespace nodepanda {

class MobileBridge {
public:
    static std::string renderMobileAppHtml(int apiPort);
    static std::string renderManifestJson();
    static std::string renderServiceWorkerJs();
    static std::string getLocalLanHint(int apiPort);
};

}
