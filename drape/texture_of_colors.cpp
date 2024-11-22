#include "drape/texture_of_colors.hpp"

#include "base/shared_buffer_manager.hpp"
#include "base/stl_helpers.hpp"

#include <cstring>

namespace dp
{

namespace
{
  int const kResourceSize = 2;
  int const kBytesPerPixel = 4;
}

ColorPalette::ColorPalette(m2::PointU const & canvasSize)
   : m_textureSize(canvasSize)
   , m_cursor(m2::PointU::Zero())
{}

ref_ptr<Texture::ResourceInfo> ColorPalette::ReserveResource(bool predefined, ColorKey const & key, bool & newResource)
{
  TPalette & palette = predefined ? m_predefinedPalette : m_palette;
  auto itm = palette.find(key.m_color);
  newResource = (itm == palette.end());
  if (newResource)
  {
    PendingColor pendingColor;
    pendingColor.m_color = key.m_color;
    pendingColor.m_rect = m2::RectU(m_cursor.x, m_cursor.y,
                                    m_cursor.x + kResourceSize, m_cursor.y + kResourceSize);
    {
      std::lock_guard<std::mutex> g(m_lock);
      m_pendingNodes.push_back(pendingColor);
    }

    m_cursor.x += kResourceSize;
    if (m_cursor.x >= m_textureSize.x)
    {
      m_cursor.y += kResourceSize;
      m_cursor.x = 0;

      ASSERT(m_cursor.y < m_textureSize.y, ());
    }

    float const sizeX = static_cast<float>(m_textureSize.x);
    float const sizeY = static_cast<float>(m_textureSize.y);
    m2::PointF const resCenter = m2::RectF(pendingColor.m_rect).Center();
    float const x = resCenter.x / sizeX;
    float const y = resCenter.y / sizeY;
    auto res = palette.emplace(key.m_color, ColorResourceInfo(m2::RectF(x, y, x, y)));
    ASSERT(res.second, ());
    itm = res.first;
  }

  return make_ref(&itm->second);
}

ref_ptr<Texture::ResourceInfo> ColorPalette::MapResource(ColorKey const & key, bool & newResource)
{
  auto itm = m_predefinedPalette.find(key.m_color);
  if (itm != m_predefinedPalette.end())
  {
    newResource = false;
    return make_ref(&itm->second);
  }

  std::lock_guard<std::mutex> lock(m_mappingLock);
  return ReserveResource(false /* predefined */, key, newResource);
}

void ColorPalette::UploadResources(ref_ptr<dp::GraphicsContext> context, ref_ptr<Texture> texture)
{
  ASSERT(texture->GetFormat() == dp::TextureFormat::RGBA8, ());
  bool const hasPartialTextureUpdate = context->HasPartialTextureUpdates();

  std::vector<PendingColor> pendingNodes;
  {
    std::lock_guard<std::mutex> g(m_lock);
    if (m_pendingNodes.empty())
      return;

    if (hasPartialTextureUpdate)
    {
      pendingNodes.swap(m_pendingNodes);
    }
    else
    {
      // Store all colors in m_nodes, because we should update *whole* texture, if partial update is not available.
      m_nodes.insert(m_nodes.end(), m_pendingNodes.begin(), m_pendingNodes.end());
      m_pendingNodes.clear();
      pendingNodes = m_nodes;
    }
  }

  if (!hasPartialTextureUpdate)
  {
    PendingColor lastPendingColor = pendingNodes.back();
    lastPendingColor.m_color = Color::Transparent();
    while (lastPendingColor.m_rect.maxX() < m_textureSize.x)
    {
      lastPendingColor.m_rect.Offset(kResourceSize, 0);
      pendingNodes.push_back(lastPendingColor);
    }
  }

  buffer_vector<size_t, 4> ranges;
  ranges.push_back(0);

  uint32_t minX = pendingNodes[0].m_rect.minX();
  for (size_t i = 1; i < pendingNodes.size(); ++i)
  {
    m2::RectU const & currentRect = pendingNodes[i].m_rect;
    if (minX > currentRect.minX())
    {
      ranges.push_back(i);
      minX = currentRect.minX();
    }
  }

  ASSERT(hasPartialTextureUpdate || ranges.size() == 1, ());

  ranges.push_back(pendingNodes.size());
  for (size_t i = 1; i < ranges.size(); ++i)
  {
    size_t startRange = ranges[i - 1];
    size_t endRange = ranges[i];

    m2::RectU const & startRect = pendingNodes[startRange].m_rect;
    m2::RectU const & endRect = pendingNodes[endRange - 1].m_rect;

    m2::RectU uploadRect;
    if (startRect.minY() == endRect.minY() &&
        startRect.maxY() == endRect.maxY())
    {
      uploadRect = m2::RectU(startRect.minX(), startRect.minY(), endRect.maxX(), endRect.maxY());
    }
    else
    {
      ASSERT(startRect.minX() == 0, ());
      uploadRect = m2::RectU(0, startRect.minY(), m_textureSize.x, endRect.maxY());
    }

    size_t const pixelStride = uploadRect.SizeX();
    size_t const byteCount = kBytesPerPixel * uploadRect.SizeX() * uploadRect.SizeY();
    size_t const bufferSize = static_cast<size_t>(math::NextPowOf2(static_cast<uint32_t>(byteCount)));

    SharedBufferManager::shared_buffer_ptr_t buffer = SharedBufferManager::instance().reserveSharedBuffer(bufferSize);
    uint8_t * pointer = SharedBufferManager::GetRawPointer(buffer);
    if (m_isDebug)
      memset(pointer, 0, bufferSize);

    uint32_t currentY = startRect.minY();
    for (size_t j = startRange; j < endRange; ++j)
    {
      ASSERT(pointer < SharedBufferManager::GetRawPointer(buffer) + byteCount, ());
      PendingColor const & c = pendingNodes[j];
      if (c.m_rect.minY() > currentY)
      {
        pointer += kBytesPerPixel * pixelStride;
        currentY = c.m_rect.minY();
      }

      uint32_t const byteStride = static_cast<uint32_t>(pixelStride * kBytesPerPixel);
      uint8_t const red = c.m_color.GetRed();
      uint8_t const green = c.m_color.GetGreen();
      uint8_t const blue = c.m_color.GetBlue();
      uint8_t const alpha = c.m_color.GetAlpha();

      for (size_t row = 0; row < kResourceSize; row++)
      {
        for (size_t column = 0; column < kResourceSize; column++)
        {
          pointer[row * byteStride + column * kBytesPerPixel] = red;
          pointer[row * byteStride + column * kBytesPerPixel + 1] = green;
          pointer[row * byteStride + column * kBytesPerPixel + 2] = blue;
          pointer[row * byteStride + column * kBytesPerPixel + 3] = alpha;
        }
      }

      pointer += kResourceSize * kBytesPerPixel;
      ASSERT(pointer <= SharedBufferManager::GetRawPointer(buffer) + byteCount, ());
    }

    pointer = SharedBufferManager::GetRawPointer(buffer);
    texture->UploadData(context, uploadRect.minX(), uploadRect.minY(),
                        uploadRect.SizeX(), uploadRect.SizeY(), make_ref(pointer));
  }
}

void ColorTexture::ReserveColor(dp::Color const & color)
{
  bool newResource = false;
  m_indexer->ReserveResource(true /* predefined */, ColorKey(color), newResource);
}

int ColorTexture::GetColorSizeInPixels()
{
  return kResourceSize;
}

} // namespace dp
