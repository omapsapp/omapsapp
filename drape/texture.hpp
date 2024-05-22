#pragma once

#include "drape/graphics_context.hpp"
#include "drape/hw_texture.hpp"
#include "drape/pointers.hpp"
#include "drape/texture_types.hpp"

#include "geometry/rect2d.hpp"

#include "base/macros.hpp"

#include <cstdint>

namespace dp
{
class Texture
{
public:
  enum class ResourceType : uint8_t
  {
    Symbol,
    Glyph,
    StipplePen,
    Color,
    Static
  };

  class Key
  {
  public:
    virtual ~Key() = default;
    virtual ResourceType GetType() const = 0;
  };

  class ResourceInfo
  {
  public:
    explicit ResourceInfo(m2::RectF const & texRect);
    virtual ~ResourceInfo() = default;
    virtual ResourceType GetType() const = 0;
    m2::RectF const & GetTexRect() const;

  private:
    m2::RectF m_texRect;
  };

  Texture() = default;
  virtual ~Texture() = default;

  virtual ref_ptr<ResourceInfo> FindResource(Key const & key, bool & newResource) = 0;
  virtual void UpdateState(ref_ptr<dp::GraphicsContext> /* context */) {}
  virtual bool HasEnoughSpace(uint32_t /* newKeysCount */) const { return true; }
  using Params = HWTexture::Params;

  virtual TextureFormat GetFormat() const;
  virtual uint32_t GetWidth() const;
  virtual uint32_t GetHeight() const;
  virtual float GetS(uint32_t x) const;
  virtual float GetT(uint32_t y) const;
  virtual uint32_t GetID() const;

  virtual void Bind(ref_ptr<dp::GraphicsContext> context) const;

  // Texture must be bound before calling this method.
  virtual void SetFilter(TextureFilter filter);

  virtual void Create(ref_ptr<dp::GraphicsContext> context, Params const & params);
  virtual void Create(ref_ptr<dp::GraphicsContext> context, Params const & params,
                      ref_ptr<void> data);
  void UploadData(ref_ptr<dp::GraphicsContext> context, uint32_t x, uint32_t y,
                  uint32_t width, uint32_t height, ref_ptr<void> data);

  ref_ptr<HWTexture> GetHardwareTexture() const;

  static bool IsPowerOfTwo(uint32_t width, uint32_t height);

protected:
  void Destroy();
  bool AllocateTexture(ref_ptr<dp::GraphicsContext> context,
                       ref_ptr<HWTextureAllocator> allocator);

  drape_ptr<HWTexture> m_hwTexture;

  DISALLOW_COPY_AND_MOVE(Texture);
};
}  // namespace dp
