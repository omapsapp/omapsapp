/* This test crashes with ASSERT_NOT_EQUAL(CurrentApiVersion, dp::ApiVersion::Invalid, ()); in gl_functions.cpp
#include "drape/drape_tests/dummy_texture.hpp"
#include "drape/drape_tests/gl_mock_functions.hpp"
#include "drape/drape_tests/img.hpp"
#include "drape/drape_tests/testing_graphics_context.hpp"

#include "base/file_name_utils.hpp"

#include "platform/platform.hpp"
#include "qt_tstfrm/test_main_loop.hpp"
#include "testing/testing.hpp"

#include "drape/drape_routine.hpp"
#include "drape/font_constants.hpp"
#include "drape/font_texture.hpp"
#include "drape/glyph_manager.hpp"

#include <functional>

#include <QtCore/QPoint>
#include <QtGui/QPainter>

#include <gmock/gmock.h>

using ::testing::_;
using ::testing::Return;
using ::testing::InSequence;
using ::testing::AnyNumber;
using ::testing::Invoke;
using namespace dp;
using namespace std::placeholders;

namespace
{
class UploadedRender
{
public:
  explicit UploadedRender(QPoint const & pen) : m_pen(pen) {}

  void glMemoryToQImage(int x, int y, int w, int h, glConst f, glConst t, void const * memory)
  {
    TEST(f == gl_const::GLAlpha || f == gl_const::GLAlpha8 || f == gl_const::GLRed, ());
    TEST(t == gl_const::GLUnsignedByteType, ());

    uint8_t const * image = reinterpret_cast<uint8_t const *>(memory);

    QPoint p(m_pen);
    p.rx() += x;
    m_images.push_back(qMakePair(p, CreateImage(w, h, image)));
    m_pen.ry() += h;
  }

  void Render(QPaintDevice * device)
  {
    QPainter p(device);
    for (auto const & d : m_images)
      p.drawImage(d.first, d.second);
  }

private:
  QPoint m_pen;
  QVector<QPair<QPoint, QImage>> m_images;
};

class DummyGlyphIndex : public GlyphIndex
{
public:
  DummyGlyphIndex(m2::PointU size, ref_ptr<GlyphManager> mng)
    : GlyphIndex(size, mng)
  {}
  ref_ptr<Texture::ResourceInfo> MapResource(GlyphKey const & key)
  {
    bool dummy = false;
    return GlyphIndex::MapResource(key, dummy);
  }
};
}  // namespace

UNIT_TEST(UploadingGlyphs)
{
  // Set QT_QPA_PLATFORM=offscreen env var to avoid running GUI on Linux
  DrapeRoutine::Init();
  EXPECTGL(glHasExtension(_)).Times(AnyNumber());
  EXPECTGL(glBindTexture(_)).Times(AnyNumber());
  EXPECTGL(glDeleteTexture(_)).Times(AnyNumber());
  EXPECTGL(glTexParameter(_, _)).Times(AnyNumber());
  EXPECTGL(glTexImage2D(_, _, _, _, _)).Times(AnyNumber());
  EXPECTGL(glGenTexture()).Times(AnyNumber());

  UploadedRender r(QPoint(10, 10));
  dp::GlyphManager::Params args;
  args.m_uniBlocks = base::JoinPath("fonts", "unicode_blocks.txt");
  args.m_whitelist = base::JoinPath("fonts", "whitelist.txt");
  args.m_blacklist = base::JoinPath("fonts", "blacklist.txt");
  GetPlatform().GetFontNames(args.m_fonts);

  uint32_t constexpr kTextureSize = 1024;
  GlyphManager mng(args);
  DummyGlyphIndex index(m2::PointU(kTextureSize, kTextureSize), make_ref(&mng));
  size_t count = 1;  // invalid symbol glyph has been mapped internally.
  count += (index.MapResource(GlyphKey(0x58)) != nullptr) ? 1 : 0;
  count += (index.MapResource(GlyphKey(0x59)) != nullptr) ? 1 : 0;
  count += (index.MapResource(GlyphKey(0x61)) != nullptr) ? 1 : 0;
  while (index.GetPendingNodesCount() < count)
    ;

  TestingGraphicsContext context;
  Texture::Params p;
  p.m_allocator = GetDefaultAllocator(make_ref(&context));
  p.m_format = dp::TextureFormat::Alpha;
  p.m_width = p.m_height = kTextureSize;

  DummyTexture tex;
  tex.Create(make_ref(&context), p);
  EXPECTGL(glTexSubImage2D(_, _, _, _, _, _, _))
      .WillRepeatedly(Invoke(&r, &UploadedRender::glMemoryToQImage));
  index.UploadResources(make_ref(&context), make_ref(&tex));

  count = 0;
  count += (index.MapResource(GlyphKey(0x68)) != nullptr) ? 1 : 0;
  count += (index.MapResource(GlyphKey(0x30)) != nullptr) ? 1 : 0;
  count += (index.MapResource(GlyphKey(0x62)) != nullptr) ? 1 : 0;
  count += (index.MapResource(GlyphKey(0x65)) != nullptr) ? 1 : 0;
  count += (index.MapResource(GlyphKey(0x400)) != nullptr) ? 1 : 0;
  count += (index.MapResource(GlyphKey(0x401)) != nullptr) ? 1 : 0;
// TODO: Fix this condition
//while (index.GetPendingNodesCount() < count)
//  ;

  EXPECTGL(glTexSubImage2D(_, _, _, _, _, _, _))
      .WillRepeatedly(Invoke(&r, &UploadedRender::glMemoryToQImage));
  index.UploadResources(make_ref(&context), make_ref(&tex));

  RunTestLoop("UploadingGlyphs", std::bind(&UploadedRender::Render, &r, _1));
  DrapeRoutine::Shutdown();
}
*/