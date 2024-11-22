#include "coding/geometry_coding.hpp"

#include "coding/point_coding.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"

#include <complex>
#include <stack>

namespace
{
inline m2::PointU ClampPoint(m2::PointD const & maxPoint, m2::PointD const & point)
{
  using uvalue_t = m2::PointU::value_type;
  return { static_cast<uvalue_t>(math::Clamp(point.x, 0.0, maxPoint.x)),
           static_cast<uvalue_t>(math::Clamp(point.y, 0.0, maxPoint.y)) };
}

struct edge_less_p0
{
  using edge_t = tesselator::Edge;

  bool operator()(edge_t const & e1, edge_t const & e2) const
  {
    return (e1.m_p[0] == e2.m_p[0]) ? (e1.m_side < e2.m_side) : (e1.m_p[0] < e2.m_p[0]);
  }
  bool operator()(edge_t const & e1, int e2) const { return e1.m_p[0] < e2; }
  bool operator()(int e1, edge_t const & e2) const { return e1 < e2.m_p[0]; }
};
}  // namespace

namespace coding
{
bool TestDecoding(InPointsT const & points, m2::PointU const & basePoint,
                  m2::PointU const & maxPoint, OutDeltasT const & deltas,
                  void (*fnDecode)(InDeltasT const & deltas, m2::PointU const & basePoint,
                                   m2::PointU const & maxPoint, OutPointsT & points))
{
  size_t const count = points.size();

  std::vector<m2::PointU> decoded;
  decoded.resize(count);

  OutPointsT decodedA(decoded);
  fnDecode(make_read_adapter(deltas), basePoint, maxPoint, decodedA);

  for (size_t i = 0; i < count; ++i)
    ASSERT_EQUAL(points[i], decoded[i], ());
  return true;
}

m2::PointU PredictPointInPolyline(m2::PointD const & maxPoint, m2::PointU const & p1,
                                  m2::PointU const & p2)
{
  return ClampPoint(maxPoint, m2::PointD(p1) + (m2::PointD(p1) - m2::PointD(p2)) / 2.0);
}

uint64_t EncodePointDeltaAsUint(m2::PointU const & actual, m2::PointU const & prediction)
{
  return bits::BitwiseMerge(
      bits::ZigZagEncode(static_cast<int32_t>(actual.x) - static_cast<int32_t>(prediction.x)),
      bits::ZigZagEncode(static_cast<int32_t>(actual.y) - static_cast<int32_t>(prediction.y)));
}

m2::PointU DecodePointDeltaFromUint(uint64_t delta, m2::PointU const & prediction)
{
  uint32_t x, y;
  bits::BitwiseSplit(delta, x, y);
  return m2::PointU(prediction.x + bits::ZigZagDecode(x), prediction.y + bits::ZigZagDecode(y));
}

m2::PointU PredictPointInPolyline(m2::PointD const & maxPoint, m2::PointU const & p1,
                                  m2::PointU const & p2, m2::PointU const & p3)
{
  CHECK_NOT_EQUAL(p2, p3, ());

  using std::complex;
  complex<double> const c1(p1.x, p1.y);
  complex<double> const c2(p2.x, p2.y);
  complex<double> const c3(p3.x, p3.y);
  complex<double> const d = (c1 - c2) / (c2 - c3);
  complex<double> const c0 = c1 + (c1 - c2) * std::polar(0.5, 0.5 * arg(d));

  /*
  complex<double> const c1(p1.x, p1.y);
  complex<double> const c2(p2.x, p2.y);
  complex<double> const c3(p3.x, p3.y);
  complex<double> const d = (c1 - c2) / (c2 - c3);
  complex<double> const c01 = c1 + (c1 - c2) * polar(0.5, arg(d));
  complex<double> const c02 = c1 + (c1 - c2) * complex<double>(0.5, 0.0);
  complex<double> const c0 = (c01 + c02) * complex<double>(0.5, 0.0);
  */

  return ClampPoint(maxPoint, { c0.real(), c0.imag() });
}

m2::PointU PredictPointInTriangle(m2::PointD const & maxPoint, m2::PointU const & p1,
                                  m2::PointU const & p2, m2::PointU const & p3)
{
  // parallelogram prediction
  return ClampPoint(maxPoint, m2::PointD(p1 + p2) - m2::PointD(p3));
}

void EncodePolylinePrev1(InPointsT const & points, m2::PointU const & basePoint,
                         m2::PointU const & maxPoint, OutDeltasT & deltas)
{
  size_t const count = points.size();
  if (count > 0)
  {
    deltas.push_back(EncodePointDeltaAsUint(points[0], basePoint));
    for (size_t i = 1; i < count; ++i)
      deltas.push_back(EncodePointDeltaAsUint(points[i], points[i - 1]));
  }

  ASSERT(TestDecoding(points, basePoint, maxPoint, deltas, &DecodePolylinePrev1), ());
}

void DecodePolylinePrev1(InDeltasT const & deltas, m2::PointU const & basePoint,
                         m2::PointU const & /*maxPoint*/, OutPointsT & points)
{
  size_t const count = deltas.size();
  if (count > 0)
  {
    points.push_back(DecodePointDeltaFromUint(deltas[0], basePoint));
    for (size_t i = 1; i < count; ++i)
      points.push_back(DecodePointDeltaFromUint(deltas[i], points.back()));
  }
}

void EncodePolylinePrev2(InPointsT const & points, m2::PointU const & basePoint,
                         m2::PointU const & maxPoint, OutDeltasT & deltas)
{
  size_t const count = points.size();
  if (count > 0)
  {
    deltas.push_back(EncodePointDeltaAsUint(points[0], basePoint));
    if (count > 1)
    {
      m2::PointD const maxPointD(maxPoint);
      deltas.push_back(EncodePointDeltaAsUint(points[1], points[0]));
      for (size_t i = 2; i < count; ++i)
        deltas.push_back(EncodePointDeltaAsUint(
            points[i], PredictPointInPolyline(maxPointD, points[i - 1], points[i - 2])));
    }
  }

  ASSERT(TestDecoding(points, basePoint, maxPoint, deltas, &DecodePolylinePrev2), ());
}

void DecodePolylinePrev2(InDeltasT const & deltas, m2::PointU const & basePoint,
                         m2::PointU const & maxPoint, OutPointsT & points)
{
  size_t const count = deltas.size();
  if (count > 0)
  {
    points.push_back(DecodePointDeltaFromUint(deltas[0], basePoint));
    if (count > 1)
    {
      m2::PointD const maxPointD(maxPoint);
      points.push_back(DecodePointDeltaFromUint(deltas[1], points.back()));
      for (size_t i = 2; i < count; ++i)
      {
        size_t const n = points.size();
        points.push_back(DecodePointDeltaFromUint(
            deltas[i], PredictPointInPolyline(maxPointD, points[n - 1], points[n - 2])));
      }
    }
  }
}

void EncodePolylinePrev3(InPointsT const & points, m2::PointU const & basePoint,
                         m2::PointU const & maxPoint, OutDeltasT & deltas)
{
  ASSERT_LESS_OR_EQUAL(basePoint.x, maxPoint.x, (basePoint, maxPoint));
  ASSERT_LESS_OR_EQUAL(basePoint.y, maxPoint.y, (basePoint, maxPoint));

  size_t const count = points.size();
  if (count > 0)
  {
    deltas.push_back(EncodePointDeltaAsUint(points[0], basePoint));
    if (count > 1)
    {
      deltas.push_back(EncodePointDeltaAsUint(points[1], points[0]));
      if (count > 2)
      {
        m2::PointD const maxPointD(maxPoint);
        m2::PointU const prediction = PredictPointInPolyline(maxPointD, points[1], points[0]);
        deltas.push_back(EncodePointDeltaAsUint(points[2], prediction));
        for (size_t i = 3; i < count; ++i)
        {
          m2::PointU const prediction = PredictPointInPolyline(
                maxPointD, points[i - 1], points[i - 2], points[i - 3]);
          deltas.push_back(EncodePointDeltaAsUint(points[i], prediction));
        }
      }
    }
  }

  ASSERT(TestDecoding(points, basePoint, maxPoint, deltas, &DecodePolylinePrev3), ());
}

void DecodePolylinePrev3(InDeltasT const & deltas, m2::PointU const & basePoint,
                         m2::PointU const & maxPoint, OutPointsT & points)
{
  ASSERT_LESS_OR_EQUAL(basePoint.x, maxPoint.x, (basePoint, maxPoint));
  ASSERT_LESS_OR_EQUAL(basePoint.y, maxPoint.y, (basePoint, maxPoint));

  size_t const count = deltas.size();
  if (count > 0)
  {
    points.push_back(DecodePointDeltaFromUint(deltas[0], basePoint));
    if (count > 1)
    {
      m2::PointU const pt0 = points.back();
      points.push_back(DecodePointDeltaFromUint(deltas[1], pt0));
      if (count > 2)
      {
        m2::PointD const maxPointD(maxPoint);
        points.push_back(DecodePointDeltaFromUint(
            deltas[2], PredictPointInPolyline(maxPointD, points.back(), pt0)));
        for (size_t i = 3; i < count; ++i)
        {
          size_t const n = points.size();
          m2::PointU const prediction = PredictPointInPolyline(
                maxPointD, points[n - 1], points[n - 2], points[n - 3]);
          points.push_back(DecodePointDeltaFromUint(deltas[i], prediction));
        }
      }
    }
  }
}

void EncodePolyline(InPointsT const & points, m2::PointU const & basePoint,
                    m2::PointU const & maxPoint, OutDeltasT & deltas)
{
  EncodePolylinePrev2(points, basePoint, maxPoint, deltas);
}

void DecodePolyline(InDeltasT const & deltas, m2::PointU const & basePoint,
                    m2::PointU const & maxPoint, OutPointsT & points)
{
  DecodePolylinePrev2(deltas, basePoint, maxPoint, points);
}

void EncodeTriangleStrip(InPointsT const & points, m2::PointU const & basePoint,
                         m2::PointU const & maxPoint, OutDeltasT & deltas)
{
  size_t const count = points.size();
  if (count > 0)
  {
    ASSERT_GREATER(count, 2, ());

    deltas.push_back(EncodePointDeltaAsUint(points[0], basePoint));
    deltas.push_back(EncodePointDeltaAsUint(points[1], points[0]));
    deltas.push_back(EncodePointDeltaAsUint(points[2], points[1]));

    m2::PointD const maxPointD(maxPoint);
    for (size_t i = 3; i < count; ++i)
    {
      m2::PointU const prediction = PredictPointInTriangle(
            maxPointD, points[i - 1], points[i - 2], points[i - 3]);
      deltas.push_back(EncodePointDeltaAsUint(points[i], prediction));
    }
  }
}

void DecodeTriangleStrip(InDeltasT const & deltas, m2::PointU const & basePoint,
                         m2::PointU const & maxPoint, OutPointsT & points)
{
  size_t const count = deltas.size();
  if (count > 0)
  {
    ASSERT_GREATER(count, 2, ());

    points.push_back(DecodePointDeltaFromUint(deltas[0], basePoint));
    points.push_back(DecodePointDeltaFromUint(deltas[1], points.back()));
    points.push_back(DecodePointDeltaFromUint(deltas[2], points.back()));

    m2::PointD const maxPointD(maxPoint);
    for (size_t i = 3; i < count; ++i)
    {
      size_t const n = points.size();
      m2::PointU const prediction = PredictPointInTriangle(
            maxPointD, points[n - 1], points[n - 2], points[n - 3]);
      points.push_back(DecodePointDeltaFromUint(deltas[i], prediction));
    }
  }
}
}  // namespace coding

