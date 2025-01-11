#pragma once

#include "drape/color.hpp"
#include "drape/glyph_manager.hpp"
#include "drape/pointers.hpp"
#include "drape/stipple_pen_resource.hpp"   // for PenPatternT
#include "drape/texture.hpp"

#include "base/string_utils.hpp"
#include "base/timer.hpp"

#include <atomic>
#include <cstdint>
#include <list>
#include <mutex>
#include <string>
#include <vector>

namespace dp
{
class HWTextureAllocator;

class TextureManager
{
public:
  class BaseRegion
  {
  public:
    BaseRegion();
    void SetResourceInfo(ref_ptr<Texture::ResourceInfo> info);
    void SetTexture(ref_ptr<Texture> texture);
    ref_ptr<Texture> GetTexture() const { return m_texture; }
    bool IsValid() const;

    m2::PointF GetPixelSize() const;
    float GetPixelHeight() const;
    m2::RectF const & GetTexRect() const;

  protected:
    ref_ptr<Texture::ResourceInfo> m_info;
    ref_ptr<Texture> m_texture;
  };

  class SymbolRegion : public BaseRegion
  {
  public:
    uint32_t GetTextureIndex() const { return m_textureIndex; }
    void SetTextureIndex(uint32_t index) { m_textureIndex = index; }
  private:
    uint32_t m_textureIndex = 0;
  };

  // TODO(AB): Remove?
  class GlyphRegion : public BaseRegion
  {
  };

  class StippleRegion : public BaseRegion
  {
  public:
    m2::PointU GetMaskPixelSize() const;
  };

  class ColorRegion : public BaseRegion
  {
  };

  struct Params
  {
    std::string m_resPostfix;
    double m_visualScale = 1.0;
    std::string m_colors;
    std::string m_patterns;
    GlyphManager::Params m_glyphMngParams;
    std::string m_arrowTexturePath; // maybe empty if no custom texture
    bool m_arrowTextureUseDefaultResourceFolder = false;
  };

  TextureManager();
  void Release();

  void Init(ref_ptr<dp::GraphicsContext> context, Params const & params);
  void OnSwitchMapStyle(ref_ptr<dp::GraphicsContext> context);
  void GetTexturesToCleanup(std::vector<drape_ptr<HWTexture>> & textures);

  bool GetSymbolRegionSafe(std::string const & symbolName, SymbolRegion & region);
  void GetSymbolRegion(std::string const & symbolName, SymbolRegion & region);

  void GetStippleRegion(PenPatternT const & pen, StippleRegion & region);
  void GetColorRegion(Color const & color, ColorRegion & region);

  using TMultilineText = buffer_vector<strings::UniString, 4>;
  using TGlyphsBuffer = buffer_vector<GlyphRegion, 128>;
  using TMultilineGlyphsBuffer = buffer_vector<TGlyphsBuffer, 4>;

  using TShapedTextLines = buffer_vector<text::TextMetrics, 4>;
  text::TextMetrics ShapeSingleTextLine(float fontPixelHeight, std::string_view utf8, TGlyphsBuffer * glyphRegions);
  TShapedTextLines ShapeMultilineText(float fontPixelHeight, std::string_view utf8, char const * delimiters,
                                      TMultilineGlyphsBuffer & multilineGlyphRegions);

  // This method must be called only on Frontend renderer's thread.
  bool AreGlyphsReady(TGlyphs const & glyphs) const;

  GlyphFontAndId GetSpaceGlyph() const;

  // On some devices OpenGL driver can't resolve situation when we upload to a texture on a thread
  // and use this texture to render on another thread. By this we move UpdateDynamicTextures call
  // into render thread. If you implement some kind of dynamic texture, you must synchronize UploadData
  // and index creation operations.
  // The result of the method shows if there are some changes in texture manager.
  bool UpdateDynamicTextures(ref_ptr<dp::GraphicsContext> context);

  ref_ptr<Texture> GetSymbolsTexture() const;
  ref_ptr<Texture> GetTrafficArrowTexture() const;
  ref_ptr<Texture> GetHatchingTexture() const;
  ref_ptr<Texture> GetArrowTexture() const;
  ref_ptr<Texture> GetSMAAAreaTexture() const;
  ref_ptr<Texture> GetSMAASearchTexture() const;

