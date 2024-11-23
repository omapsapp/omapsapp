#include "testing/testing.hpp"

#include "drape/drape_tests/img.hpp"

#include "drape/font_constants.hpp"
#include "drape/glyph_manager.hpp"
#include "drape/harfbuzz_shaping.hpp"

#include "base/file_name_utils.hpp"

#include "qt_tstfrm/test_main_loop.hpp"

#include "platform/platform.hpp"

#include <QtGui/QPainter>

#include <functional>
#include <iostream>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include <hb-ft.h>

namespace glyph_mng_tests
{
class GlyphRenderer
{
  FT_Library m_freetypeLibrary;
  std::string m_utf8;
  int m_fontPixelSize;
  char const * m_lang;

  static constexpr FT_Int kSdfSpread {dp::kSdfBorder};

public:
  GlyphRenderer()
  {
    // Initialize FreeType
    TEST_EQUAL(0, FT_Init_FreeType(&m_freetypeLibrary), ("Can't initialize FreeType"));
    for (auto const module : {"sdf", "bsdf"})
      TEST_EQUAL(0, FT_Property_Set(m_freetypeLibrary, module, "spread", &kSdfSpread), ());

    dp::GlyphManager::Params args;
    args.m_uniBlocks = base::JoinPath("fonts", "unicode_blocks.txt");
    args.m_whitelist = base::JoinPath("fonts", "whitelist.txt");
    args.m_blacklist = base::JoinPath("fonts", "blacklist.txt");
    GetPlatform().GetFontNames(args.m_fonts);

    m_mng = std::make_unique<dp::GlyphManager>(args);
  }

  ~GlyphRenderer()
  {
    FT_Done_FreeType(m_freetypeLibrary);
  }

  void SetString(std::string const & s, int fontPixelSize, char const * lang)
  {
    m_utf8 = s;
    m_fontPixelSize = fontPixelSize;
    m_lang = lang;
  }

  static float Smoothstep(float edge0, float edge1, float x)
  {
    x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return x * x * (3 - 2 * x);
  }

  static float PixelColorFromDistance(float distance)
  {
    //float const normalizedDistance = (distance - 128.f) / 128.f;
    float const normalizedDistance = distance / 255.f;
    static constexpr float kFontScale = 1.f;
    static constexpr float kSmoothing = 0.25f / (kSdfSpread * kFontScale);
    float const alpha = Smoothstep(0.5f - kSmoothing, 0.5f + kSmoothing, normalizedDistance);
    return 255.f * alpha;
  }

