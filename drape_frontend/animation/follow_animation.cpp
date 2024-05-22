#include "drape_frontend/animation/follow_animation.hpp"

#include "drape_frontend/animation_constants.hpp"
#include "drape_frontend/animation_system.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <algorithm>

namespace df
{

MapFollowAnimation::MapFollowAnimation(ScreenBase const & screen,
                                       m2::PointD const & globalUserPosition,
                                       m2::PointD const & endPixelPosition,
                                       double endScale, double endAngle,
                                       bool isAutoZoom)
  : Animation(true /* couldBeInterrupted */, true /* couldBeBlended */)
  , m_isAutoZoom(isAutoZoom)
  , m_globalPosition(globalUserPosition)
  , m_endPixelPosition(endPixelPosition)
  , m_endScale(endScale)
  , m_endAngle(endAngle)
{
  TPropertyCache properties;
  Init(screen, properties);
}

void MapFollowAnimation::Init(ScreenBase const & screen, TPropertyCache const & properties)
{
  ScreenBase currentScreen;
  GetCurrentScreen(properties, screen, currentScreen);

  double minDuration = m_offsetInterpolator.GetMinDuration();
  double maxDuration = m_offsetInterpolator.GetMaxDuration();

  m_offset = currentScreen.PtoG(currentScreen.P3dtoP(m_endPixelPosition)) - m_globalPosition;
  double const averageScale = m_isAutoZoom ? currentScreen.GetScale() : (currentScreen.GetScale() + m_endScale) / 2.0;
  double const moveDuration = PositionInterpolator::GetMoveDuration(m_offset.Length(), screen.PixelRectIn3d(), averageScale);
  m_offsetInterpolator = PositionInterpolator(moveDuration, 0.0, m_offset, m2::PointD(0.0, 0.0));

  m_offsetInterpolator.SetMinDuration(minDuration);
  m_offsetInterpolator.SetMaxDuration(maxDuration);

  maxDuration = m_scaleInterpolator.GetMaxDuration();
  m_scaleInterpolator = ScaleInterpolator(currentScreen.GetScale(), m_endScale, m_isAutoZoom);
  m_scaleInterpolator.SetMaxDuration(maxDuration);

  maxDuration = m_angleInterpolator.GetMaxDuration();
  m_angleInterpolator = AngleInterpolator(currentScreen.GetAngle(), m_endAngle);
  m_angleInterpolator.SetMaxDuration(maxDuration);

  double const duration = CalculateDuration();

  m_scaleInterpolator.SetMinDuration(duration);
  m_angleInterpolator.SetMinDuration(duration);

  m_objects.insert(Animation::Object::MapPlane);

  if (m_scaleInterpolator.IsActive())
    m_properties.insert(Animation::ObjectProperty::Scale);
  if (m_angleInterpolator.IsActive())
    m_properties.insert(Animation::ObjectProperty::Angle);
  if (m_offsetInterpolator.IsActive() || m_scaleInterpolator.IsActive() || m_angleInterpolator.IsActive())
    m_properties.insert(Animation::ObjectProperty::Position);

  // If MapFollowAnimation affects only angles, disable rewinding.
  SetCouldBeRewinded(!m_angleInterpolator.IsActive() || m_scaleInterpolator.IsActive() ||
                     m_offsetInterpolator.IsActive());
}

Animation::TObjectProperties const & MapFollowAnimation::GetProperties(Object object) const
{
  ASSERT_EQUAL(static_cast<int>(object), static_cast<int>(Animation::Object::MapPlane), ());
  return m_properties;
}

bool MapFollowAnimation::HasProperty(Object object, ObjectProperty property) const
{
  return HasObject(object) && m_properties.find(property) != m_properties.end();
}

void MapFollowAnimation::Advance(double elapsedSeconds)
{
  if (m_angleInterpolator.IsActive())
    m_angleInterpolator.Advance(elapsedSeconds);
  if (m_scaleInterpolator.IsActive())
    m_scaleInterpolator.Advance(elapsedSeconds);
  if (m_offsetInterpolator.IsActive())
    m_offsetInterpolator.Advance(elapsedSeconds);
}

void MapFollowAnimation::Finish()
{
  if (m_angleInterpolator.IsActive())
    m_angleInterpolator.Finish();
  if (m_scaleInterpolator.IsActive())
  {
    if (m_isAutoZoom)
      m_scaleInterpolator.SetActive(false);
    else
      m_scaleInterpolator.Finish();
  }
  if (m_offsetInterpolator.IsActive())
    m_offsetInterpolator.Finish();
  Animation::Finish();
}

void MapFollowAnimation::SetMaxDuration(double maxDuration)
{
  if (m_angleInterpolator.IsActive())
    m_angleInterpolator.SetMaxDuration(maxDuration);
  if (!m_isAutoZoom && m_scaleInterpolator.IsActive())
    m_scaleInterpolator.SetMaxDuration(maxDuration);
  if (m_offsetInterpolator.IsActive())
    m_offsetInterpolator.SetMaxDuration(maxDuration);
}

void MapFollowAnimation::SetMinDuration(double minDuration)
{
  if (m_angleInterpolator.IsActive())
    m_angleInterpolator.SetMinDuration(minDuration);
  if (!m_isAutoZoom && m_scaleInterpolator.IsActive())
    m_scaleInterpolator.SetMinDuration(minDuration);
  if (m_offsetInterpolator.IsActive())
    m_offsetInterpolator.SetMinDuration(minDuration);
}

double MapFollowAnimation::GetDuration() const
{
  return CalculateDuration();
}

double MapFollowAnimation::GetMaxDuration() const
{
  double maxDuration = Animation::kInvalidAnimationDuration;

  if (!Animation::GetMaxDuration(m_angleInterpolator, maxDuration) ||
      (!m_isAutoZoom && !Animation::GetMaxDuration(m_scaleInterpolator, maxDuration)) ||
      !Animation::GetMaxDuration(m_offsetInterpolator, maxDuration))
    return Animation::kInvalidAnimationDuration;

  return maxDuration;
}

double MapFollowAnimation::GetMinDuration() const
{
  double minDuration = Animation::kInvalidAnimationDuration;

  if (!Animation::GetMinDuration(m_angleInterpolator, minDuration) ||
      (!m_isAutoZoom && !Animation::GetMinDuration(m_scaleInterpolator, minDuration)) ||
      !Animation::GetMinDuration(m_offsetInterpolator, minDuration))
    return Animation::kInvalidAnimationDuration;

  return minDuration;
}


double MapFollowAnimation::CalculateDuration() const
{
  double duration = std::max(m_angleInterpolator.GetDuration(), m_offsetInterpolator.GetDuration());
  if (!m_isAutoZoom)
    duration = std::max(duration, m_scaleInterpolator.GetDuration());
  return duration;
}

bool MapFollowAnimation::IsFinished() const
{
  return m_angleInterpolator.IsFinished() && m_scaleInterpolator.IsFinished() && m_offsetInterpolator.IsFinished();
}

bool MapFollowAnimation::GetProperty(Object object, ObjectProperty property, PropertyValue & value) const
{
  return GetProperty(object, property, false /* targetValue */, value);
}

bool MapFollowAnimation::GetTargetProperty(Object object, ObjectProperty property, PropertyValue & value) const
{
  return GetProperty(object, property, true /* targetValue */, value);
}

bool MapFollowAnimation::GetProperty(Object, ObjectProperty property, bool targetValue, PropertyValue & value) const
{
  if (property == Animation::ObjectProperty::Position)
  {
    ScreenBase tmp = AnimationSystem::Instance().GetLastScreen();
    if (targetValue)
    {
      tmp.SetFromParams(m_globalPosition, m_angleInterpolator.GetTargetAngle(), m_isAutoZoom ? m_scaleInterpolator.GetScale()
                                                                                             : m_scaleInterpolator.GetTargetScale());
      tmp.MatchGandP3d(m_globalPosition, m_endPixelPosition);
    }
    else
    {
      double const scale = m_scaleInterpolator.GetScale() / m_scaleInterpolator.GetStartScale();
      double const angle = m_angleInterpolator.GetAngle() - m_angleInterpolator.GetStartAngle();
      m2::PointD offset = m_offsetInterpolator.GetPosition() * scale;
      offset.Rotate(angle);
      m2::PointD pos = m_globalPosition + offset;

      tmp.SetFromParams(m_globalPosition, m_angleInterpolator.GetAngle(), m_scaleInterpolator.GetScale());
      tmp.MatchGandP3d(pos, m_endPixelPosition);
    }
    value = PropertyValue(tmp.GetOrg());
    return true;
  }
  if (property == Animation::ObjectProperty::Angle)
  {
    value = PropertyValue(targetValue ? m_angleInterpolator.GetTargetAngle() : m_angleInterpolator.GetAngle());
    return true;
  }
  if (property == Animation::ObjectProperty::Scale)
  {
    value = PropertyValue((targetValue && !m_isAutoZoom) ? m_scaleInterpolator.GetTargetScale()
                                                         : m_scaleInterpolator.GetScale());
    return true;
  }
  ASSERT(false, ("Wrong property:", static_cast<int>(property)));
  return false;
}

bool MapFollowAnimation::HasScale() const
{
  return m_scaleInterpolator.IsActive();
}

bool MapFollowAnimation::HasPixelOffset() const
{
  return m_offsetInterpolator.IsActive();
}

} // namespace df
