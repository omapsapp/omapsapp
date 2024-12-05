#pragma once

#include "geometry/point2d.hpp"

#include "base/assert.hpp"
#include "base/internal/message.hpp"

#include <algorithm>
#include <limits>
#include <sstream>
#include <string>

namespace m2
{
namespace impl
{
template <typename T, bool has_sign>
struct min_max_value;

template <typename T>
struct min_max_value<T, true>
{
  T get_min() { return -get_max(); }
  T get_max() { return std::numeric_limits<T>::max(); }
};

template <typename T>
struct min_max_value<T, false>
{
  T get_min() { return std::numeric_limits<T>::min(); }
  T get_max() { return std::numeric_limits<T>::max(); }
};
}  // namespace impl

template <typename T>
class Rect
{
public:
  using value_type = T;

  Rect() { MakeEmpty(); }

  constexpr Rect(T minX, T minY, T maxX, T maxY) : m_minX(minX), m_minY(minY), m_maxX(maxX), m_maxY(maxY)
  {
    ASSERT(minX <= maxX && minY <= maxY, (minX, maxX, minY, maxY));
  }

  Rect(Point<T> const & p1, Point<T> const & p2)
    : m_minX(std::min(p1.x, p2.x))
    , m_minY(std::min(p1.y, p2.y))
    , m_maxX(std::max(p1.x, p2.x))
    , m_maxY(std::max(p1.y, p2.y))
  {
  }

  template <typename U>
  explicit Rect(Rect<U> const & src)
    : m_minX(src.minX()), m_minY(src.minY()), m_maxX(src.maxX()), m_maxY(src.maxY())
  {
  }

  static Rect GetEmptyRect() { return Rect(); }

  static Rect GetInfiniteRect()
  {
    Rect r;
    r.MakeInfinite();
    return r;
  }

  void MakeEmpty()
  {
    m_minX = m_minY = impl::min_max_value<T, IsSigned>().get_max();
    m_maxX = m_maxY = impl::min_max_value<T, IsSigned>().get_min();
  }

  void MakeInfinite()
  {
    m_minX = m_minY = impl::min_max_value<T, IsSigned>().get_min();
    m_maxX = m_maxY = impl::min_max_value<T, IsSigned>().get_max();
  }

  bool IsValid() const { return (m_minX <= m_maxX && m_minY <= m_maxY); }

  bool IsEmptyInterior() const { return m_minX >= m_maxX || m_minY >= m_maxY; }

  void Add(m2::Point<T> const & p)
  {
    m_minX = std::min(p.x, m_minX);
    m_minY = std::min(p.y, m_minY);
    m_maxX = std::max(p.x, m_maxX);
    m_maxY = std::max(p.y, m_maxY);
  }

  void Add(m2::Rect<T> const & r)
  {
    m_minX = std::min(r.m_minX, m_minX);
    m_minY = std::min(r.m_minY, m_minY);
    m_maxX = std::max(r.m_maxX, m_maxX);
    m_maxY = std::max(r.m_maxY, m_maxY);
  }

  void Offset(m2::Point<T> const & p)
  {
    m_minX += p.x;
    m_minY += p.y;
    m_maxX += p.x;
    m_maxY += p.y;
  }

  void Offset(T const & dx, T const & dy)
  {
    m_minX += dx;
    m_minY += dy;
    m_maxX += dx;
    m_maxY += dy;
  }

  Point<T> LeftTop() const { return Point<T>(m_minX, m_maxY); }
  Point<T> RightTop() const { return Point<T>(m_maxX, m_maxY); }
  Point<T> RightBottom() const { return Point<T>(m_maxX, m_minY); }
  Point<T> LeftBottom() const { return Point<T>(m_minX, m_minY); }

  template <typename Fn>
  void ForEachCorner(Fn && fn) const
  {
    fn(LeftTop());
    fn(LeftBottom());
    fn(RightBottom());
    fn(RightTop());
  }

  template <typename Fn>
  void ForEachSide(Fn && fn) const
  {
    fn(LeftTop(), LeftBottom());
    fn(LeftBottom(), RightBottom());
    fn(RightBottom(), RightTop());
    fn(RightTop(), LeftTop());
  }

