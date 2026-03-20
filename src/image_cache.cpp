
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "image_cache.h"

#ifdef _WIN32
#include <windows.h>
#endif
#include <GLFW/glfw3.h>

#include <algorithm>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace nodepanda {

ImageCache::~ImageCache() {
    clear();
}

const CachedTexture* ImageCache::loadTexture(const std::string& filepath) {
    auto it = m_cache.find(filepath);
    if (it != m_cache.end()) {
        return &it->second;
    }

    int width, height, channels;
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 4); 
    if (!data) {
        return nullptr;
    }

    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);

    CachedTexture tex;
    tex.textureId = textureId;
    tex.width = width;
    tex.height = height;
    m_cache[filepath] = tex;

    return &m_cache[filepath];
}

void ImageCache::clear() {
    for (auto& [path, tex] : m_cache) {
        if (tex.textureId > 0) {
            glDeleteTextures(1, &tex.textureId);
        }
    }
    m_cache.clear();
}

bool ImageCache::isSupportedImage(const std::string& extension) {
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif";
}

} 
