#pragma once

#include "drape_frontend/gui/shape.hpp"

namespace gui::elements
{
class CircleHandle : public Handle
{
public:
  CircleHandle(uint32_t id, dp::Anchor anchor, m2::PointF const & pivot, m2::PointF const & size);
};

class Circle
{
public:
  using HandleCreator =
      std::function<drape_ptr<CircleHandle>(uint32_t, dp::Anchor, m2::PointF const &, m2::PointF const &)>;

  Circle() = default;

  void SetHandleId(uint32_t handleId);
  void SetPosition(Position const & position);
  void SetRadius(float radius);
  void SetOutlineWidthRatio(float widthRatio);
  void SetColor(dp::Color const & color);
  void SetOutlineColor(dp::Color const & color);
  void SetHandleCreator(HandleCreator handleCreator);

  void Draw(ref_ptr<dp::GraphicsContext> context, ShapeControl & control) const;

private:
  void Validate() const;

  uint32_t m_handleId = 0;
  Position m_position{};
  float m_radius = 0.0f;
  float m_outlineWidthRatio = 0.0f;
  dp::Color m_color = dp::Color::Transparent();
  dp::Color m_outlineColor = dp::Color::Transparent();
  HandleCreator m_handleCreator;
};
}  // namespace gui::elements