  bool IsIntersect(Rect const & r) const
  {
    return !((m_maxX < r.m_minX) || (m_minX > r.m_maxX) || (m_maxY < r.m_minY) ||
             (m_minY > r.m_maxY));
  }

  bool IsPointInside(Point<T> const & pt) const
  {
    return !(m_minX > pt.x || pt.x > m_maxX || m_minY > pt.y || pt.y > m_maxY);
  }

  bool IsRectInside(Rect<T> const & rect) const
  {
    return (IsPointInside(Point<T>(rect.minX(), rect.minY())) &&
            IsPointInside(Point<T>(rect.maxX(), rect.maxY())));
  }

  Point<T> Center() const { return Point<T>((m_minX + m_maxX) / 2.0, (m_minY + m_maxY) / 2.0); }
  T SizeX() const { return std::max(static_cast<T>(0), m_maxX - m_minX); }
  T SizeY() const { return std::max(static_cast<T>(0), m_maxY - m_minY); }
  T Area() const { return SizeX() * SizeY(); }

  void DivideByGreaterSize(Rect & r1, Rect & r2) const
  {
    if (SizeX() > SizeY())
    {
      T const pivot = (m_minX + m_maxX) / 2;
      r1 = Rect<T>(m_minX, m_minY, pivot, m_maxY);
      r2 = Rect<T>(pivot, m_minY, m_maxX, m_maxY);
    }
    else
    {
      T const pivot = (m_minY + m_maxY) / 2;
      r1 = Rect<T>(m_minX, m_minY, m_maxX, pivot);
      r2 = Rect<T>(m_minX, pivot, m_maxX, m_maxY);
    }
  }

  void SetSizes(T dx, T dy)
  {
    ASSERT_GREATER(dx, 0, ());
    ASSERT_GREATER(dy, 0, ());

    dx /= 2;
    dy /= 2;

    Point<T> const c = Center();
    m_minX = c.x - dx;
    m_minY = c.y - dy;
    m_maxX = c.x + dx;
    m_maxY = c.y + dy;
  }

  void SetSizesToIncludePoint(Point<T> const & pt)
  {
    Point<T> const c = Center();
    T const dx = math::Abs(pt.x - c.x);
    T const dy = math::Abs(pt.y - c.y);

    m_minX = c.x - dx;
    m_minY = c.y - dy;
    m_maxX = c.x + dx;
    m_maxY = c.y + dy;
  }

  void SetCenter(m2::Point<T> const & p) { Offset(p - Center()); }

  T minX() const { return m_minX; }
  T minY() const { return m_minY; }
  T maxX() const { return m_maxX; }
  T maxY() const { return m_maxY; }

  void setMinX(T minX) { m_minX = minX; }
  void setMinY(T minY) { m_minY = minY; }
  void setMaxX(T maxX) { m_maxX = maxX; }
  void setMaxY(T maxY) { m_maxY = maxY; }

  void Scale(T scale)
  {
    ASSERT_GREATER(scale, 0, ());
    scale *= 0.5;

    m2::Point<T> const center = Center();
    T const halfSizeX = SizeX() * scale;
    T const halfSizeY = SizeY() * scale;
    m_minX = center.x - halfSizeX;
    m_minY = center.y - halfSizeY;
    m_maxX = center.x + halfSizeX;
    m_maxY = center.y + halfSizeY;
  }

  void Inflate(T dx, T dy)
  {
    m_minX -= dx;
    m_maxX += dx;
    m_minY -= dy;
    m_maxY += dy;
  }

  bool Intersect(m2::Rect<T> const & r)
  {
    T newMinX = std::max(m_minX, r.minX());
    T newMaxX = std::min(m_maxX, r.maxX());

    if (newMinX > newMaxX)
      return false;

    T newMinY = std::max(m_minY, r.minY());
    T newMaxY = std::min(m_maxY, r.maxY());

    if (newMinY > newMaxY)
      return false;

    m_minX = newMinX;
    m_minY = newMinY;
    m_maxX = newMaxX;
    m_maxY = newMaxY;

    return true;
  }

  bool operator==(m2::Rect<T> const & r) const
  {
    return m_minX == r.m_minX && m_minY == r.m_minY && m_maxX == r.m_maxX && m_maxY == r.m_maxY;
  }

