
#include "stb_image.h"
#include "texture_manager.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace nodepanda {

TextureManager::~TextureManager() {
    clear();
}


bool TextureManager::LoadTextureFromFile(const char* filename,
                                          GLuint* out_texture,
                                          int* out_width,
                                          int* out_height) {
    if (!filename || !out_texture || !out_width || !out_height) return false;

    std::string key(filename);

    auto it = m_cache.find(key);
    if (it != m_cache.end()) {
        *out_texture = it->second.textureId;
        *out_width   = it->second.width;
        *out_height  = it->second.height;
        return true;
    }

    int width = 0, height = 0, channels = 0;
    unsigned char* data = stbi_load(filename, &width, &height, &channels, 4);
    if (!data) {
        fprintf(stderr, "[TextureManager] Error cargando imagen: %s\n", filename);
        return false;
    }

    GLuint textureId = 0;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);

    TextureInfo info;
    info.textureId = textureId;
    info.width = width;
    info.height = height;
    m_cache[key] = info;

    *out_texture = textureId;
    *out_width   = width;
    *out_height  = height;

    return true;
}


const TextureInfo* TextureManager::getTexture(const std::string& filepath) {
    GLuint tex = 0;
    int w = 0, h = 0;
    if (LoadTextureFromFile(filepath.c_str(), &tex, &w, &h)) {
        return &m_cache[filepath];
    }
    return nullptr;
}


void TextureManager::clear() {
    for (auto& [path, info] : m_cache) {
        if (info.textureId > 0) {
            glDeleteTextures(1, &info.textureId);
        }
    }
    m_cache.clear();
}


bool TextureManager::isSupportedImage(const std::string& extension) {
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".png"  || ext == ".jpg" || ext == ".jpeg" ||
           ext == ".bmp"  || ext == ".gif" || ext == ".tga"  ||
           ext == ".webp";
}

} 
