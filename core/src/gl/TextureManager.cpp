#include "dc/gl/TextureManager.hpp"

namespace dc {

TextureManager::~TextureManager() {
  for (auto& [id, entry] : textures_) {
    if (entry.glTexture) glDeleteTextures(1, &entry.glTexture);
  }
}

TextureId TextureManager::load(const std::uint8_t* rgbaData, int width, int height) {
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, rgbaData);
  glBindTexture(GL_TEXTURE_2D, 0);

  TextureId id = nextId_++;
  textures_[id] = {tex, width, height};
  return id;
}

void TextureManager::remove(TextureId id) {
  auto it = textures_.find(id);
  if (it != textures_.end()) {
    if (it->second.glTexture) glDeleteTextures(1, &it->second.glTexture);
    textures_.erase(it);
  }
}

void TextureManager::bind(TextureId id, int unit) {
  auto it = textures_.find(id);
  if (it != textures_.end()) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, it->second.glTexture);
  }
}

GLuint TextureManager::getGlTexture(TextureId id) const {
  auto it = textures_.find(id);
  return (it != textures_.end()) ? it->second.glTexture : 0;
}

} // namespace dc