  bool operator!=(m2::Rect<T> const & r) const { return !(*this == r); }

private:
  enum
  {
    IsSigned = std::numeric_limits<T>::is_signed
  };

  template <class TArchive, class TPoint>
  friend TArchive & operator<<(TArchive & ar, Rect<TPoint> const & rect);

  template <class TArchive, class TPoint>
  friend TArchive & operator>>(TArchive & ar, Rect<TPoint> & rect);

  T m_minX, m_minY, m_maxX, m_maxY;
};

using RectF = Rect<float>;
using RectD = Rect<double>;
using RectU = Rect<unsigned>;
using RectU32 = Rect<uint32_t>;
using RectI = Rect<int>;

template <typename T>
bool IsEqual(Rect<T> const & r1, Rect<T> const & r2, double epsX, double epsY)
{
  Rect<T> r = r1;
  r.Inflate(epsX, epsY);
  if (!r.IsRectInside(r2))
    return false;

  r = r2;
  r.Inflate(epsX, epsY);
  if (!r.IsRectInside(r1))
    return false;

  return true;
}

template <typename T>
bool IsEqualSize(Rect<T> const & r1, Rect<T> const & r2, double epsX, double epsY)
{
  return fabs(r1.SizeX() - r2.SizeX()) < epsX && fabs(r1.SizeY() - r2.SizeY()) < epsY;
}

template <typename T>
m2::Rect<T> const Add(m2::Rect<T> const & r, m2::Point<T> const & p)
{
  return m2::Rect<T>(std::min(p.x, r.minX()), std::min(p.y, r.minY()), std::max(p.x, r.maxX()),
                     std::max(p.y, r.maxY()));
}

template <typename T>
m2::Rect<T> const Add(m2::Rect<T> const & r1, m2::Rect<T> const & r2)
{
  return m2::Rect<T>(std::min(r2.minX(), r1.minX()), std::min(r2.minY(), r1.minY()),
                     std::max(r2.maxX(), r1.maxX()), std::max(r2.maxY(), r1.maxY()));
}

template <typename T>
m2::Rect<T> const Offset(m2::Rect<T> const & r1, m2::Point<T> const & p)
{
  return m2::Rect<T>(r1.minX() + p.x, r1.minY() + p.y, r1.maxX() + p.x, r1.maxY() + p.y);
}

template <typename T>
m2::Rect<T> const Inflate(m2::Rect<T> const & r, T const & dx, T const & dy)
{
  return m2::Rect<T>(r.minX() - dx, r.minY() - dy, r.maxX() + dx, r.maxY() + dy);
}

template <typename T>
m2::Rect<T> const Inflate(m2::Rect<T> const & r, m2::Point<T> const & p)
{
  return Inflate(r, p.x, p.y);
}

template <typename T>
m2::Rect<T> const Offset(m2::Rect<T> const & r1, T const & dx, T const & dy)
{
  return m2::Rect<T>(r1.minX() + dx, r1.minY() + dy, r1.maxX() + dx, r1.maxY() + dy);
}

template <typename T, typename TCollection>
bool HasIntersection(m2::Rect<T> const & rect, TCollection const & geometry)
{
  for (auto const & g : geometry)
  {
    if (rect.IsIntersect(g))
      return true;
  }
  return false;
}

template <class TArchive, class PointT>
TArchive & operator>>(TArchive & ar, m2::Rect<PointT> & rect)
{
  ar >> rect.m_minX;
  ar >> rect.m_minY;
  ar >> rect.m_maxX;
  ar >> rect.m_maxY;
  return ar;
}

template <class TArchive, class PointT>
TArchive & operator<<(TArchive & ar, m2::Rect<PointT> const & rect)
{
  ar << rect.m_minX;
  ar << rect.m_minY;
  ar << rect.m_maxX;
  ar << rect.m_maxY;
  return ar;
}

template <typename T>
std::string DebugPrint(m2::Rect<T> const & r)
{
  std::ostringstream out;
  out.precision(20);
  out << "m2::Rect(" << r.minX() << ", " << r.minY() << ", " << r.maxX() << ", " << r.maxY() << ")";
  return out.str();
}
}  // namespace m2
