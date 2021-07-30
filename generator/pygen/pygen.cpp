#include "generator/utils.hpp"

#include "indexer/classificator.hpp"
#include "indexer/classificator_loader.hpp"
#include "indexer/feature.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/feature_decl.hpp"
#include "indexer/feature_meta.hpp"
#include "indexer/feature_processor.hpp"
#include "indexer/features_vector.hpp"

#include "platform/mwm_version.hpp"
#include "platform/platform.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"
#include "geometry/triangle2d.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"

#include "pyhelpers/module_version.hpp"

#include <fstream>
#include <limits>
#include <map>
#include <string>

#include "3party/boost/boost/noncopyable.hpp"
#include "3party/boost/boost/python.hpp"
#include "3party/boost/boost/shared_ptr.hpp"
#include "3party/boost/boost/weak_ptr.hpp"

using namespace feature;
namespace bp = boost::python;

namespace
{
class Mwm;
class MwmIter;

uint32_t const kInvalidIndex = std::numeric_limits<uint32_t>::max();

class FeatureTypeWrapper
{
public:
  explicit FeatureTypeWrapper(boost::shared_ptr<Mwm> const & mwm,
                              boost::shared_ptr<FeatureType> const & feature)
    : m_mwm(mwm), m_feature(feature)
  {
  }

  uint32_t GetIndex() const { return m_feature->GetID().m_index; }

  bp::list GetTypes()
  {
    bp::list types;
    m_feature->ForEachType([&](auto t) {
      // A type can be invalid because the type was marked as deprecated in mapcss file.
      types.append(classif().IsTypeValid(t) ? classif().GetIndexForType(t) : kInvalidIndex);
    });
    return types;
  }

  bp::dict GetMetadata()
  {
    bp::dict mmetadata;
    auto const & metadata = m_feature->GetMetadata();
    for (auto k : metadata.GetKeys())
      mmetadata[k] = metadata.Get(k);

    return mmetadata;
  }

  bp::dict GetNames()
  {
    bp::dict mnames;
    auto const & name = m_feature->GetNames();
    name.ForEach([&](auto code, auto && str) {
      mnames[StringUtf8Multilang::GetLangByCode(code)] = str;
    });

    return mnames;
  }

  std::string GetReadableName()
  {
    std::string name;
    m_feature->GetReadableName(name);
    return name;
  }

  uint8_t GetRank() { return m_feature->GetRank(); }

  uint64_t GetPopulation() { return m_feature->GetPopulation(); }

  std::string GetRoadNumber() { return m_feature->GetRoadNumber(); }

  std::string GetHouseNumber() { return m_feature->GetHouseNumber(); }

  int8_t GetLayer() { return m_feature->GetLayer(); }

  GeomType GetGeomType() { return m_feature->GetGeomType(); }

  bp::list GetGeometry()
  {
    bp::list geometry;
    switch (m_feature->GetGeomType())
    {
    case GeomType::Point:
    case GeomType::Line:
    {
      m_feature->ForEachPoint([&](auto const & p) { geometry.append(p); },
                              FeatureType::BEST_GEOMETRY);
    }
    break;
    case GeomType::Area:
    {
      m_feature->ForEachTriangle(
          [&](auto const & p1, auto const & p2, auto const & p3) {
            geometry.append(m2::TriangleD(p1, p2, p3));
          },
          FeatureType::BEST_GEOMETRY);
    }
    break;
    case GeomType::Undefined: break;
    default: UNREACHABLE();
    }

    return geometry;
  }

  m2::PointD GetCenter() { return feature::GetCenter(*m_feature); }

  m2::RectD GetLimitRect() { return m_feature->GetLimitRect(FeatureType::BEST_GEOMETRY); }

  void ParseAll()
  {
    // FeatureType is a lazy data loader, but it can cache all data.
    // We need to run all methods to store data into cache.
    GetIndex();
    GetTypes();
    GetMetadata();
    GetNames();
    GetReadableName();
    GetRank();
    GetPopulation();
    GetRoadNumber();
    GetHouseNumber();
    GetLayer();
    GetGeomType();
    GetGeometry();
    GetCenter();
    GetLimitRect();
  }

