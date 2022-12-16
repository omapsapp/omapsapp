#include "drape/stipple_pen_resource.hpp"

#include "drape/texture.hpp"

#include "base/logging.hpp"
#include "base/shared_buffer_manager.hpp"

#include <cstring>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace dp
{
uint32_t const kMaxStipplePenLength = 512;  /// @todo Should be equal with kStippleTextureWidth?

StipplePenPacker::StipplePenPacker(m2::PointU const & canvasSize)
  : m_canvasSize(canvasSize)
  , m_currentRow(0)
{
  ASSERT_GREATER_OR_EQUAL(canvasSize.x, kMaxStipplePenLength, ());
}

m2::RectU StipplePenPacker::PackResource(m2::PointU const & size)
{
  ASSERT_LESS(m_currentRow, m_canvasSize.y, ());
  ASSERT_LESS_OR_EQUAL(size.x, m_canvasSize.x, ());
  uint32_t const yOffset = m_currentRow;
  m_currentRow += size.y;
  return m2::RectU(0, yOffset, size.x, m_currentRow);
}

m2::RectF StipplePenPacker::MapTextureCoords(m2::RectU const & pixelRect) const
{
  return { (pixelRect.minX() + 0.5f) / m_canvasSize.x,
           (pixelRect.minY() + 0.5f) / m_canvasSize.y,
           (pixelRect.maxX() - 0.5f) / m_canvasSize.x,
           (pixelRect.maxY() - 0.5f) / m_canvasSize.y };
}

StipplePenRasterizator::StipplePenRasterizator(StipplePenKey const & key)
  : m_key(key)
{
  if (IsTrianglePattern(m_key.m_pattern))
  {
    m_patternLength = 2 * m_key.m_pattern[0] + m_key.m_pattern[1];
    m_height = m_key.m_pattern[2] + m_key.m_pattern[3];
  }
  else
  {
    m_patternLength = std::accumulate(m_key.m_pattern.begin(), m_key.m_pattern.end(), 0);
    m_height = 1;
  }

  uint32_t const availableSize = kMaxStipplePenLength - 2; // the first and the last pixel reserved
  ASSERT(m_patternLength > 0 && m_patternLength < availableSize, (m_patternLength, availableSize));
  uint32_t const count = floor(availableSize / m_patternLength);
  m_pixelLength = count * m_patternLength;
}

void StipplePenRasterizator::Rasterize(uint8_t * buffer) const
{
  if (IsTrianglePattern(m_key.m_pattern))
    RasterizeTriangle(buffer);
  else
    RasterizeDash(buffer);
}

void StipplePenRasterizator::RasterizeDash(uint8_t * pixels) const
{
  // No problem here, but we use 2 entries patterns now (dash length, space length).
  ASSERT_EQUAL(m_key.m_pattern.size(), 2, ());

  uint32_t offset = 1;
  for (size_t i = 0; i < m_key.m_pattern.size(); ++i)
  {
    uint8_t const length = m_key.m_pattern[i];
    ASSERT(length > 0, ());

    memset(pixels + offset, (i & 0x1) == 0 ? 255 : 0, length);
    offset += length;
  }

  ClonePattern(pixels);
}

void StipplePenRasterizator::ClonePattern(uint8_t * pixels) const
{
  uint32_t offset = m_patternLength + 1;
  while (offset < m_pixelLength + 1)
  {
    memcpy(pixels + offset, pixels + 1, m_patternLength);
    offset += m_patternLength;
  }

  ASSERT_EQUAL(offset, m_pixelLength + 1, ());

  pixels[0] = pixels[1];
  pixels[offset] = pixels[offset - 1];
}

void StipplePenRasterizator::RasterizeTriangle(uint8_t * pixels) const
{
  // 4 values: dash (===), triangle base (tb), triangle height, base height.
  // Triangle should go on the right.
  // ===\tb /===  - base height
  //     \/     | - triangle height

  uint8_t baseH = m_key.m_pattern[3];
  ASSERT(baseH > 0, ());

  while (baseH > 0)
  {
    memset(pixels, 255, m_pixelLength + 2);

    pixels += kMaxStipplePenLength;
    --baseH;
  }

  uint8_t trgH = m_key.m_pattern[2];
  ASSERT(trgH > 0, ());
  double const tan = m_key.m_pattern[1] / double(trgH);
  ASSERT(tan > 0, ());

  while (trgH > 0)
  {
    uint8_t const base = std::round(trgH * tan);
    uint32_t const left = (m_patternLength - base) / 2;
    memset(pixels + 1, 0, left);
    memset(pixels + left + 1, 255, base);
    memset(pixels + left + 1 + base, 0, m_patternLength - left - base);

    ClonePattern(pixels);

    pixels += kMaxStipplePenLength;
    --trgH;
  }
}

ref_ptr<Texture::ResourceInfo> StipplePenIndex::ReserveResource(bool predefined, StipplePenKey const & key,
                                                                bool & newResource)
{
  TResourceMapping & resourceMapping = predefined ? m_predefinedResourceMapping : m_resourceMapping;
  auto it = resourceMapping.find(key);
  if (it != resourceMapping.end())
  {
    newResource = false;
    return make_ref(&it->second);
  }
  newResource = true;

  StipplePenRasterizator resource(key);
  m2::RectU const pixelRect = m_packer.PackResource(resource.GetSize());
  {
    std::lock_guard<std::mutex> g(m_lock);
    m_pendingNodes.emplace_back(pixelRect, resource);
  }

  auto res = resourceMapping.emplace(key, StipplePenResourceInfo(m_packer.MapTextureCoords(pixelRect),
                                                                 resource.GetSize()));
  ASSERT(res.second, ());
  return make_ref(&res.first->second);
}

ref_ptr<Texture::ResourceInfo> StipplePenIndex::MapResource(StipplePenKey const & key, bool & newResource)
{
  auto it = m_predefinedResourceMapping.find(key);
  if (it != m_predefinedResourceMapping.end())
  {
    newResource = false;
    return make_ref(&it->second);
  }

  std::lock_guard<std::mutex> g(m_mappingLock);
  return ReserveResource(false /* predefined */, key, newResource);
}

void StipplePenIndex::UploadResources(ref_ptr<dp::GraphicsContext> context, ref_ptr<Texture> texture)
{
  ASSERT(texture->GetFormat() == dp::TextureFormat::Alpha, ());
  TPendingNodes pendingNodes;
  {
    std::lock_guard<std::mutex> g(m_lock);
    if (m_pendingNodes.empty())
      return;
    m_pendingNodes.swap(pendingNodes);
  }

  // Assume that all patterns are initialized when creating texture (ReserveResource) and uploaded once.
  // Should provide additional logic like in ColorPalette::UploadResources, if we want multiple uploads.
//  if (m_uploadCalled)
//    LOG(LERROR, ("Multiple stipple pen texture uploads are not supported"));
  m_uploadCalled = true;

  uint32_t height = 0;
  for (auto const & n : pendingNodes)
    height += n.second.GetSize().y;

  uint32_t const reserveBufferSize = base::NextPowOf2(height * kMaxStipplePenLength);

  SharedBufferManager & mng = SharedBufferManager::instance();
  SharedBufferManager::shared_buffer_ptr_t ptr = mng.reserveSharedBuffer(reserveBufferSize);
  uint8_t * rawBuffer = SharedBufferManager::GetRawPointer(ptr);
  memset(rawBuffer, 0, reserveBufferSize);

  uint8_t * pixels = rawBuffer;
  for (auto const & n : pendingNodes)
  {
    n.second.Rasterize(pixels);
    pixels += (kMaxStipplePenLength * n.second.GetSize().y);
  }

  texture->UploadData(context, 0, pendingNodes.front().first.minY(), kMaxStipplePenLength, height, make_ref(rawBuffer));

  mng.freeSharedBuffer(reserveBufferSize, ptr);
}

void StipplePenTexture::ReservePattern(PenPatternT const & pattern)
{
  bool newResource = false;
  m_indexer->ReserveResource(true /* predefined */, StipplePenKey(pattern), newResource);
}
}  // namespace dp
