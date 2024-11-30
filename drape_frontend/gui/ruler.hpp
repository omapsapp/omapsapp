#pragma once

#include "drape_frontend/gui/drape_gui.hpp"
#include "drape_frontend/gui/ruler_helper.hpp"
#include "drape_frontend/gui/shape.hpp"

namespace gui
{
class Ruler : public Shape
{
public:
  explicit Ruler(Position const & position)
    : Shape(position)
  {
    // Localize text of ruler.
    DrapeGui::GetRulerHelper().LocalizeTexts();
  }
  drape_ptr<ShapeRenderer> Draw(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::TextureManager> tex) const;

private:
  void DrawRuler(ref_ptr<dp::GraphicsContext> context, ShapeControl & control,
                 ref_ptr<dp::TextureManager> tex, bool isAppearing) const;
  void DrawText(ref_ptr<dp::GraphicsContext> context, ShapeControl & control,
                ref_ptr<dp::TextureManager> tex, bool isAppearing) const;
};
}  // namespace gui