  std::string DebugString() { return m_feature->DebugString(FeatureType::BEST_GEOMETRY); }

private:
  boost::shared_ptr<Mwm> m_mwm;
  boost::shared_ptr<FeatureType> m_feature;
};

class Mwm
{
public:
  static boost::shared_ptr<Mwm> Create(std::string const & filename, bool parse = true)
  {
    // We cannot use boost::make_shared, because the constructor is private.
    auto ptr = boost::shared_ptr<Mwm>(new Mwm(filename, parse));
    ptr->SetSelfPtr(ptr);
    return ptr;
  }

  version::MwmVersion const & GetVersion() const { return m_mwmValue.GetMwmVersion(); }

  DataHeader::MapType GetType() const { return m_mwmValue.GetHeader().GetType(); }

  m2::RectD GetBounds() const { return m_mwmValue.GetHeader().GetBounds(); }

  size_t Size() const { return m_guard->GetNumFeatures(); }

  FeatureTypeWrapper GetFeatureByIndex(uint32_t index)
  {
    FeatureTypeWrapper ftw(m_self.lock(), m_guard->GetFeatureByIndex(index));
    if (m_parse)
      ftw.ParseAll();

    return ftw;
  }

  bp::dict GetSectionsInfo() const
  {
    bp::dict sectionsInfo;
    m_mwmValue.m_cont.ForEachTagInfo([&](auto const & info) { sectionsInfo[info.m_tag] = info; });
    return sectionsInfo;
  }

  MwmIter MakeMwmIter();

private:
  explicit Mwm(std::string const & filename, bool parse = true)
    : m_ds(filename)
    , m_mwmValue(m_ds.GetLocalCountryFile())
    , m_guard(std::make_unique<FeaturesLoaderGuard>(m_ds.GetDataSource(), m_ds.GetMwmId()))
    , m_parse(parse)
  {
  }

  void SetSelfPtr(boost::weak_ptr<Mwm> const & self) { m_self = self; }

  generator::SingleMwmDataSource m_ds;
  MwmValue m_mwmValue;
  std::unique_ptr<FeaturesLoaderGuard> m_guard;
  boost::weak_ptr<Mwm> m_self;
  bool m_parse = false;
};

BOOST_PYTHON_FUNCTION_OVERLOADS(MwmCreateOverloads, Mwm::Create, 1 /* min_args */,
                                2 /* max_args */);

class MwmIter
{
public:
  MwmIter(boost::shared_ptr<Mwm> const & mwm) : m_mwm(mwm) {}

