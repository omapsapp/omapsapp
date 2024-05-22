#include "kinetic_scroller.hpp"
#include "visual_params.hpp"

#include <algorithm>

namespace df
{
double constexpr kKineticDuration = 1.5;
double constexpr kKineticFadeoff = 4.0;
double constexpr kKineticAcceleration = 0.4;

// Time window to store move points for better (smooth) _instant_ velocity calculation.
double constexpr kTimeWindowSec = 0.05;
double constexpr kMinDragTimeSec = 0.01;

/// @name Generic pixels per second. Should multiply on visual scale.
/// @{
double constexpr kKineticMaxSpeedStart = 1000.0;
double constexpr kKineticMaxSpeedEnd = 5000.0;
double constexpr kInstantVelocityThreshold = 200.0;
/// @}

double CalculateKineticMaxSpeed(ScreenBase const & modelView)
{
  double const lerpCoef = 1.0 - GetNormalizedZoomLevel(modelView.GetScale());
  return kKineticMaxSpeedStart * lerpCoef + kKineticMaxSpeedEnd * (1.0 - lerpCoef);
}

class KineticScrollAnimation : public Animation
{
public:
  // startRect - mercator visible on screen rect in moment when user release fingers.
  // direction - mercator space direction of moving. length(direction) - mercator distance on wich map will be offset.
  KineticScrollAnimation(m2::PointD const & startPos, m2::PointD const & direction, double duration)
    : Animation(true /* couldBeInterrupted */, true /* couldBeBlended */)
    , m_endPos(startPos + direction)
    , m_direction(direction)
    , m_duration(duration)
    , m_elapsedTime(0.0)
  {
    SetInterruptedOnCombine(true);
    m_objects.insert(Animation::Object::MapPlane);
    m_properties.insert(Animation::ObjectProperty::Position);
  }

  Animation::Type GetType() const override { return Animation::Type::KineticScroll; }

  TAnimObjects const & GetObjects() const override
  {
    return m_objects;
  }

  bool HasObject(Object object) const override
  {
    return m_objects.find(object) != m_objects.end();
  }

  TObjectProperties const & GetProperties(Object object) const override
  {
    ASSERT(HasObject(object), ());
    return m_properties;
  }

  bool HasProperty(Object object, ObjectProperty property) const override
  {
    return HasObject(object) && m_properties.find(property) != m_properties.end();
  }

  void SetMaxDuration(double maxDuration) override
  {
    if (m_duration > maxDuration)
      m_duration = maxDuration;
  }

  void SetMinDuration(double /* minDuration */) override {}
  double GetMaxDuration() const override { return Animation::kInvalidAnimationDuration; }
  double GetMinDuration() const override { return Animation::kInvalidAnimationDuration; }

  double GetDuration() const override { return m_duration; }
  bool IsFinished() const override { return m_elapsedTime >= m_duration; }

  void Advance(double elapsedSeconds) override
  {
    m_elapsedTime += elapsedSeconds;
  }

  void Finish() override
  {
    m_elapsedTime = m_duration;
    Animation::Finish();
  }

  bool GetProperty(Object object, ObjectProperty property, PropertyValue & value) const override
  {
    ASSERT(HasProperty(object, property), ());
    // Current position = target position - amplutide * e ^ (elapsed / duration).
    // We calculate current position not based on start position, but based on target position.
    value = PropertyValue(m_endPos - m_direction * exp(-kKineticFadeoff * GetT()));
    return true;
  }

  bool GetTargetProperty(Object object, ObjectProperty property, PropertyValue & value) const override
  {
    ASSERT(HasProperty(object, property), ());
    value = PropertyValue(m_endPos);
    return true;
  }

private:
  double GetT() const
  {
    return IsFinished() ? 1.0 : m_elapsedTime / m_duration;
  }

  m2::PointD m_endPos;
  m2::PointD m_direction;
  double m_duration;
  double m_elapsedTime;
  TAnimObjects m_objects;
  TObjectProperties m_properties;
};

void KineticScroller::Init(ScreenBase const & modelView)
{
  ASSERT(!m_isActive, ());
  m_isActive = true;

  m_points.clear();
  m_points.emplace_back(modelView.GlobalRect().Center(), ClockT::now());
}

void KineticScroller::Update(ScreenBase const & modelView)
{
  ASSERT(m_isActive, ());

  auto const nowTime = ClockT::now();
  if (m_points.size() > 1)
  {
    auto it = std::find_if(m_points.begin(), m_points.end(), [&nowTime](auto const & e)
    {
      return GetDurationSeconds(nowTime, e.second) <= kTimeWindowSec;
    });

    // Keep last point always.
    if (it == m_points.end())
      --it;
    m_points.erase(m_points.begin(), it);
  }

  m_points.emplace_back(modelView.GlobalRect().Center(), nowTime);
}

bool KineticScroller::IsActive() const
{
  return m_isActive;
}

// Calculate direction in mercator space, and velocity in pixel space.
// We need the same reaction on different zoom levels, and should calculate velocity on pixel space.
std::pair<m2::PointD, double> KineticScroller::GetDirectionAndVelocity(ScreenBase const & modelView) const
{
  ASSERT(m_isActive, ());
  ASSERT(!m_points.empty(), ());

  // Or take m_points.back() ?
  m2::PointD const currentCenter = modelView.GlobalRect().GlobalCenter();

  double const lengthPixel = (modelView.GtoP(currentCenter) - modelView.GtoP(m_points.front().first)).Length();
  double const elapsedSec = std::max(kMinDragTimeSec, GetDurationSeconds(ClockT::now(), m_points.front().second));
  double const vs = VisualParams::Instance().GetVisualScale();

  // Most touch filtrations happen here.
  double const velocity = lengthPixel / elapsedSec;
  if (velocity < kInstantVelocityThreshold * vs)
    return {{}, 0};

  m2::PointD const delta = currentCenter - m_points.front().first;
  if (delta.IsAlmostZero())
    return {{}, 0};

  return {delta.Normalize(), std::min(velocity, CalculateKineticMaxSpeed(modelView) * vs)};
}

void KineticScroller::Cancel()
{
  m_isActive = false;
}

drape_ptr<Animation> KineticScroller::CreateKineticAnimation(ScreenBase const & modelView)
{
  auto const [dir, velocity] = GetDirectionAndVelocity(modelView);
  // Cancel current animation in any case.
  Cancel();
  if (velocity < 1E-6)
    return {};

  // Before we start animation we have to convert velocity vector from pixel space to mercator space.
  m2::PointD const center = modelView.GlobalRect().GlobalCenter();
  double const offset = (modelView.PtoG(modelView.GtoP(center) + dir * velocity) - center).Length();
  double const glbLength = kKineticAcceleration * offset;
  m2::PointD const glbDirection = dir * glbLength;
  m2::PointD const targetCenter = center + glbDirection;
  if (!df::GetWorldRect().IsPointInside(targetCenter))
    return {};

  return make_unique_dp<KineticScrollAnimation>(center, glbDirection, kKineticDuration);
}
}  // namespace df