  void InvalidateArrowTexture(ref_ptr<dp::GraphicsContext> context,
                              std::string const & texturePath = {},
                              bool useDefaultResourceFolder = false);
  // Apply must be called on FrontendRenderer.
  void ApplyInvalidatedStaticTextures();

  ref_ptr<HWTextureAllocator> GetTextureAllocator() const;

private:
  struct GlyphGroup
  {
    std::set<GlyphFontAndId> m_glyphKeys;
    ref_ptr<Texture> m_texture {nullptr};
  };

  uint32_t m_maxTextureSize;
  uint32_t m_maxGlypsCount;

  ref_ptr<Texture> AllocateGlyphTexture();
  void GetRegionBase(ref_ptr<Texture> tex, BaseRegion & region, Texture::Key const & key);

  size_t FindHybridGlyphsGroup(std::vector<text::GlyphMetrics> const & glyphs);

  static uint32_t GetNumberOfGlyphsNotInGroup(std::vector<text::GlyphMetrics> const & glyphs, GlyphGroup const & group);

  template<typename TGlyphGroup>
  void FillResultBuffer(strings::UniString const & text, TGlyphGroup & group, TGlyphsBuffer & regions)
  {
    if (group.m_texture == nullptr)
      group.m_texture = AllocateGlyphTexture();

    GetGlyphsRegions(group.m_texture, text, regions);
  }

  template<typename TGlyphGroup>
  void FillResults(strings::UniString const & text, TGlyphsBuffer & buffers, TGlyphGroup & group)
  {
    FillResultBuffer<TGlyphGroup>(text, group, buffers);
  }

  template<typename TGlyphGroup>
  void FillResults(TMultilineText const & text, TMultilineGlyphsBuffer & buffers, TGlyphGroup & group)
  {
     buffers.resize(text.size());
     for (size_t i = 0; i < text.size(); ++i)
     {
       strings::UniString const & str = text[i];
       TGlyphsBuffer & buffer = buffers[i];
       FillResults<TGlyphGroup>(str, buffer, group);
     }
  }

  template<typename TText, typename TBuffer>
  void CalcGlyphRegions(TText const & text, TBuffer & buffers)
  {
    CHECK(m_isInitialized, ());
    size_t const hybridGroupIndex = FindHybridGlyphsGroup(text);
    ASSERT(hybridGroupIndex != GetInvalidGlyphGroup(), ());
    GlyphGroup & group = m_glyphGroups[hybridGroupIndex];
    FillResults<GlyphGroup>(text, buffers, group);
  }

  void UpdateGlyphTextures(ref_ptr<dp::GraphicsContext> context);

  static constexpr size_t GetInvalidGlyphGroup();

private:
  bool m_isInitialized = false;
  std::string m_resPostfix;
  std::vector<drape_ptr<Texture>> m_symbolTextures;
  drape_ptr<Texture> m_stipplePenTexture;
  drape_ptr<Texture> m_colorTexture;
  std::list<drape_ptr<Texture>> m_glyphTextures;
  mutable std::mutex m_glyphTexturesMutex;

  drape_ptr<Texture> m_trafficArrowTexture;
  drape_ptr<Texture> m_hatchingTexture;
  drape_ptr<Texture> m_arrowTexture;
  drape_ptr<Texture> m_smaaAreaTexture;
  drape_ptr<Texture> m_smaaSearchTexture;

  drape_ptr<Texture> m_newArrowTexture;

  drape_ptr<GlyphManager> m_glyphManager;
  drape_ptr<HWTextureAllocator> m_textureAllocator;

  buffer_vector<GlyphGroup, 4> m_glyphGroups;

  std::vector<drape_ptr<HWTexture>> m_texturesToCleanup;

  base::Timer m_uploadTimer;
  std::atomic_flag m_nothingToUpload {false};
  std::mutex m_calcGlyphsMutex;

  // TODO(AB): Make a more robust use of BreakIterator to split strings and get rid of this space glyph.
  GlyphFontAndId m_spaceGlyph;
};
}  // namespace dp
