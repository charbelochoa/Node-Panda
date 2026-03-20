#pragma once
#include <string>
#include <map>

typedef unsigned int GLuint;

namespace nodepanda {

struct TextureInfo {
    GLuint textureId = 0;
    int width = 0;
    int height = 0;
};

class TextureManager {
public:
    ~TextureManager();

    
    bool LoadTextureFromFile(const char* filename,
                             GLuint* out_texture,
                             int* out_width,
                             int* out_height);

    const TextureInfo* getTexture(const std::string& filepath);

    void clear();

    static bool isSupportedImage(const std::string& extension);

private:
    
    std::map<std::string, TextureInfo> m_cache;
};

} 
