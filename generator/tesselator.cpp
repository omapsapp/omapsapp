#include "generator/tesselator.hpp"

#include "coding/geometry_coding.hpp"
#include "coding/writer.hpp"

#include "geometry/robust_orientation.hpp"

#include "base/assert.hpp"
#include "base/logging.hpp"

#include <limits>
#include <memory>
#include <queue>

#include "3party/libtess2/Include/tesselator.h"

namespace tesselator
{
int TesselateInterior(PolygonsT const & polys, TrianglesInfo & info)
{
  int constexpr kCoordinatesPerVertex = 2;
  int constexpr kVerticesInPolygon = 3;

  auto const deleter = [](TESStesselator * tess) {tessDeleteTess(tess);};
  std::unique_ptr<TESStesselator, decltype(deleter)> tess(tessNewTess(nullptr), deleter);

  for (auto const & contour : polys)
  {
    tessAddContour(tess.get(), kCoordinatesPerVertex, &contour[0], sizeof(contour[0]),
                   static_cast<int>(contour.size()));
  }

  if (0 == tessTesselate(tess.get(), TESS_WINDING_ODD, TESS_CONSTRAINED_DELAUNAY_TRIANGLES,
                         kVerticesInPolygon, kCoordinatesPerVertex, nullptr))
  {
    LOG(LERROR, ("Tesselator error for polygon", polys));
    return 0;
  }

  int const elementCount = tessGetElementCount(tess.get());
  if (elementCount)
  {
    int const vertexCount = tessGetVertexCount(tess.get());
    TESSreal const * vertices = tessGetVertices(tess.get());
    m2::PointD const * points = reinterpret_cast<m2::PointD const *>(vertices);
    info.AssignPoints(points, points + vertexCount);

    // Elements are triplets of vertex indices.
    TESSindex const * elements = tessGetElements(tess.get());
    info.Reserve(elementCount);
    for (int i = 0; i < elementCount; ++i)
      info.Add(elements[i * 3], elements[i * 3 + 1], elements[i * 3 + 2]);
  }
  return elementCount;
}

  ///////////////////////////////////////////////////////////////////////////////////////////////////////////
  // TrianglesInfo::ListInfo implementation
  ///////////////////////////////////////////////////////////////////////////////////////////////////////////

  int TrianglesInfo::ListInfo::empty_key = -1;

  void TrianglesInfo::ListInfo::AddNeighbour(int p1, int p2, int trg)
  {
    // find or insert element for key
    std::pair<TNeighbours::iterator, bool> ret = m_neighbors.insert(std::make_pair(std::make_pair(p1, p2), trg));

    // triangles should not duplicate
    CHECK ( ret.second, ("Duplicating triangles for indices : ", p1, p2) );
  }

  void TrianglesInfo::ListInfo::Add(int p0, int p1, int p2)
  {
    m_triangles.emplace_back(p0, p1, p2);

    int const trg = static_cast<int>(m_triangles.size()) - 1;
    AddNeighbour(p0, p1, trg);
    AddNeighbour(p1, p2, trg);
    AddNeighbour(p2, p0, trg);
  }

  template <class IterT> size_t GetBufferSize(IterT b, IterT e)
  {
    std::vector<char> buffer;
    MemWriter<std::vector<char> > writer(buffer);
    while (b != e) WriteVarUint(writer, *b++);
    return buffer.size();
  }

  /// Find best (cheap in serialization) start edge for processing.
  TrianglesInfo::ListInfo::TIterator
  TrianglesInfo::ListInfo::FindStartTriangle(PointsInfo const & points) const
  {
    TIterator ret = m_neighbors.end();
    size_t cr = std::numeric_limits<size_t>::max();

    for (TIterator i = m_neighbors.begin(); i != m_neighbors.end(); ++i)
    {
      if (!m_visited[i->second] &&
          m_neighbors.find(std::make_pair(i->first.second, i->first.first)) == m_neighbors.end())
      {
        uint64_t deltas[3];
        deltas[0] = coding::EncodePointDeltaAsUint(points.m_points[i->first.first], points.m_base);
        deltas[1] = coding::EncodePointDeltaAsUint(points.m_points[i->first.second],
                                                   points.m_points[i->first.first]);
        deltas[2] = coding::EncodePointDeltaAsUint(
            points.m_points[m_triangles[i->second].GetPoint3(i->first)],
            points.m_points[i->first.second]);

        size_t const sz = GetBufferSize(deltas, deltas + 3);
        if (sz < cr)
        {
          ret = i;
          cr = sz;
        }
      }
    }

    ASSERT ( ret != m_neighbors.end(), ("?WTF? There is no border triangles!") );
    return ret;
  }

