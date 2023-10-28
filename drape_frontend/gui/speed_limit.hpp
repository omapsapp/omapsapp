#pragma once

#include "drape_frontend/gui/elements/circle.hpp"
#include "drape_frontend/gui/shape.hpp"

namespace gui
{
class SpeedLimit : public Shape
{
  struct Config
  {
    static constexpr float kBackgroundRadius = 40.0f;
    static constexpr float kBackgroundOutlineWidthRatio = 0.2f;
    static constexpr dp::Color kBackgroundColor = dp::Color::White();
    static constexpr dp::Color kBackgroundOutlineColor = dp::Color::Red();
    static constexpr dp::Color kTextColor = dp::Color::Black();
  };

public:
  explicit SpeedLimit(gui::Position const & position);

  drape_ptr<ShapeRenderer> Draw(ref_ptr<dp::GraphicsContext> context, ref_ptr<dp::TextureManager> tex) const;

private:
  void DrawText(ref_ptr<dp::GraphicsContext> context, ShapeControl & control, ref_ptr<dp::TextureManager> tex) const;

  elements::Circle m_background;
};
}  // namespace gui
