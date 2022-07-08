#pragma once

#include "indexer/feature.hpp"

#include <cstdint>
#include <map>
#include <ostream>
#include <string>

namespace stats
{
  struct GeneralInfo
  {
    GeneralInfo() : m_count(0), m_size(0), m_names(0), m_length(0), m_area(0) {}

    void Add(uint64_t szBytes, double len = 0, double area = 0, bool hasName = false)
    {
      if (szBytes > 0)
      {
        ++m_count;
        m_size += szBytes;
        m_length += len;
        m_area += area;
        if (hasName)
          ++m_names;
      }
    }

    uint64_t m_count;
    uint64_t m_size;
    uint64_t m_names;
    double m_length;
    double m_area;
  };

  struct GeomInfo
  {
    GeomInfo() : m_count(0), m_size(0), m_elements(0) {}

    void Add(uint64_t szBytes, uint32_t elements)
    {
      if (szBytes > 0)
      {
        ++m_count;
        m_size += szBytes;
        m_elements += elements;
      }
    }

    uint64_t m_count, m_size, m_elements;
  };

  using GeomStats = GeomInfo[feature::DataHeader::kMaxScalesCount];

  template <class T, int Tag>
  struct IntegralType
  {
    T m_val;
    explicit IntegralType(T v) : m_val(v) {}
    bool operator<(IntegralType const & rhs) const { return m_val < rhs.m_val; }
  };

  using ClassifType = IntegralType<uint32_t, 0>;
  using CountType = IntegralType<uint32_t, 1>;
  using AreaType = IntegralType<size_t, 2>;

  struct MapInfo
  {
    std::map<feature::GeomType, GeneralInfo> m_byGeomType;
    std::map<ClassifType, GeneralInfo> m_byClassifType;
    std::map<CountType, GeneralInfo> m_byPointsCount, m_byTrgCount;
    std::map<AreaType, GeneralInfo> m_byAreaSize;

    GeomStats m_byLineGeom, m_byAreaGeom,
              m_byLineGeomCompared, m_byAreaGeomCompared,
              m_byLineGeomDup, m_byAreaGeomDup;

    GeneralInfo m_inner[3];
  };

  void FileContainerStatistics(std::ostream & os, std::string const & fPath);

  void CalcStatistics(std::string const & fPath, MapInfo & info);
  void PrintStatistics(std::ostream & os, MapInfo & info);
  void PrintTypeStatistics(std::ostream & os, MapInfo & info);
  void PrintOuterGeometryStatistics(std::ostream & os, MapInfo & info);
}