  /// Return indexes of common edges of [to, from] triangles.
  std::pair<int, int> CommonEdge(Triangle const & to, Triangle const & from)
  {
    for (int i = 0; i < 3; ++i)
    {
      for (int j = 0; j < 3; ++j)
      {
        if (to.m_p[i] == from.m_p[math::NextModN(j, 3)] && to.m_p[math::NextModN(i, 3)] == from.m_p[j])
          return std::make_pair(i, j);
      }
    }

    ASSERT ( false, ("?WTF? Triangles not neighbors!") );
    return std::make_pair(-1, -1);
  }

  /// Get neighbors of 'trg' triangle, which was achieved from 'from' triangle.
  /// @param[out] nb  neighbors indexes of 'trg' if 0->1 is common edge with'from':
  /// - nb[0] - by 1->2 edge;
  /// - nb[1] - by 2->0 edge;
  void TrianglesInfo::ListInfo::GetNeighbors(
      Triangle const & trg, Triangle const & from, int * nb) const
  {
    int i = math::NextModN(CommonEdge(trg, from).first, 3);
    int j = math::NextModN(i, 3);

    int ind = 0;
    TIterator it = m_neighbors.find(std::make_pair(trg.m_p[j], trg.m_p[i]));
    nb[ind++] = (it != m_neighbors.end()) ? it->second : empty_key;

    it = m_neighbors.find(std::make_pair(trg.m_p[math::NextModN(j, 3)], trg.m_p[j]));
    nb[ind++] = (it != m_neighbors.end()) ? it->second : empty_key;
  }

  /// Calc delta of 'from'->'to' graph edge.
  uint64_t TrianglesInfo::ListInfo::CalcDelta(
      PointsInfo const & points, Triangle const & from, Triangle const & to) const
  {
    std::pair<int, int> const p = CommonEdge(to, from);

    m2::PointU const prediction = coding::PredictPointInTriangle(
        points.m_max,
        // common edge with 'to'
        points.m_points[from.m_p[(p.second + 1) % 3]], points.m_points[from.m_p[(p.second)]],
        // diagonal point of 'from'
        points.m_points[from.m_p[(p.second + 2) % 3]]);

    // delta from prediction to diagonal point of 'to'
    return coding::EncodePointDeltaAsUint(points.m_points[to.m_p[(p.first + 2) % 3]], prediction);
  }

  template <class TPopOrder>
  void TrianglesInfo::ListInfo::MakeTrianglesChainImpl(
      PointsInfo const & points, TIterator start, std::vector<Edge> & chain) const
  {
    chain.clear();

    Triangle const fictive(start->first.second, start->first.first, -1);

    std::priority_queue<Edge, std::vector<Edge>, TPopOrder> q;
    q.push(Edge(-1, start->second, 0, -1));

    while (!q.empty())
    {
      // pop current element
      Edge e = q.top();
      q.pop();

      // check if already processed
      if (m_visited[e.m_p[1]])
        continue;
      m_visited[e.m_p[1]] = true;

      // push to chain
      chain.push_back(e);

      Triangle const & trg = m_triangles[e.m_p[1]];

      // get neighbors
      int nb[2];
      GetNeighbors(trg, (e.m_p[0] == -1) ? fictive : m_triangles[e.m_p[0]], nb);

      // push neighbors to queue
      for (int i = 0; i < 2; ++i)
        if (nb[i] != empty_key && !m_visited[nb[i]])
          q.push(Edge(e.m_p[1], nb[i], CalcDelta(points, trg, m_triangles[nb[i]]), i));
    }
  }

  // Element with less m_delta is better than another one.
  struct edge_greater_delta
  {
    bool operator() (Edge const & e1, Edge const & e2) const
    {
      return (e1.m_delta > e2.m_delta);
    }
  };

  // Experimental ...
  struct edge_less_delta
  {
    bool operator() (Edge const & e1, Edge const & e2) const
    {
      return (e1.m_delta < e2.m_delta);
    }
  };

  void TrianglesInfo::ListInfo::MakeTrianglesChain(
    PointsInfo const & points, TIterator start, std::vector<Edge> & chain, bool /*goodOrder*/) const
  {
    //if (goodOrder)
      MakeTrianglesChainImpl<edge_greater_delta>(points, start, chain);
    //else
    //  MakeTrianglesChainImpl<edge_less_delta>(points, start, chain);
  }

  void TrianglesInfo::Add(int p0, int p1, int p2)
  {
    m_triangles.back().Add(p0, p1, p2);
  }

  void TrianglesInfo::GetPointsInfo(m2::PointU const & baseP,
                                    m2::PointU const & maxP,
                                    std::function<m2::PointU (m2::PointD)> const & convert,
                                    PointsInfo & info) const
  {
    info.m_base = baseP;
    info.m_max = m2::PointD(maxP);

    size_t const count = m_points.size();
    info.m_points.reserve(count);
    for (size_t i = 0; i < count; ++i)
      info.m_points.push_back(convert(m_points[i]));
  }
}
