#include "drape_frontend/gui/ruler_helper.hpp"
#include "drape_frontend/gui/drape_gui.hpp"
#include "drape_frontend/visual_params.hpp"

#include "platform/measurement_utils.hpp"

#include "geometry/mercator.hpp"
#include "geometry/screenbase.hpp"

#include "base/macros.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <set>

namespace gui
{
namespace
{
float constexpr kMinPixelWidth = 60.f;
int constexpr kMinMetersWidth = 5;
int constexpr kMaxMetersWidth = 1000000;

int constexpr kMinUnitValue = -1;
int constexpr kMaxUnitValue = std::numeric_limits<int>::max() - 1;
int constexpr kInvalidUnitValue = kMaxUnitValue + 1;

int constexpr kVisibleRulerBottomScale = 5;

struct UnitValue
{
  std::string m_s;
  int m_i;
};

UnitValue g_arrFeet[] = {
  { "10 ft", 10 },
  { "20 ft", 20 },
  { "50 ft", 50 },
  { "100 ft", 100 },
  { "200 ft", 200 },
  { "0.1 mi", 528 },
  { "0.2 mi", 528 * 2 },
  { "0.5 mi", 528 * 5 },
  { "1 mi", 5280 },
  { "2 mi", 2 * 5280 },
  { "5 mi", 5 * 5280 },
  { "10 mi", 10 * 5280 },
  { "20 mi", 20 * 5280 },
  { "50 mi", 50 * 5280 },
  { "100 mi", 100 * 5280 },
  { "200 mi", 200 * 5280 },
  { "500 mi", 500 * 5280 }
};

UnitValue g_arrYards[] = {
  { "50 yd", 50 },
  { "100 yd", 100 },
  { "200 yd", 200 },
  { "500 yd", 500 },
  { "0.5 mi", 1760 / 2 },
  { "1 mi", 1760 },
  { "2 mi", 2 * 1760 },
  { "5 mi", 5 * 1760 },
  { "10 mi", 10 * 1760 },
  { "20 mi", 20 * 1760 },
  { "50 mi", 50 * 1760 },
  { "100 mi", 100 * 1760 },
  { "200 mi", 200 * 1760 },
  { "500 mi", 500 * 1760 }
};

// TODO: fix ruler text to the current zoom level, i.e. make it 100m for z16 always
// (ruler length will vary still). It'll make debugging and user support easier as
// we'll be able to tell zoom level of a screenshot by looking at the ruler.
UnitValue g_arrMeters[] = {
  { "1 m", 1 },
  { "2 m", 2 },
  { "5 m", 5 },
  { "10 m", 10 },
  { "20 m", 20 },
  { "50 m", 50 },
  { "100 m", 100 },
  { "200 m", 200 },
  { "500 m", 500 },
  { "1 km", 1000 },
  { "2 km", 2000 },
  { "5 km", 5000 },
  { "10 km", 10000 },
  { "20 km", 20000 },
  { "50 km", 50000 },
  { "100 km", 100000 },
  { "200 km", 200000 },
  { "500 km", 500000 },
  { "1000 km", 1000000 }
};

double Identity(double val)
{
  return val;
}
}  // namespace

RulerHelper::RulerHelper()
  : m_pixelLength(0.0f)
  , m_rangeIndex(kInvalidUnitValue)
  , m_isTextDirty(false)
  , m_dirtyTextRequested(false)
{}

void RulerHelper::Update(ScreenBase const & screen)
{
  m2::PointD pivot = screen.PixelRect().Center();
  int const minPxWidth = base::SignedRound(kMinPixelWidth * df::VisualParams::Instance().GetVisualScale());
  m2::PointD pt1 = screen.PtoG(pivot);
  m2::PointD pt0 = screen.PtoG(pivot - m2::PointD(minPxWidth, 0));

  double const distanceInMeters = mercator::DistanceOnEarth(pt0, pt1);

  // convert metres to units for calculating m_metresDiff.
  double metersDiff = CalcMetersDiff(distanceInMeters);

  bool const higherThanMax = metersDiff > kMaxMetersWidth;
  ASSERT_GREATER_OR_EQUAL(metersDiff, kMinMetersWidth, ());
  m_pixelLength = static_cast<float>(minPxWidth);

  if (higherThanMax)
    m_pixelLength = static_cast<float>(minPxWidth) * 3.0f / 2.0f;
  else
  {
    double const a = ang::AngleTo(pt1, pt0);
    pt0 = mercator::GetSmPoint(pt1, cos(a) * metersDiff, sin(a) * metersDiff);

    m_pixelLength = base::SignedRound(pivot.Length(screen.GtoP(pt0)));
  }

  int drawScale = df::GetDrawTileScale(screen);
  if (m_currentDrawScale < kVisibleRulerBottomScale &&
      drawScale >= kVisibleRulerBottomScale)
  {
    SetTextDirty();
  }

  m_currentDrawScale = drawScale;
}

// static
bool RulerHelper::IsVisible(ScreenBase const & screen)
{
  DrapeGui & gui = DrapeGui::Instance();
  return !gui.IsCopyrightActive() && df::GetDrawTileScale(screen) >= kVisibleRulerBottomScale;
}

void RulerHelper::Invalidate()
{
  SetTextDirty();
}

// static
float RulerHelper::GetRulerHalfHeight()
{
  float constexpr kRulerHalfHeight = 1.0f;
  return kRulerHalfHeight * static_cast<float>(df::VisualParams::Instance().GetVisualScale());
}

float RulerHelper::GetRulerPixelLength() const
{
  return m_pixelLength;
}

// static
float RulerHelper::GetMaxRulerPixelLength()
{
  return kMinPixelWidth * 3.0f / 2.0f;
}

// static
int RulerHelper::GetVerticalTextOffset()
{
  return static_cast<int>(-5 * df::VisualParams::Instance().GetVisualScale());
}

bool RulerHelper::IsTextDirty() const
{
  return m_isTextDirty;
}

std::string const & RulerHelper::GetRulerText() const
{
  m_dirtyTextRequested = true;
  return m_rulerText;
}

void RulerHelper::ResetTextDirtyFlag()
{
  if (m_dirtyTextRequested)
    m_isTextDirty = false;
}

// static
void RulerHelper::GetTextInitInfo(std::string & alphabet, uint32_t & size)
{
  std::set<char> symbols;
  size_t result = 0;
  auto const functor = [&result, &symbols](UnitValue const & v)
  {
    size_t stringSize = v.m_s.length();
    result = std::max(result, stringSize);
    for (size_t i = 0; i < stringSize; ++i)
      symbols.insert(v.m_s[i]);
  };

  std::for_each(std::begin(g_arrFeet), std::end(g_arrFeet), functor);
  std::for_each(std::begin(g_arrMeters), std::end(g_arrMeters), functor);
  std::for_each(std::begin(g_arrYards), std::end(g_arrYards), functor);

  std::for_each(begin(symbols), end(symbols), [&alphabet](char c)
  {
    alphabet.push_back(c);
  });
  alphabet.append("<>");

  size = static_cast<uint32_t>(result) + 2; // add 2 char for symbols "< " and "> ".
}

double RulerHelper::CalcMetersDiff(double value)
{
  UnitValue const * arrU = g_arrMeters;
  int count = ARRAY_SIZE(g_arrMeters);

  auto conversionFn = &Identity;

  using namespace measurement_utils;
  if (GetMeasurementUnits() == Units::Imperial)
  {
    arrU = g_arrFeet;
    count = ARRAY_SIZE(g_arrFeet);
    conversionFn = &MetersToFeet;
  }

  int prevUnitRange = m_rangeIndex;
  double result = 0.0;
  double v = conversionFn(value);
  if (arrU[0].m_i > v)
  {
    m_rangeIndex = kMinUnitValue;
    // TODO: "< X" ruler text seems to be never used.
    m_rulerText = std::string("< ") + arrU[0].m_s;
    result = kMinMetersWidth - 1.0;
  }
  else if (arrU[count-1].m_i <= v)
  {
    m_rangeIndex = kMaxUnitValue;
    m_rulerText = std::string("> ") + arrU[count-1].m_s;
    result = kMaxMetersWidth + 1.0;
  }
  else
  {
    for (int i = 0; i < count; ++i)
    {
      if (arrU[i].m_i > v)
      {
        m_rangeIndex = i;
        result = arrU[i].m_i / conversionFn(1.0);
        m_rulerText = arrU[i].m_s;
        break;
      }
    }
  }

  if (m_rangeIndex != prevUnitRange)
    SetTextDirty();

  return result;
}

void RulerHelper::SetTextDirty()
{
  m_dirtyTextRequested = false;
  m_isTextDirty = true;
}

void RulerHelper::LocalizeTexts()
{
  std::string decSep = platform::GetCurrentLocale().m_decimalSeparator;

  if (decSep == ".")
    return;

  for (auto & [m_s, m_i] : g_arrFeet)
  {
    size_t pos;

    if ((pos = m_s.find(".")) != std::string::npos)
      m_s.replace(pos, 1, decSep);
  }

  for (auto & [m_s, m_i] : g_arrYards)
  {
    size_t pos;

    if ((pos = m_s.find(".")) != std::string::npos)
      m_s.replace(pos, 1, decSep);
  }
}
}  // namespace gui
