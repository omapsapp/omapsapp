#include "drape_frontend/animation/arrow_animation.hpp"

#include <algorithm>

namespace  df
{

ArrowAnimation::ArrowAnimation(m2::PointD const & startPos, m2::PointD const & endPos, double moveDuration,
               double startAngle, double endAngle)
  : Animation(true /* couldBeInterrupted */, true /* couldBeBlended */)
  , m_positionInterpolator(moveDuration, 0.0 /* delay */, startPos, endPos)
  , m_angleInterpolator(startAngle, endAngle)
{
  m_objects.insert(Animation::Object::MyPositionArrow);
  if (m_positionInterpolator.IsActive())
    m_properties.insert(Animation::ObjectProperty::Position);
  if (m_angleInterpolator.IsActive())
    m_properties.insert(Animation::ObjectProperty::Angle);
}

void ArrowAnimation::Init(ScreenBase const &, TPropertyCache const & properties)
{
  PropertyValue value;
  double minDuration;
  double maxDuration;
  if (GetCachedProperty(properties, Animation::Object::MyPositionArrow, Animation::ObjectProperty::Position, value))
  {
    minDuration = m_positionInterpolator.GetMinDuration();
    maxDuration = m_positionInterpolator.GetMaxDuration();

    m_positionInterpolator = PositionInterpolator(m_positionInterpolator.GetDuration(),
                                                  0.0 /* delay */,
                                                  value.m_valuePointD,
                                                  m_positionInterpolator.GetTargetPosition());

    m_positionInterpolator.SetMinDuration(minDuration);
    m_positionInterpolator.SetMaxDuration(maxDuration);

    if (m_positionInterpolator.IsActive())
      m_properties.insert(Animation::ObjectProperty::Position);
  }
  if (GetCachedProperty(properties, Animation::Object::MyPositionArrow, Animation::ObjectProperty::Angle, value))
  {
    minDuration = m_angleInterpolator.GetMinDuration();
    maxDuration = m_angleInterpolator.GetMaxDuration();

    m_angleInterpolator = AngleInterpolator(value.m_valueD, m_angleInterpolator.GetTargetAngle());

    m_angleInterpolator.SetMinDuration(minDuration);
    m_angleInterpolator.SetMaxDuration(maxDuration);

    if (m_angleInterpolator.IsActive())
      m_properties.insert(Animation::ObjectProperty::Angle);
  }
}

Animation::TAnimObjects const & ArrowAnimation::GetObjects() const
{
   return m_objects;
}

bool ArrowAnimation::HasObject(Object object) const
{
  return object == Animation::Object::MyPositionArrow;
}

Animation::TObjectProperties const & ArrowAnimation::GetProperties(Object) const
{
  return m_properties;
}

bool ArrowAnimation::HasProperty(Object object, ObjectProperty property) const
{
  return HasObject(object) && m_properties.find(property) != m_properties.end();
}

void ArrowAnimation::Advance(double elapsedSeconds)
{
  if (m_positionInterpolator.IsActive())
    m_positionInterpolator.Advance(elapsedSeconds);

  if (m_angleInterpolator.IsActive())
    m_angleInterpolator.Advance(elapsedSeconds);
}

void ArrowAnimation::Finish()
{
  if (m_positionInterpolator.IsActive())
    m_positionInterpolator.Finish();

  if (m_angleInterpolator.IsActive())
    m_angleInterpolator.Finish();
}

void ArrowAnimation::SetMaxDuration(double maxDuration)
{
  if (m_positionInterpolator.IsActive())
    m_positionInterpolator.SetMaxDuration(maxDuration);

  if (m_angleInterpolator.IsActive())
    m_angleInterpolator.SetMaxDuration(maxDuration);
}

void ArrowAnimation::SetMinDuration(double minDuration)
{
  if (m_positionInterpolator.IsActive())
    m_positionInterpolator.SetMinDuration(minDuration);

  if (m_angleInterpolator.IsActive())
    m_angleInterpolator.SetMinDuration(minDuration);
}

double ArrowAnimation::GetDuration() const
{
  double duration = 0.0;
  if (m_angleInterpolator.IsActive())
    duration = m_angleInterpolator.GetDuration();
  if (m_positionInterpolator.IsActive())
    duration = std::max(duration, m_positionInterpolator.GetDuration());
  return duration;
}

double ArrowAnimation::GetMaxDuration() const
{
  double maxDuration = Animation::kInvalidAnimationDuration;

  if (!Animation::GetMaxDuration(m_angleInterpolator, maxDuration) ||
      !Animation::GetMaxDuration(m_positionInterpolator, maxDuration))
    return Animation::kInvalidAnimationDuration;

  return maxDuration;
}

double ArrowAnimation::GetMinDuration() const
{
  double minDuration = Animation::kInvalidAnimationDuration;

  if (!Animation::GetMinDuration(m_angleInterpolator, minDuration) ||
      !Animation::GetMinDuration(m_positionInterpolator, minDuration))
    return Animation::kInvalidAnimationDuration;

  return minDuration;
}

bool ArrowAnimation::IsFinished() const
{
  return m_positionInterpolator.IsFinished() && m_angleInterpolator.IsFinished();
}

bool ArrowAnimation::GetProperty(Object object, ObjectProperty property, PropertyValue & value) const
{
  return GetProperty(object, property, false /* targetValue */, value);
}

bool ArrowAnimation::GetTargetProperty(Object object, ObjectProperty property, PropertyValue & value) const
{
  return GetProperty(object, property, true /* targetValue */, value);
}

bool ArrowAnimation::GetProperty(Object object, ObjectProperty property, bool targetValue, PropertyValue & value) const
{
  ASSERT_EQUAL(static_cast<int>(object), static_cast<int>(Animation::Object::MyPositionArrow), ());

  switch (property)
  {
  case Animation::ObjectProperty::Position:
    if (m_positionInterpolator.IsActive())
    {
      value = PropertyValue(targetValue ? m_positionInterpolator.GetTargetPosition() : m_positionInterpolator.GetPosition());
      return true;
    }
    return false;
  case Animation::ObjectProperty::Angle:
    if (m_angleInterpolator.IsActive())
    {
      value = PropertyValue(targetValue ? m_angleInterpolator.GetTargetAngle() : m_angleInterpolator.GetAngle());
      return true;
    }
    return false;
  default:
    ASSERT(false, ("Wrong property:", static_cast<int>(property)));
  }

  return false;
}


} // namespace df
