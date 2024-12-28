#pragma once

#include "geometry/point2d.hpp"

#include <memory>
#include <vector>

namespace m2
{
class Spline
{
public:
  class iterator
  {
  public:
    PointD m_pos;
    PointD m_dir;
    PointD m_avrDir;

    iterator();

    void Attach(Spline const & spl);
    void Advance(double step);
    bool BeginAgain() const;
    double GetFullLength() const;

    size_t GetIndex() const;
    bool IsAttached() const;

  private:
    friend class Spline;
    double GetDistance() const;

    void AdvanceForward(double step);
    void AdvanceBackward(double step);

  private:
    bool m_checker;
    Spline const * m_spl;
    size_t m_index;
    double m_dist;
  };

public:
  Spline() = default;
  explicit Spline(size_t reservedSize);
  explicit Spline(std::vector<PointD> const & path);
  explicit Spline(std::vector<PointD> && path);

  void AddPoint(PointD const & pt);
  void ReplacePoint(PointD const & pt);
  bool IsProlonging(PointD const & pt) const;

  size_t GetSize() const;
  std::vector<PointD> const & GetPath() const { return m_position; }
  void Clear();

  iterator GetPoint(double step) const;

  template <typename TFunctor>
  void ForEachNode(iterator const & begin, iterator const & end, TFunctor const & f) const
  {
    ASSERT(begin.BeginAgain() == false, ());
    ASSERT(end.BeginAgain() == false, ());

    f(begin.m_pos);

    for (size_t i = begin.GetIndex() + 1; i <= end.GetIndex(); ++i)
      f(m_position[i]);

    f(end.m_pos);
  }

  bool IsEmpty() const;
  bool IsValid() const;

  double GetLength() const;
  double GetLastLength() const;
  /// @return for (i) -> (i + 1) section.
  std::pair<PointD, double> GetTangentAndLength(size_t i) const;

protected:
  void InitDirections();

  std::vector<PointD> m_position;
  std::vector<PointD> m_direction;
  std::vector<double> m_length;
};

class SplineEx : public Spline
{
public:
  explicit SplineEx(size_t reservedSize = 2);

  std::vector<double> const & GetLengths() const { return m_length; }
  std::vector<PointD> const & GetDirections() const { return m_direction; }
};

class SharedSpline
{
public:
  SharedSpline() = default;
  explicit SharedSpline(std::vector<PointD> const & path);
  explicit SharedSpline(std::vector<PointD> && path);

  bool IsNull() const;
  void Reset(Spline * spline);
  //void Reset(std::vector<PointD> const & path);

  Spline::iterator CreateIterator() const;

  Spline * operator->();
  Spline const * operator->() const;

  Spline const * Get() const;

private:
  std::shared_ptr<Spline> m_spline;
};
}  // namespace m2
