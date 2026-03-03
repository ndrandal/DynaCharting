#pragma once
#include <cstdint>
#include <unordered_map>
#include <glad/gl.h>

namespace dc {

using TextureId = std::uint32_t;

class TextureManager {
public:
  ~TextureManager();

  TextureId load(const std::uint8_t* rgbaData, int width, int height);
  void remove(TextureId id);
  void bind(TextureId id, int unit);
  GLuint getGlTexture(TextureId id) const;
  std::size_t count() const { return textures_.size(); }

private:
  struct TextureEntry {
    GLuint glTexture{0};
    int width{0}, height{0};
  };

  TextureId nextId_{1};
  std::unordered_map<TextureId, TextureEntry> textures_;
};

} // namespace dc
