#include "drape_frontend/animation/interpolators.hpp"
#include "drape_frontend/animation/interpolations.hpp"

#include "base/assert.hpp"

#include <algorithm>

namespace df
{

double CalcAnimSpeedDuration(double pxDiff, double pxSpeed)
{
  double constexpr kEps = 1e-5;

  if (AlmostEqualAbs(pxDiff, 0.0, kEps))
    return 0.0;

  return fabs(pxDiff) / pxSpeed;
}

Interpolator::Interpolator(double duration, double delay)
  : m_elapsedTime(0.0)
  , m_duration(duration)
  , m_maxDuration(Interpolator::kInvalidDuration)
  , m_minDuration(Interpolator::kInvalidDuration)
  , m_delay(delay)
  , m_isActive(false)
{
  ASSERT_GREATER_OR_EQUAL(m_duration, 0.0, ());
}

bool Interpolator::IsFinished() const
{
  if (!IsActive())
    return true;

  return m_elapsedTime > (m_duration + m_delay);
}

void Interpolator::Advance(double elapsedSeconds)
{
  m_elapsedTime += elapsedSeconds;
}

void Interpolator::Finish()
{
  m_elapsedTime = m_duration + m_delay + 1.0;
}

bool Interpolator::IsActive() const
{
  return m_isActive;
}

void Interpolator::SetActive(bool active)
{
  m_isActive = active;
}

void Interpolator::SetMaxDuration(double maxDuration)
{
  m_maxDuration = maxDuration;
  if (m_maxDuration >= 0.0)
    m_duration = std::min(m_duration, m_maxDuration);
}

void Interpolator::SetMinDuration(double minDuration)
{
  m_minDuration = minDuration;
  if (m_minDuration >= 0.0)
    m_duration = std::max(m_duration, m_minDuration);
}

double Interpolator::GetMaxDuration() const
{
  return m_maxDuration;
}

double Interpolator::GetMinDuration() const
{
  return m_minDuration;
}

double Interpolator::GetT() const
{
  if (IsFinished())
    return 1.0;

  return std::max(m_elapsedTime - m_delay, 0.0) / m_duration;
}

double Interpolator::GetElapsedTime() const
{
  return m_elapsedTime;
}

double Interpolator::GetDuration() const
{
  return m_duration;
}

PositionInterpolator::PositionInterpolator()
  : PositionInterpolator(0.0 /* duration */, 0.0 /* delay */, m2::PointD(), m2::PointD())
{}

PositionInterpolator::PositionInterpolator(double duration, double delay,
                                           m2::PointD const & startPosition, m2::PointD const & endPosition)
  : Interpolator(duration, delay)
  , m_startPosition(startPosition)
  , m_endPosition(endPosition)
  , m_position(startPosition)
{
  SetActive((GetDuration() > 0.0) && (m_startPosition != m_endPosition));
}

PositionInterpolator::PositionInterpolator(m2::PointD const & startPosition, m2::PointD const & endPosition,
                                           ScreenBase const & convertor)
  : PositionInterpolator(0.0 /* delay */, startPosition, endPosition, convertor)
{}

PositionInterpolator::PositionInterpolator(double delay,
                                           m2::PointD const & startPosition, m2::PointD const & endPosition,
                                           ScreenBase const & convertor)
  : Interpolator(PositionInterpolator::GetMoveDuration(startPosition, endPosition, convertor), delay)
  , m_startPosition(startPosition)
  , m_endPosition(endPosition)
  , m_position(startPosition)
{
  SetActive((GetDuration() > 0.0) && (m_startPosition != m_endPosition));
}

PositionInterpolator::PositionInterpolator(m2::PointD const & startPosition, m2::PointD const & endPosition,
                                           m2::RectD const & viewportRect, double scale)
  : PositionInterpolator(0.0 /* delay */, startPosition, endPosition, viewportRect, scale)
{}

PositionInterpolator::PositionInterpolator(double delay,
                                           m2::PointD const & startPosition, m2::PointD const & endPosition,
                                           m2::RectD const & viewportRect, double scale)
  : Interpolator(PositionInterpolator::GetMoveDuration(startPosition, endPosition, viewportRect, scale), delay)
  , m_startPosition(startPosition)
  , m_endPosition(endPosition)
  , m_position(startPosition)
{
  SetActive((GetDuration() > 0.0) && (m_startPosition != m_endPosition));
}

//static
double PositionInterpolator::GetMoveDuration(double globalDistance, m2::RectD const & viewportRect, double scale)
{
  double constexpr kMinMoveDuration = 0.2;
  double constexpr kMinSpeedScalar = 0.2;
  double constexpr kMaxSpeedScalar = 7.0;
  double constexpr kEps = 1e-5;

  ASSERT_GREATER(scale, 0.0, ());
  double const pixelLength = globalDistance / scale;
  if (pixelLength < kEps)
    return 0.0;

  double const minSize = std::min(viewportRect.SizeX(), viewportRect.SizeY());
  if (pixelLength < kMinSpeedScalar * minSize)
    return kMinMoveDuration;

  double const pixelSpeed = kMaxSpeedScalar * minSize;
  return CalcAnimSpeedDuration(pixelLength, pixelSpeed);
}

//static
double PositionInterpolator::GetMoveDuration(m2::PointD const & startPosition, m2::PointD const & endPosition,
                              m2::RectD const & viewportRect, double scale)
{
  return GetMoveDuration(endPosition.Length(startPosition), viewportRect, scale);
}

//static
double PositionInterpolator::GetMoveDuration(m2::PointD const & startPosition,
                                             m2::PointD const & endPosition,
                                             ScreenBase const & convertor)
{
  return GetMoveDuration(startPosition, endPosition, convertor.PixelRectIn3d(), convertor.GetScale());
}


void PositionInterpolator::Advance(double elapsedSeconds)
{
  TBase::Advance(elapsedSeconds);
  m_position = InterpolatePoint(m_startPosition, m_endPosition, GetT());
}

void PositionInterpolator::Finish()
{
  TBase::Finish();
  m_position = m_endPosition;
}

ScaleInterpolator::ScaleInterpolator()
  : ScaleInterpolator(1.0 /* startScale */, 1.0 /* endScale */, false /* isAutoZoom */)
{}

ScaleInterpolator::ScaleInterpolator(double startScale, double endScale, bool isAutoZoom)
  : ScaleInterpolator(0.0 /* delay */, startScale, endScale, isAutoZoom)
{}

ScaleInterpolator::ScaleInterpolator(double delay, double startScale, double endScale, bool isAutoZoom)
  : Interpolator(ScaleInterpolator::GetScaleDuration(startScale, endScale, isAutoZoom), delay)
  , m_startScale(startScale)
  , m_endScale(endScale)
  , m_scale(startScale)
{
  SetActive((GetDuration() > 0.0) && (m_startScale != m_endScale));
}

// static
double ScaleInterpolator::GetScaleDuration(double startScale, double endScale, bool isAutoZoom)
{
  // Resize 2.0 times should be done for 1.2 seconds in autozoom or for 0.2 seconds in usual case.
  double const kPixelSpeed = isAutoZoom ? (2.0 / 1.2) : (2.0 / 0.2);

  if (startScale > endScale)
    std::swap(startScale, endScale);

  return CalcAnimSpeedDuration(endScale / startScale, kPixelSpeed);
}

void ScaleInterpolator::Advance(double elapsedSeconds)
{
  TBase::Advance(elapsedSeconds);
  m_scale = InterpolateDouble(m_startScale, m_endScale, GetT());
}

void ScaleInterpolator::Finish()
{
  TBase::Finish();
  m_scale = m_endScale;
}

AngleInterpolator::AngleInterpolator()
  : AngleInterpolator(0.0 /* startAngle */, 0.0 /* endAngle */)
{}

AngleInterpolator::AngleInterpolator(double startAngle, double endAngle)
  : AngleInterpolator(0.0 /* delay */, startAngle, endAngle)
{}

AngleInterpolator::AngleInterpolator(double delay, double startAngle, double endAngle)
  : Interpolator(AngleInterpolator::GetRotateDuration(startAngle, endAngle), delay)
  , m_startAngle(ang::AngleIn2PI(startAngle))
  , m_endAngle(ang::AngleIn2PI(endAngle))
  , m_angle(m_startAngle)
{
  SetActive((GetDuration() > 0.0) && (m_startAngle != m_endAngle));
}

AngleInterpolator::AngleInterpolator(double delay, double duration, double startAngle, double endAngle)
  : Interpolator(duration, delay)
  , m_startAngle(ang::AngleIn2PI(startAngle))
  , m_endAngle(ang::AngleIn2PI(endAngle))
  , m_angle(m_startAngle)
{
  SetActive((GetDuration() > 0.0) && (m_startAngle != m_endAngle));
}

// static
double AngleInterpolator::GetRotateDuration(double startAngle, double endAngle)
{
  double constexpr kRotateDurationScalar = 0.75;
  startAngle = ang::AngleIn2PI(startAngle);
  endAngle = ang::AngleIn2PI(endAngle);
  return kRotateDurationScalar * fabs(ang::GetShortestDistance(startAngle, endAngle)) / math::pi;
}

void AngleInterpolator::Advance(double elapsedSeconds)
{
  TBase::Advance(elapsedSeconds);
  m_angle = m_startAngle + ang::GetShortestDistance(m_startAngle, m_endAngle) * GetT();
}

void AngleInterpolator::Finish()
{
  TBase::Finish();
  m_angle = m_endAngle;
}

} // namespace df