  FeatureTypeWrapper Next()
  {
    if (m_current == m_mwm->Size())
    {
      PyErr_SetNone(PyExc_StopIteration);
      bp::throw_error_already_set();
    }

    return m_mwm->GetFeatureByIndex(m_current++);
  }

private:
  boost::shared_ptr<Mwm> m_mwm;
  uint32_t m_current = 0;
};

MwmIter Mwm::MakeMwmIter() { return MwmIter(m_self.lock()); }

std::string ReadAll(std::string const & filename)
{
  std::ifstream file;
  file.exceptions(std::ios::failbit | std::ios::badbit);
  file.open(filename);
  return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void InitClassificator(std::string const & resourcePath)
{
  classificator::LoadTypes(
        ReadAll(base::JoinPath(resourcePath, "classificator.txt")),
        ReadAll(base::JoinPath(resourcePath, "types.txt"))
  );
}

struct GeometryNamespace
{
};

struct MwmNamespace
{
};

struct ClassifNamespace
{
};
}  // namespace

BOOST_PYTHON_MODULE(pygen)
{
  bp::scope().attr("__version__") = PYBINDINGS_VERSION;

  {
    bp::scope geometryNamespace = bp::class_<GeometryNamespace>("geometry");

    bp::class_<m2::PointD>("PointD", bp::init<double, double>())
        .def_readwrite("x", &m2::PointD::x)
        .def_readwrite("y", &m2::PointD::y)
        .def("__repr__", static_cast<std::string (*)(m2::PointD const &)>(m2::DebugPrint));

    bp::class_<m2::TriangleD>("TriangleD", bp::init<m2::PointD, m2::PointD, m2::PointD>())
        .def("x", &m2::TriangleD::p1, bp::return_value_policy<bp::copy_const_reference>())
        .def("y", &m2::TriangleD::p2, bp::return_value_policy<bp::copy_const_reference>())
        .def("z", &m2::TriangleD::p3, bp::return_value_policy<bp::copy_const_reference>())
        .def("__repr__", static_cast<std::string (*)(m2::TriangleD const &)>(m2::DebugPrint));

    bp::class_<m2::RectD>("RectD", bp::init<double, double, double, double>())
        .def(bp::init<m2::PointD, m2::PointD>())
        .add_property("min_x", &m2::RectD::minX, &m2::RectD::setMinX)
        .add_property("min_y", &m2::RectD::minY, &m2::RectD::setMinY)
        .add_property("max_x", &m2::RectD::maxX, &m2::RectD::setMaxX)
        .add_property("max_y", &m2::RectD::maxY, &m2::RectD::setMaxY)
        .add_property(
            "right_top", &m2::RectD::RightTop,
            +[](m2::RectD & self, m2::RectD const & p) {
              self.setMaxX(p.maxX());
              self.setMaxY(p.maxY());
            })
        .add_property(
            "left_bottom", &m2::RectD::LeftBottom,
            +[](m2::RectD & self, m2::RectD const & p) {
              self.setMinX(p.minX());
              self.setMinY(p.minY());
            })
        .def("__repr__", static_cast<std::string (*)(m2::RectD const &)>(m2::DebugPrint));
  }
  {
    bp::scope mwmNamespace = bp::class_<MwmNamespace>("mwm");

    bp::enum_<Metadata::EType>("EType")
        .value("cuisine", Metadata::EType::FMD_CUISINE)
        .value("open_hours", Metadata::EType::FMD_OPEN_HOURS)
        .value("phone_number", Metadata::EType::FMD_PHONE_NUMBER)
        .value("fax_number", Metadata::EType::FMD_FAX_NUMBER)
        .value("stars", Metadata::EType::FMD_STARS)
        .value("operator", Metadata::EType::FMD_OPERATOR)
        .value("url", Metadata::EType::FMD_URL)
        .value("website", Metadata::EType::FMD_WEBSITE)
        .value("internet", Metadata::EType::FMD_INTERNET)
        .value("ele", Metadata::EType::FMD_ELE)
        .value("turn_lanes", Metadata::EType::FMD_TURN_LANES)
        .value("turn_lanes_forward", Metadata::EType::FMD_TURN_LANES_FORWARD)
        .value("turn_lanes_backward", Metadata::EType::FMD_TURN_LANES_BACKWARD)
        .value("email", Metadata::EType::FMD_EMAIL)
        .value("postcode", Metadata::EType::FMD_POSTCODE)
        .value("wikipedia", Metadata::EType::FMD_WIKIPEDIA)
        .value("flats", Metadata::EType::FMD_FLATS)
        .value("height", Metadata::EType::FMD_HEIGHT)
        .value("min_height", Metadata::EType::FMD_MIN_HEIGHT)
        .value("denomination", Metadata::EType::FMD_DENOMINATION)
        .value("building_levels", Metadata::EType::FMD_BUILDING_LEVELS)
        .value("test_id", Metadata::EType::FMD_TEST_ID)
        .value("sponsored_id", Metadata::EType::FMD_SPONSORED_ID)
        .value("price_rate", Metadata::EType::FMD_PRICE_RATE)
        .value("rating", Metadata::EType::FMD_RATING)
        .value("banner_url", Metadata::EType::FMD_BANNER_URL)
        .value("level", Metadata::EType::FMD_LEVEL)
        .value("airport_iata", Metadata::EType::FMD_AIRPORT_IATA)
        .value("brand", Metadata::EType::FMD_BRAND)
        .value("duration", Metadata::EType::FMD_DURATION)
        .value("facebook_page", Metadata::EType::FMD_FACEBOOK_PAGE)
        .value("instagram_page", Metadata::EType::FMD_INSTAGRAM_PAGE)
        .value("twitter_page", Metadata::EType::FMD_TWITTER_PAGE)
        .value("vk_page", Metadata::EType::FMD_VK_PAGE);

    bp::enum_<GeomType>("GeomType")
        .value("undefined", GeomType::Undefined)
        .value("point", GeomType::Point)
        .value("line", GeomType::Line)
        .value("area", GeomType::Area);

    bp::enum_<DataHeader::MapType>("MapType")
        .value("world", DataHeader::MapType::World)
        .value("worldCoasts", DataHeader::MapType::WorldCoasts)
        .value("country", DataHeader::MapType::Country);

    bp::enum_<version::Format>("MwmFormat")
        .value("unknown", version::Format::unknownFormat)
        .value("v1", version::Format::v1)
        .value("v2", version::Format::v2)
        .value("v3", version::Format::v3)
        .value("v4", version::Format::v4)
        .value("v5", version::Format::v5)
        .value("v6", version::Format::v6)
        .value("v7", version::Format::v7)
        .value("v8", version::Format::v8)
        .value("v9", version::Format::v9)
        .value("v10", version::Format::v10)
        .value("last", version::Format::lastFormat);

    bp::class_<FilesContainerR::TagInfo>("SectionInfo", bp::no_init)
        .def_readwrite("tag", &FilesContainerR::TagInfo::m_tag)
        .def_readwrite("offset", &FilesContainerR::TagInfo::m_offset)
        .def_readwrite("size", &FilesContainerR::TagInfo::m_size)
        .def("__repr__",
             static_cast<std::string (*)(FilesContainerR::TagInfo const &)>(DebugPrint));

    bp::class_<version::MwmVersion>("MwmVersion", bp::no_init)
        .def("format", &version::MwmVersion::GetFormat)
        .def("seconds_since_epoch", &version::MwmVersion::GetSecondsSinceEpoch)
        .def("version", &version::MwmVersion::GetVersion)
        .def("__repr__",
             static_cast<std::string (*)(version::MwmVersion const &)>(version::DebugPrint));

    bp::class_<FeatureTypeWrapper>("FeatureType", bp::no_init)
        .def("index", &FeatureTypeWrapper::GetIndex)
        .def("types", &FeatureTypeWrapper::GetTypes)
        .def("metadata", &FeatureTypeWrapper::GetMetadata)
        .def("names", &FeatureTypeWrapper::GetNames)
        .def("readable_name", &FeatureTypeWrapper::GetReadableName)
        .def("rank", &FeatureTypeWrapper::GetRank)
        .def("population", &FeatureTypeWrapper::GetPopulation)
        .def("road_number", &FeatureTypeWrapper::GetRoadNumber)
        .def("house_number", &FeatureTypeWrapper::GetHouseNumber)
        .def("layer", &FeatureTypeWrapper::GetLayer)
        .def("geom_type", &FeatureTypeWrapper::GetGeomType)
        .def("center", &FeatureTypeWrapper::GetCenter)
        .def("geometry", &FeatureTypeWrapper::GetGeometry)
        .def("limit_rect", &FeatureTypeWrapper::GetLimitRect)
        .def(
            "parse",
            +[](FeatureTypeWrapper & self) {
              self.ParseAll();
              return self;
            })
        .def("__repr__", &FeatureTypeWrapper::DebugString);

    bp::class_<MwmIter>("MwmIter", bp::no_init)
        .def(
            "__iter__", +[](MwmIter self) { return self; })
        .def("__next__", &MwmIter::Next)
        .def("next", &MwmIter::Next);

    bp::class_<Mwm, boost::shared_ptr<Mwm>, boost::noncopyable>("Mwm_", bp::no_init)
        .def("version", &Mwm::GetVersion, bp::return_value_policy<bp::copy_const_reference>())
        .def("type", &Mwm::GetType)
        .def("bounds", &Mwm::GetBounds)
        .def("sections_info", &Mwm::GetSectionsInfo)
        .def("__iter__", &Mwm::MakeMwmIter)
        .def("__len__", &Mwm::Size);

    bp::def("Mwm", &Mwm::Create, MwmCreateOverloads());
  }
  {
    bp::scope classifNamespace = bp::class_<ClassifNamespace>("classif");

    bp::def("init_classificator", InitClassificator);

    bp::def(
        "readable_type", +[](uint32_t index) {
          return index == kInvalidIndex
                     ? "unknown"
                     : classif().GetReadableObjectName(classif().GetTypeForIndex(index));
        });
  }
}
