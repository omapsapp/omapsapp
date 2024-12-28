#include "geometry/spline.hpp"

#include <numeric>

namespace m2
{
Spline::Spline(std::vector<PointD> const & path)
  : m_position(path)
{
  InitDirections();
}

Spline::Spline(std::vector<PointD> && path)
  : m_position(std::move(path))
{
  InitDirections();
}

Spline::Spline(size_t reservedSize)
{
  ASSERT_GREATER(reservedSize, 0, ());
  m_position.reserve(reservedSize);
  m_direction.reserve(reservedSize - 1);
  m_length.reserve(reservedSize - 1);
}

SplineEx::SplineEx(size_t reservedSize)
  : Spline(reservedSize)
{
}

void Spline::AddPoint(PointD const & pt)
{
  if (!IsEmpty())
  {
    /// @todo remove this check when fix generator.
    /// Now we have line objects with zero length segments
    /// https://jira.mail.ru/browse/MAPSME-3561
    PointD const dir = pt - m_position.back();
    if (dir.IsAlmostZero())
      return;

    double const len = dir.Length();
    ASSERT_GREATER(len, 0, ());
    m_length.push_back(len);
    m_direction.push_back(dir / len);
  }

  m_position.push_back(pt);
}

void Spline::ReplacePoint(PointD const & pt)
{
  ASSERT_GREATER(m_position.size(), 1, ());
  ASSERT(!m_length.empty(), ());
  ASSERT(!m_direction.empty(), ());
  m_position.pop_back();
  m_length.pop_back();
  m_direction.pop_back();
  AddPoint(pt);
}

bool Spline::IsProlonging(PointD const & pt) const
{
  size_t const sz = m_position.size();
  if (sz < 2)
    return false;

  PointD dir = pt - m_position.back();
  if (dir.IsAlmostZero())
    return true;
  dir = dir.Normalize();

  ASSERT(!m_direction.empty(), ());
  // Some cos(angle) == 1 (angle == 0) threshold.
  return std::fabs(DotProduct(m_direction.back(), dir)) > 0.995;
}

size_t Spline::GetSize() const
{
  return m_position.size();
}

void Spline::Clear()
{
  m_position.clear();
  m_direction.clear();
  m_length.clear();
}

bool Spline::IsEmpty() const
{
  return m_position.empty();
}

bool Spline::IsValid() const
{
  return m_position.size() > 1;
}

Spline::iterator Spline::GetPoint(double step) const
{
  iterator it;
  it.Attach(*this);
  it.Advance(step);
  return it;
}

double Spline::GetLength() const
{
  return std::accumulate(m_length.begin(), m_length.end(), 0.0);
}

double Spline::GetLastLength() const
{
  ASSERT(!m_length.empty(), ());
  return m_length.back();
}

std::pair<PointD, double> Spline::GetTangentAndLength(size_t i) const
{
  ASSERT_LESS(i, m_length.size(), ());
  return { m_direction[i], m_length[i] };
}

void Spline::InitDirections()
{
  ASSERT_GREATER(m_position.size(), 1, ());
  size_t const sz = m_position.size() - 1;

  ASSERT(m_direction.empty() && m_length.empty(), ());
  m_direction.resize(sz);
  m_length.resize(sz);

  for (size_t i = 0; i < sz; ++i)
  {
    m_direction[i] = m_position[i + 1] - m_position[i];
    double const len = m_direction[i].Length();
    ASSERT_GREATER(len, 0, (i));
    m_length[i] = len;
    m_direction[i] = m_direction[i] / len;
  }
}

Spline::iterator::iterator()
  : m_checker(false)
  , m_spl(NULL)
  , m_index(0)
  , m_dist(0)
{
}

void Spline::iterator::Attach(Spline const & spl)
{
  m_spl = &spl;
  m_index = 0;
  m_dist = 0;
  m_checker = false;
  m_dir = m_spl->m_direction[m_index];
  m_avrDir = m_spl->m_direction[m_index];
  m_pos = m_spl->m_position[m_index] + m_dir * m_dist;
}

bool Spline::iterator::IsAttached() const
{
  return m_spl != nullptr;
}

void Spline::iterator::Advance(double step)
{
  if (step < 0.0)
    AdvanceBackward(step);
  else
    AdvanceForward(step);
}

double Spline::iterator::GetFullLength() const
{
  return m_spl->GetLength();
}

bool Spline::iterator::BeginAgain() const
{
  return m_checker;
}

double Spline::iterator::GetDistance() const
{
  return m_dist;
}

size_t Spline::iterator::GetIndex() const
{
  return m_index;
}

void Spline::iterator::AdvanceBackward(double step)
{
  m_dist += step;
  while (m_dist < 0.0f)
  {
    if (m_index == 0)
    {
      m_checker = true;
      m_pos = m_spl->m_position[m_index];
      m_dir = m_spl->m_direction[m_index];
      m_avrDir = m2::PointD::Zero();
      m_dist = 0.0;
      return;
    }

    --m_index;

    m_dist += m_spl->m_length[m_index];
  }

  m_dir = m_spl->m_direction[m_index];
  m_avrDir = -m_pos;
  m_pos = m_spl->m_position[m_index] + m_dir * m_dist;
  m_avrDir += m_pos;
}

void Spline::iterator::AdvanceForward(double step)
{
  m_dist += step;
  if (m_checker)
  {
    m_pos = m_spl->m_position[m_index] + m_dir * m_dist;
    return;
  }

  while (m_dist > m_spl->m_length[m_index])
  {
    m_dist -= m_spl->m_length[m_index];
    m_index++;
    if (m_index >= m_spl->m_direction.size())
    {
      m_index--;
      m_dist += m_spl->m_length[m_index];
      m_checker = true;
      break;
    }
  }
  m_dir = m_spl->m_direction[m_index];
  m_avrDir = -m_pos;
  m_pos = m_spl->m_position[m_index] + m_dir * m_dist;
  m_avrDir += m_pos;
}

SharedSpline::SharedSpline(std::vector<PointD> const & path)
  : m_spline(std::make_shared<Spline>(path))
{
}

SharedSpline::SharedSpline(std::vector<PointD> && path)
  : m_spline(std::make_shared<Spline>(std::move(path)))
{
}

bool SharedSpline::IsNull() const
{
  return m_spline == nullptr;
}

void SharedSpline::Reset(Spline * spline)
{
  m_spline.reset(spline);
}

//void SharedSpline::Reset(std::vector<PointD> const & path)
//{
//  m_spline.reset(new Spline(path));
//}

Spline::iterator SharedSpline::CreateIterator() const
{
  Spline::iterator result;
  result.Attach(*m_spline.get());
  return result;
}

Spline * SharedSpline::operator->()
{
  ASSERT(!IsNull(), ());
  return m_spline.get();
}

Spline const * SharedSpline::operator->() const
{
  return Get();
}

Spline const * SharedSpline::Get() const
{
  ASSERT(!IsNull(), ());
  return m_spline.get();
}
} // namespace m2