namespace serial
{
// GeometryCodingParams ----------------------------------------------------------------------------
GeometryCodingParams::GeometryCodingParams() : m_BasePointUint64(0), m_CoordBits(kPointCoordBits)
{
  m_BasePoint = Uint64ToPointUObsolete(m_BasePointUint64);
}

GeometryCodingParams::GeometryCodingParams(uint8_t coordBits, m2::PointD const & pt)
  : m_CoordBits(coordBits)
{
  SetBasePoint(pt);
}

GeometryCodingParams::GeometryCodingParams(uint8_t coordBits, uint64_t basePointUint64)
  : m_BasePointUint64(basePointUint64), m_CoordBits(coordBits)
{
  m_BasePoint = Uint64ToPointUObsolete(m_BasePointUint64);
}

void GeometryCodingParams::SetBasePoint(m2::PointD const & pt)
{
  m_BasePoint = PointDToPointU(pt, m_CoordBits);
  m_BasePointUint64 = PointUToUint64Obsolete(m_BasePoint);
}

namespace pts
{
m2::PointU D2U(m2::PointD const & p, uint32_t coordBits) { return PointDToPointU(p, coordBits); }

m2::PointD U2D(m2::PointU const & p, uint32_t coordBits)
{
  m2::PointD const pt = PointUToPointD(p, coordBits);
  ASSERT(mercator::Bounds::kMinX <= pt.x && pt.y <= mercator::Bounds::kMaxX, (p, pt, coordBits));
  ASSERT(mercator::Bounds::kMinY <= pt.x && pt.y <= mercator::Bounds::kMaxY, (p, pt, coordBits));
  return pt;
}

m2::PointU GetMaxPoint(GeometryCodingParams const & params)
{
  return D2U(m2::PointD(mercator::Bounds::kMaxX, mercator::Bounds::kMaxY), params.GetCoordBits());
}

m2::PointU GetBasePoint(GeometryCodingParams const & params) { return params.GetBasePoint(); }
}  // namespace pts

void Encode(EncodeFunT fn, std::vector<m2::PointD> const & points, GeometryCodingParams const & params,
            DeltasT & deltas)
{
  size_t const count = points.size();

  pts::PointsU upoints;
  upoints.reserve(count);

  transform(points.begin(), points.end(), std::back_inserter(upoints),
            std::bind(&pts::D2U, std::placeholders::_1, params.GetCoordBits()));

  ASSERT(deltas.empty(), ());
  deltas.resize(count);

  coding::OutDeltasT adapt(deltas);
  (*fn)(make_read_adapter(upoints), pts::GetBasePoint(params), pts::GetMaxPoint(params), adapt);
}

void Decode(DecodeFunT fn, DeltasT const & deltas, GeometryCodingParams const & params,
            OutPointsT & points, size_t reserveF)
{
  DecodeImpl(fn, deltas, params, points, reserveF);
}

void Decode(DecodeFunT fn, DeltasT const & deltas, GeometryCodingParams const & params,
            std::vector<m2::PointD> & points, size_t reserveF)
{
  DecodeImpl(fn, deltas, params, points, reserveF);
}

void const * LoadInner(DecodeFunT fn, void const * pBeg, size_t count,
                       GeometryCodingParams const & params, OutPointsT & points)
{
  DeltasT deltas;
  deltas.reserve(count);
  void const * ret =
      ReadVarUint64Array(static_cast<char const *>(pBeg), count, base::MakeBackInsertFunctor(deltas));

  Decode(fn, deltas, params, points);
  return ret;
}

TrianglesChainSaver::TrianglesChainSaver(GeometryCodingParams const & params)
{
  m_base = pts::GetBasePoint(params);
  m_max = pts::GetMaxPoint(params);
}

void TrianglesChainSaver::operator()(TPoint arr[3], std::vector<TEdge> edges)
{
  m_buffers.push_back(TBuffer());
  MemWriter<TBuffer> writer(m_buffers.back());

  WriteVarUint(writer, coding::EncodePointDeltaAsUint(arr[0], m_base));
  WriteVarUint(writer, coding::EncodePointDeltaAsUint(arr[1], arr[0]));

  TEdge curr = edges.front();
  curr.m_delta = coding::EncodePointDeltaAsUint(arr[2], arr[1]);

  sort(edges.begin(), edges.end(), edge_less_p0());

  std::stack<TEdge> st;
  while (true)
  {
    CHECK_EQUAL(curr.m_delta >> 62, 0, ());
    uint64_t delta = curr.m_delta << 2;

    // find next edges
    int const nextNode = curr.m_p[1];
    auto i = lower_bound(edges.begin(), edges.end(), nextNode, edge_less_p0());
    bool const found = (i != edges.end() && i->m_p[0] == nextNode);
    if (found)
    {
      // fill 2 tree-struct bites
      ASSERT_NOT_EQUAL(i->m_side, -1, ());

      uint64_t const one = 1;

      // first child
      delta |= (one << i->m_side);

      std::vector<TEdge>::iterator j = i + 1;
      if (j != edges.end() && j->m_p[0] == nextNode)
      {
        // second child
        ASSERT_EQUAL(i->m_side, 0, ());
        ASSERT_EQUAL(j->m_side, 1, ());

        delta |= (one << j->m_side);

        // push to stack for further processing
        st.push(*j);
      }

      curr = *i;
    }

    // write delta for current element
    WriteVarUint(writer, delta);

    if (!found)
    {
      // end of chain - pop current from stack or exit
      if (st.empty())
        break;
      else
      {
        curr = st.top();
        st.pop();
      }
    }
  }
}

void DecodeTriangles(coding::InDeltasT const & deltas, m2::PointU const & basePoint,
                     m2::PointU const & maxPoint, coding::OutPointsT & points)
{
  size_t const count = deltas.size();
  ASSERT_GREATER(count, 2, ());

  m2::PointD const maxPointD(maxPoint);

  points.push_back(coding::DecodePointDeltaFromUint(deltas[0], basePoint));
  points.push_back(coding::DecodePointDeltaFromUint(deltas[1], points.back()));
  points.push_back(coding::DecodePointDeltaFromUint(deltas[2] >> 2, points.back()));

  std::stack<size_t> st;

  size_t ind = 2;
  uint8_t treeBits = deltas[2] & 3;

  for (size_t i = 3; i < count;)
  {
    // points 0, 1 - is a common edge
    // point 2 - is an opposite point for new triangle to calculate prediction
    size_t trg[3];

    if (treeBits & 1)
    {
      // common edge is 1->2
      trg[0] = ind;
      trg[1] = ind - 1;
      trg[2] = ind - 2;

      // push to stack for further processing
      if (treeBits & 2)
        st.push(ind);
    }
    else if (treeBits & 2)
    {
      // common edge is 2->0
      trg[0] = ind - 2;
      trg[1] = ind;
      trg[2] = ind - 1;
    }
    else
    {
      // end of chain - pop current from stack
      ASSERT(!st.empty(), ());
      ind = st.top();
      st.pop();
      treeBits = 2;
      continue;
    }

    // push points
    points.push_back(points[trg[0]]);
    points.push_back(points[trg[1]]);
    points.push_back(coding::DecodePointDeltaFromUint(deltas[i] >> 2, coding::PredictPointInTriangle(
            maxPointD, points[trg[0]], points[trg[1]], points[trg[2]])));

    // next step
    treeBits = deltas[i] & 3;
    ind = points.size() - 1;
    ++i;
  }

  ASSERT(treeBits == 0 && st.empty(), ());
}
}  // namespace serial
