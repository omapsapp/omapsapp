#pragma once

#include "drape/texture.hpp"

#include <string>

namespace dp
{
class StaticTexture : public Texture
{
  using Base = Texture;
public:
  class StaticKey : public Key
  {
  public:
    ResourceType GetType() const override { return ResourceType::Static; }
  };

  static std::string const kDefaultResource;

  StaticTexture();

  /// @todo All xxxName can be std::string_view after Platform::GetReader (StyleReader) refactoring.
  StaticTexture(ref_ptr<dp::GraphicsContext> context, std::string const & textureName,
                std::string const & skinPathName, dp::TextureFormat format,
                ref_ptr<HWTextureAllocator> allocator, bool allowOptional = false);

  ref_ptr<ResourceInfo> FindResource(Key const & key, bool & newResource) override;
  void Create(ref_ptr<dp::GraphicsContext> context, Params const & params) override;
  void Create(ref_ptr<dp::GraphicsContext> context, Params const & params,
              ref_ptr<void> data) override;

  bool IsLoadingCorrect() const { return m_isLoadingCorrect; }

private:
  void Fail(ref_ptr<dp::GraphicsContext> context);

  drape_ptr<Texture::ResourceInfo> m_info;

  bool m_isLoadingCorrect = false;
};
}  // namespace dp