  void RenderGlyphs(QPaintDevice * device) const
  {
    QPainter painter(device);
    painter.fillRect(QRectF(0.0, 0.0, device->width(), device->height()), Qt::white);

    auto const shapedText = m_mng->ShapeText(m_utf8, m_fontPixelSize, m_lang);

    std::cout << "Total width: " << shapedText.m_lineWidthInPixels << '\n';
    std::cout << "Max height: " << shapedText.m_maxLineHeightInPixels << '\n';

    int constexpr kLineStartX = 10;
    int constexpr kLineMarginY = 50;
    QPoint pen(kLineStartX, kLineMarginY);

    for (auto const & glyph : shapedText.m_glyphs)
    {
      constexpr bool kUseSdfBitmap = false;
      dp::GlyphImage img = m_mng->GetGlyphImage(glyph.m_key, m_fontPixelSize, kUseSdfBitmap);

      auto const w = img.m_width;
      auto const h = img.m_height;
      // Spaces do not have images.
      if (w && h)
      {
        QPoint currentPen = pen;
        currentPen.rx() += glyph.m_xOffset;
        // Image is drawn at the top left origin, text metrics returns bottom left origin.
        currentPen.ry() -= glyph.m_yOffset + h;
        painter.drawImage(currentPen, CreateImage(w, h, img.m_data->data()), QRect(0, 0, w, h));
      }
      pen += QPoint(glyph.m_xAdvance, glyph.m_yAdvance /* 0 for horizontal texts */);

      img.Destroy();
    }

    pen.rx() = kLineStartX;
    pen.ry() += kLineMarginY;

    for (auto const & glyph : shapedText.m_glyphs)
    {
      constexpr bool kUseSdfBitmap = true;
      auto img = m_mng->GetGlyphImage(glyph.m_key, m_fontPixelSize, kUseSdfBitmap);

      auto const w = img.m_width;
      auto const h = img.m_height;
      // Spaces do not have images.
      if (w && h)
      {
        QPoint currentPen = pen;
        currentPen.rx() += glyph.m_xOffset;
        currentPen.ry() -= glyph.m_yOffset + h;
        painter.drawImage(currentPen, CreateImage(w, h, img.m_data->data()),
            QRect(dp::kSdfBorder, dp::kSdfBorder, w - 2 * dp::kSdfBorder, h - 2 * dp::kSdfBorder));
      }
      pen += QPoint(glyph.m_xAdvance, glyph.m_yAdvance /* 0 for horizontal texts */);

      img.Destroy();
    }

    ////////////////////////////////////////////////////////////////////////////
    // Manual rendering using HB functions.
    {
      pen.rx() = kLineStartX;
      pen.ry() += kLineMarginY;

      auto const hbLanguage = hb_language_from_string(m_lang, -1);

      auto const runs = harfbuzz_shaping::GetTextSegments(m_utf8);
      for (auto const & segment : runs.m_segments)
      {
        hb_buffer_t * buf = hb_buffer_create();
        hb_buffer_add_utf16(buf, reinterpret_cast<const uint16_t *>(runs.m_text.data()), runs.m_text.size(), segment.m_start, segment.m_length);
        hb_buffer_set_direction(buf, segment.m_direction);
        hb_buffer_set_script(buf, segment.m_script);
        hb_buffer_set_language(buf, hbLanguage);

        // If direction, script, and language are not known.
        // hb_buffer_guess_segment_properties(buf);

        std::string const lang = m_lang;
        std::string const fontFileName = lang == "ar" ? "00_NotoNaskhArabic-Regular.ttf" : "07_roboto_medium.ttf";

        auto reader = GetPlatform().GetReader("fonts/" + fontFileName);
        auto fontFile = reader->GetName();
        FT_Face face;
        if (FT_New_Face(m_freetypeLibrary, fontFile.c_str(), 0, &face)) {
          std::cerr << "Can't load font " << fontFile << '\n';
          return;
        }
        // Set character size
        FT_Set_Pixel_Sizes(face, 0 , m_fontPixelSize );
        // This also works.
        // if (FT_Set_Char_Size(face, 0, m_fontPixelSize << 6, 0, 0)) {
        //   std::cerr << "Can't set character size\n";
        //   return;
        // }

        // Set no transform (identity)
        //FT_Set_Transform(face, nullptr, nullptr);

        // Load font into HarfBuzz
        hb_font_t *font = hb_ft_font_create(face, nullptr);

        // Shape!
        hb_shape(font, buf, nullptr, 0);

        // Get the glyph and position information.
        unsigned int glyph_count;
        hb_glyph_info_t *glyph_info    = hb_buffer_get_glyph_infos(buf, &glyph_count);
        hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

        for (unsigned int i = 0; i < glyph_count; i++)
        {
          hb_codepoint_t const glyphid = glyph_info[i].codepoint;

          FT_Int32 const flags =  FT_LOAD_RENDER;
          FT_Load_Glyph(face, glyphid, flags);
          FT_Render_Glyph(face->glyph, FT_RENDER_MODE_SDF);

          FT_GlyphSlot slot = face->glyph;

          FT_Bitmap const & ftBitmap = slot->bitmap;

          auto const buffer = ftBitmap.buffer;
          auto const width = ftBitmap.width;
          auto const height = ftBitmap.rows;

          for (unsigned h = 0; h < height; ++h)
          {
            for (unsigned w = 0; w < width; ++w)
            {
              auto curPixelAddr = buffer + h * width + w;
              float currPixel = *curPixelAddr;
              currPixel = PixelColorFromDistance(currPixel);
              *curPixelAddr = static_cast<unsigned char>(currPixel);
            }
          }

          auto const bearing_x = slot->metrics.horiBearingX;//slot->bitmap_left;
          auto const bearing_y = slot->metrics.horiBearingY;//slot->bitmap_top;

          auto const & glyphPos = glyph_pos[i];
          hb_position_t const x_offset  = (glyphPos.x_offset + bearing_x) >> 6;
          hb_position_t const y_offset  = (glyphPos.y_offset + bearing_y) >> 6;
          hb_position_t const x_advance = glyphPos.x_advance >> 6;
          hb_position_t const y_advance = glyphPos.y_advance >> 6;

          // Empty images are possible for space characters.
          if (width != 0 && height != 0)
          {
            QPoint currentPen = pen;
            currentPen.rx() += x_offset;
            currentPen.ry() -= y_offset;
            painter.drawImage(currentPen, CreateImage(width, height, buffer), QRect(kSdfSpread, kSdfSpread, width - 2*kSdfSpread, height - 2*kSdfSpread));
          }
          pen += QPoint(x_advance, y_advance);
        }

        // Tidy up.
        hb_buffer_destroy(buf);
        hb_font_destroy(font);
        FT_Done_Face(face);
      }
    }

    //////////////////////////////////////////////////////////////////
    // QT text renderer.
    {
      pen.rx() = kLineStartX;
      pen.ry() += kLineMarginY;

      //QFont font("Noto Naskh Arabic");
      QFont font("Roboto");
      font.setPixelSize(m_fontPixelSize);
      //font.setWeight(QFont::Weight::Normal);
      painter.setFont(font);
      painter.drawText(pen, QString::fromUtf8(m_utf8.c_str(), m_utf8.size()));
    }

  }

private:
  std::unique_ptr<dp::GlyphManager> m_mng;
};

// This unit test creates a window so can't be run in GUI-less Linux machine.
// Make sure that the QT_QPA_PLATFORM=offscreen environment variable is set.
UNIT_TEST(GlyphLoadingTest)
{
  GlyphRenderer renderer;

  using namespace std::placeholders;

  constexpr int fontSize = 27;

  renderer.SetString("Muḩāfaz̧at", fontSize, "en");
  RunTestLoop("Latin Extended", std::bind(&GlyphRenderer::RenderGlyphs, &renderer, _1));

  renderer.SetString("Строка", fontSize, "ru");
  RunTestLoop("ru", std::bind(&GlyphRenderer::RenderGlyphs, &renderer, _1));

  renderer.SetString("ØŒÆ", fontSize, "en");
  RunTestLoop("en", std::bind(&GlyphRenderer::RenderGlyphs, &renderer, _1));

  renderer.SetString("𫝚 𫝛 𫝜", fontSize, "zh");
  RunTestLoop("CJK Surrogates", std::bind(&GlyphRenderer::RenderGlyphs, &renderer, _1));

  renderer.SetString("الحلّة گلها"" كسول الزنجبيل القط""56""عين علي (الحربية)""123"" اَلْعَرَبِيَّةُ", fontSize, "ar");
  RunTestLoop("Arabic1", std::bind(&GlyphRenderer::RenderGlyphs, &renderer, _1));

  renderer.SetString("12345""گُلها""12345""گُلها""12345", fontSize, "ar");
  RunTestLoop("Arabic2", std::bind(&GlyphRenderer::RenderGlyphs, &renderer, _1));

  renderer.SetString("മനക്കലപ്പടി", fontSize, "ml");
  RunTestLoop("Malay", std::bind(&GlyphRenderer::RenderGlyphs, &renderer, _1));

  renderer.SetString("Test 12 345 ""گُلها""678 9000 Test", fontSize, "ar");
  RunTestLoop("Arabic Mixed", std::bind(&GlyphRenderer::RenderGlyphs, &renderer, _1));

  renderer.SetString("NFKC Razdoĺny NFKD Razdoĺny", fontSize, "be");
  RunTestLoop("Polish", std::bind(&GlyphRenderer::RenderGlyphs, &renderer, _1));
}

}  // namespace glyph_mng_tests
