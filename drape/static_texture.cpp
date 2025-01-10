#include "drape/static_texture.hpp"

#include "indexer/map_style_reader.hpp"

#include "platform/platform.hpp"

#include "coding/reader.hpp"

#include "3party/stb_image/stb_image.h"

#ifdef DEBUG
#include <glm/gtc/round.hpp>  // glm::isPowerOfTwo
#endif

#include <functional>
#include <limits>
#include <vector>

namespace dp
{
std::string const StaticTexture::kDefaultResource = "default";

namespace
{
using TLoadingCompletion = std::function<void(unsigned char *, uint32_t, uint32_t)>;
using TLoadingFailure = std::function<void(std::string const &)>;

bool LoadData(std::string const & textureName, std::string const & skinPathName,
              uint8_t bytesPerPixel, TLoadingCompletion const & completionHandler,
              TLoadingFailure const & failureHandler)
{
  ASSERT(completionHandler != nullptr, ());
  ASSERT(failureHandler != nullptr, ());

  std::vector<unsigned char> rawData;
  try
  {
    ReaderPtr<Reader> reader =
        !skinPathName.empty()
            ? skinPathName == StaticTexture::kDefaultResource
                ? GetStyleReader().GetDefaultResourceReader(textureName)
                : GetStyleReader().GetResourceReader(textureName, skinPathName)
            : GetPlatform().GetReader(textureName);

    CHECK_LESS(reader.Size(), static_cast<uint64_t>(std::numeric_limits<size_t>::max()), ());
    size_t const size = static_cast<size_t>(reader.Size());
    rawData.resize(size);
    reader.Read(0, &rawData[0], size);
  }
  catch (RootException & e)
  {
    failureHandler(e.what());
    return false;
  }

  int w, h, bpp;
  if (!stbi_info_from_memory(&rawData[0], static_cast<int>(rawData.size()), &w, &h, &bpp))
  {
    failureHandler(std::string("Failed to get image file info from ") + textureName);
    return false;
  }

  ASSERT(glm::isPowerOfTwo(w), (w));
  ASSERT(glm::isPowerOfTwo(h), (h));

  unsigned char * data =
      stbi_load_from_memory(&rawData[0], static_cast<int>(rawData.size()), &w, &h, &bpp, 0);

  if (bytesPerPixel != bpp)
  {
    std::vector<unsigned char> convertedData(static_cast<size_t>(w * h * bytesPerPixel));
    auto const pixelsCount = static_cast<uint32_t>(w * h);
    for (uint32_t i = 0; i < pixelsCount; ++i)
    {
      unsigned char const * p = data + i * bpp;
      for (uint8_t b = 0; b < bytesPerPixel; ++b)
        convertedData[i * bytesPerPixel + b] = (b < bpp) ? p[b] : 255;
    }
    stbi_image_free(data);
    completionHandler(convertedData.data(), static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    return true;
  }

  completionHandler(data, static_cast<uint32_t>(w), static_cast<uint32_t>(h));
  stbi_image_free(data);
  return true;
}

class StaticResourceInfo : public Texture::ResourceInfo
{
public:
  StaticResourceInfo() : Texture::ResourceInfo(m2::RectF(0.0f, 0.0f, 1.0f, 1.0f)) {}
  Texture::ResourceType GetType() const override { return Texture::ResourceType::Static; }
};
}  // namespace

StaticTexture::StaticTexture()
  : m_info(make_unique_dp<StaticResourceInfo>()) {}

StaticTexture::StaticTexture(ref_ptr<dp::GraphicsContext> context,
                             std::string const & textureName, std::string const & skinPathName,
                             dp::TextureFormat format, ref_ptr<HWTextureAllocator> allocator,
                             bool allowOptional /* = false */)
: m_info(make_unique_dp<StaticResourceInfo>())
{
  auto completionHandler = [&](unsigned char * data, uint32_t width, uint32_t height)
  {
    Texture::Params p;
    p.m_allocator = allocator;
    p.m_format = format;
    p.m_width = width;
    p.m_height = height;
    p.m_wrapSMode = TextureWrapping::Repeat;
    p.m_wrapTMode = TextureWrapping::Repeat;

    Create(context, p, make_ref(data));
  };

  auto failureHandler = [&](std::string const & reason)
  {
    if (!allowOptional)
    {
      LOG(LERROR, (reason));
      Fail(context);
    }
  };

  uint8_t const bytesPerPixel = GetBytesPerPixel(format);
  m_isLoadingCorrect = LoadData(textureName, skinPathName, bytesPerPixel, completionHandler, failureHandler);
}

ref_ptr<Texture::ResourceInfo> StaticTexture::FindResource(Texture::Key const & key,
                                                           bool & newResource)
{
  newResource = false;
  if (key.GetType() != Texture::ResourceType::Static)
    return nullptr;
  return make_ref(m_info);
}

void StaticTexture::Create(ref_ptr<dp::GraphicsContext> context, Params const & params)
{
  ASSERT(Base::IsPowerOfTwo(params.m_width, params.m_height), (params.m_width, params.m_height));

  Base::Create(context, params);
}

void StaticTexture::Create(ref_ptr<dp::GraphicsContext> context, Params const & params,
                           ref_ptr<void> data)
{
  ASSERT(Base::IsPowerOfTwo(params.m_width, params.m_height), (params.m_width, params.m_height));

  Base::Create(context, params, data);
}

void StaticTexture::Fail(ref_ptr<dp::GraphicsContext> context)
{
  int32_t alphaTexture = 0;
  Texture::Params p;
  p.m_allocator = GetDefaultAllocator(context);
  p.m_format = dp::TextureFormat::RGBA8;
  p.m_width = 1;
  p.m_height = 1;
  Create(context, p, make_ref(&alphaTexture));
}
}  // namespace dp
