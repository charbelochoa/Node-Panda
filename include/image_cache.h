#pragma once
#include <string>
#include <unordered_map>

typedef unsigned int GLuint;

namespace nodepanda {

struct CachedTexture {
    GLuint textureId = 0;
    int width = 0;
    int height = 0;
};

class ImageCache {
public:
    ~ImageCache();

    
    const CachedTexture* loadTexture(const std::string& filepath);

    void clear();

    static bool isSupportedImage(const std::string& extension);

private:
    std::unordered_map<std::string, CachedTexture> m_cache;
};

} 
