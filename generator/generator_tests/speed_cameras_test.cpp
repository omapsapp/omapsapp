#include "testing/testing.hpp"

#include "generator/camera_info_collector.hpp"
#include "generator/maxspeeds_parser.hpp"

#include "generator/generator_tests_support/routing_helpers.hpp"
#include "generator/generator_tests_support/test_generator.hpp"

#include "routing/speed_camera_ser_des.hpp"

#include "routing_common/maxspeed_conversion.hpp"

#include "platform/measurement_utils.hpp"
#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"

#include "geometry/point2d.hpp"

#include "base/file_name_utils.hpp"
#include "base/math.hpp"

#include "defines.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace speed_cameras_test
{
using namespace generator;
using namespace measurement_utils;
using namespace routing;
using std::string;

// Directory name for creating test mwm and temporary files.
string const kTestDir = "speed_camera_generation_test";

// Temporary mwm name for testing.
string const kTestMwm = "test";

string const kOsmFileName = "town" OSM_DATA_FILE_EXTENSION;

double constexpr kCoefEqualityEpsilon = 1e-5;

// Pair of featureId and segmentId.
using routing::SegmentCoord;
using CameraMap = SpeedCamerasMapT;
using CameraMapItem = std::pair<SegmentCoord, RouteSegment::SpeedCamera>;

CameraMap LoadSpeedCameraFromMwm(string const & mwmFullPath)
{
  FilesContainerR const cont(mwmFullPath);
  CHECK(cont.IsExist(CAMERAS_INFO_FILE_TAG), ("Cannot find", CAMERAS_INFO_FILE_TAG, "section"));

  try
  {
    FilesContainerR::TReader const reader = cont.GetReader(CAMERAS_INFO_FILE_TAG);
    ReaderSource<FilesContainerR::TReader> src(reader);

    CameraMap result;
    DeserializeSpeedCamsFromMwm(src, result);
    return result;
  }
  catch (Reader::OpenException const & e)
  {
    TEST(false, ("Error while reading", CAMERAS_INFO_FILE_TAG, "section.", e.Msg()));
    return {};
  }
}

std::vector<CameraMapItem> UnpackMapToVector(CameraMap const & cameraMap)
{
  std::vector<CameraMapItem> result;
  for (auto const & mapIter : cameraMap)
  {
    for (auto const & vectorIter : mapIter.second)
      result.push_back({mapIter.first, vectorIter});
  }

  sort(result.begin(), result.end());
  return result;
}

void CheckCameraMapsEquality(CameraMap const & lhs, CameraMap const & rhs, double epsilon)
{
  TEST_EQUAL(lhs.size(), rhs.size(), ());

  auto const & vectorL = UnpackMapToVector(lhs);
  auto const & vectorR = UnpackMapToVector(rhs);

  for (size_t i = 0; i < vectorL.size(); ++i)
  {
    // Do not check feature id because of fake nodes. Data about them placed in ./data/
    // It can differ on Jenknins and local computer.
    TEST_EQUAL(vectorL[i].first.GetPointId(), vectorR[i].first.GetPointId(), ());
    TEST_EQUAL(vectorL[i].second.m_maxSpeedKmPH, vectorR[i].second.m_maxSpeedKmPH, ());
    TEST(AlmostEqualAbs(vectorL[i].second.m_coef, vectorR[i].second.m_coef, epsilon), ());
  }
}

void TestSpeedCameraSectionBuilding(string const & osmContent, CameraMap const & answer,
                                    double epsilon)
{
  // ScopedFile takes relative path only?! and makes WritableDir() + kOsmFileName.
  // Replace WritebleDir here with temporary path.
  Platform & platform = GetPlatform();
  auto const tmpDir = platform.TmpDir();
  platform.SetWritableDirForTests(tmpDir);

  platform::tests_support::ScopedFile const osmScopedFile(kOsmFileName, osmContent);

  generator::tests_support::TestRawGenerator generator;
  generator.SetupTmpFolder(base::JoinPath(tmpDir, kTestDir));
  generator.BuildFB(osmScopedFile.GetFullPath(), kTestMwm);
  generator.BuildFeatures(kTestMwm);

  auto const & genInfo = generator.GetGenInfo();

  // Step 3. Build section into mwm.
  string const camerasFilename = genInfo.GetIntermediateFileName(CAMERAS_TO_WAYS_FILENAME);

  if (!answer.empty())
  {
    // Check that intermediate file is non-empty.
    TEST_NOT_EQUAL(base::FileData(camerasFilename, base::FileData::Op::READ).Size(), 0,
                   ("SpeedCam intermediate file is empty"));
  }
  else
  {
    // Check that intermediate file is empty.
    TEST_EQUAL(base::FileData(camerasFilename, base::FileData::Op::READ).Size(), 0,
               ("SpeedCam intermediate file is non empty"));
  }

  string const osmToFeatureFilename = genInfo.GetTargetFileName(kTestMwm) + OSM2FEATURE_FILE_EXTENSION;

  auto const mwmFullPath = generator.GetMwmPath(kTestMwm);
  BuildCamerasInfo(mwmFullPath, camerasFilename, osmToFeatureFilename);

  CameraMap const cameras = LoadSpeedCameraFromMwm(mwmFullPath);
  CheckCameraMapsEquality(answer, cameras, epsilon);
}

// Next unit tests check building of speed camera section in mwm.

UNIT_TEST(SpeedCameraGenerationTest_Empty)
{
  string const osmContent = R"(
 <osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">
    <node id="10000000" lat="55.7793084" lon="37.3699375" version="1">
    </node>
    <node id="10000001" lat="55.7793085" lon="37.3699399" version="1"></node>
    <way id="2000000" version="1">
      <nd ref="10000000"/>
      <nd ref="10000001"/>
      <tag k="highway" v="primary"/>
    </way>
  </osm>
  )";

  CameraMap const answer = {};
  TestSpeedCameraSectionBuilding(osmContent, answer, kCoefEqualityEpsilon);
}

UNIT_TEST(SpeedCameraGenerationTest_CameraIsOnePointOfFeature_1)
{
  string const osmContent = R"(
 <osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">
    <node id="10000000" lat="55.7793084" lon="37.3699375" version="1">
      <tag k="highway" v="speed_camera"/>
      <tag k="maxspeed" v="100"/>
    </node>
    <node id="10000001" lat="55.7793085" lon="37.3699399" version="1"></node>
    <way id="2000000" version="1">
      <nd ref="10000000"/>
      <nd ref="10000001"/>
      <tag k="highway" v="primary"/>
    </way>
  </osm>
  )";

  // Geometry:
  // Feature number 0: <node-camera>----<node>
  // Result:
  // {(0, 0), (0, 100)} - featureId - 0, segmentId - 0,
  //                      coef - position on segment (at the beginning of segment) - 0,
  //                      maxSpeed - 100.

  CameraMap const answer = {
    {SegmentCoord(0, 0), std::vector<RouteSegment::SpeedCamera>{{0, 100}}}
  };
  TestSpeedCameraSectionBuilding(osmContent, answer, kCoefEqualityEpsilon);
}

UNIT_TEST(SpeedCameraGenerationTest_CameraIsOnePointOfFeature_2)
{
  string const osmContent = R"(
 <osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">
    <node id="10000001" lat="55.7793000" lon="37.3699000" version="1"></node>
    <node id="10000002" lat="55.7793100" lon="37.3699100" version="1">
      <tag k="highway" v="speed_camera"/>
      <tag k="maxspeed" v="100"/>
    </node>
    <node id="10000003" lat="55.7793200" lon="37.3699200" version="1"></node>
    <way id="2000000" version="1">
      <nd ref="10000001"/>
      <nd ref="10000002"/>
      <nd ref="10000003"/>
      <tag k="highway" v="primary"/>
    </way>
  </osm>
  )";

  // Geometry:
  // Feature number 0: <done>----<node-camera>----<node>
  //                             ^____segmentID = 1____^
  // Result:
  // {(0, 1), (0, 100)} - featureId - 0, segmentId - 1,
  //                      coef - position on segment (at the beginning of segment) - 0,
  //                      maxSpeed - 100.

  CameraMap const answer = {
    {SegmentCoord(0, 1), std::vector<RouteSegment::SpeedCamera>{{0, 100}}}
  };
  TestSpeedCameraSectionBuilding(osmContent, answer, kCoefEqualityEpsilon);
}

UNIT_TEST(SpeedCameraGenerationTest_CameraIsOnePointOfFeature_3)
{
  string const osmContent = R"(
 <osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">
    <node id="10000001" lat="55.7793000" lon="37.3699000" version="1"></node>
    <node id="10000002" lat="55.7793100" lon="37.3699100" version="1"></node>
    <node id="10000003" lat="55.7793200" lon="37.3699200" version="1">
      <tag k="highway" v="speed_camera"/>
      <tag k="maxspeed" v="100"/>
    </node>
    <way id="2000000" version="1">
      <nd ref="10000001"/>
      <nd ref="10000002"/>
      <nd ref="10000003"/>
      <tag k="highway" v="primary"/>
    </way>
  </osm>
  )";

  // Geometry:
  // Feature number 0: <node>----<node>----<node-camera>
  //                             ^____segmentID = 1____^
  // Result:
  // {(0, 1), (1, 100)} - featureId - 0, segmentId - 1,
  //                      coef - position on segment (at the end of segment) - 1,
  //                      maxSpeed - 100.

  CameraMap const answer = {
    {SegmentCoord(0, 1), std::vector<RouteSegment::SpeedCamera>{{1, 100}}}
  };
  TestSpeedCameraSectionBuilding(osmContent, answer, kCoefEqualityEpsilon);
}

UNIT_TEST(SpeedCameraGenerationTest_CameraIsOnePointOfFeature_4)
{
  string const osmContent = R"(
 <osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">
    <node id="10000001" lat="55.7793100" lon="37.3699100" version="1"></node>
    <node id="10000002" lat="55.7793200" lon="37.3699200" version="1"></node>
    <node id="10000003" lat="55.7793300" lon="37.3699300" version="1">
      <tag k="highway" v="speed_camera"/>
      <tag k="maxspeed" v="100"/>
    </node>

    <node id="10000004" lat="55.7793400" lon="37.3699400" version="1"></node>
    <node id="10000005" lat="55.7793500" lon="37.3699500" version="1">
      <tag k="highway" v="speed_camera"/>
      <tag k="maxspeed" v="100"/>
    </node>
    <node id="10000006" lat="55.7793600" lon="37.3699600" version="1"></node>

    <way id="2000000" version="1">
      <nd ref="10000001"/>
      <nd ref="10000002"/>
      <nd ref="10000003"/>
      <tag k="highway" v="primary"/>
    </way>

    <way id="2000001" version="1">
      <nd ref="10000004"/>
      <nd ref="10000005"/>
      <nd ref="10000006"/>
      <tag k="highway" v="primary"/>
    </way>
  </osm>
  )";

  // Geometry:
  // Feature number 0: <node>----<node>----<node-camera>
  //                             ^____segmentID = 1____^
  //
  // Feature number 1: <node>----<node-camera>----<node>
  //                             ^____segmentID = 1____^
  // Result:
  // {
  //   (0, 1), (1, 100) - featureId - 0, segmentId - 1,
  //                      coef - position on segment (at the end of segment) - 1,
  //                      maxSpeed - 100.
  //
  //   (1, 1), (0, 100) - featureId - 1, segmentId - 1,
  //                      coef - position on segment (at the beginning of segment) - 0,
  //                      maxSpeed - 100.

  CameraMap const answer = {
    {SegmentCoord(0, 1), std::vector<RouteSegment::SpeedCamera>{{1, 100}}},
    {SegmentCoord(1, 1), std::vector<RouteSegment::SpeedCamera>{{0, 100}}}
  };
  TestSpeedCameraSectionBuilding(osmContent, answer, kCoefEqualityEpsilon);
}

UNIT_TEST(SpeedCameraGenerationTest_CameraIsNearFeature_1)
{
  string const osmContent = R"(
 <osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">
    <node id="10000001" lat="55.7793100" lon="37.3699100" version="1"></node>
    <node id="10000002" lat="55.7793200" lon="37.3699200" version="1">
      <tag k="highway" v="speed_camera"/>
      <tag k="maxspeed" v="100"/>
    </node>
    <node id="10000003" lat="55.7793300" lon="37.3699300" version="1"></node>

    <way id="2000000" version="1">
      <nd ref="10000001"/>
      <nd ref="10000003"/>
      <tag k="highway" v="primary"/>
    </way>

  </osm>
  )";

  // Geometry:
  // Feature number 0: <node>--------<node>
  //                             ^___ somewhere here camera, but it is not the point of segment.
  //                                  We add it with coef close to 0.5 because of points' coords
  //                                  (lat, lon). Coef was calculated by this program and checked by
  //                                  eye.
  // Result:
  // {(0, 0), (0.5, 100)} - featureId - 0, segmentId - 0,
  //                        coef - position on segment (at the half of segment) - 0.5,
  //                        maxSpeed - 100.

  auto epsilon = mercator::DistanceOnEarth({0, 0}, {kMwmPointAccuracy, kMwmPointAccuracy}) /
                 mercator::DistanceOnEarth(mercator::FromLatLon(55.7793100, 37.3699100),
                                           mercator::FromLatLon(55.7793300, 37.3699300));
  epsilon = math::Clamp(epsilon, 0.0, 1.0);
  CameraMap const answer = {
      {SegmentCoord(0, 0), std::vector<RouteSegment::SpeedCamera>{{0.5, 100}}}};
  TestSpeedCameraSectionBuilding(osmContent, answer, epsilon);
}

UNIT_TEST(SpeedCameraGenerationTest_CameraIsNearFeature_2)
{
  string const osmContent = R"(
 <osm version="0.6" generator="osmconvert 0.8.4" timestamp="2018-07-16T02:00:00Z">
    <node id="10000001" lat="55.7793100" lon="37.3699100" version="1"></node>
    <node id="10000002" lat="55.7793150" lon="37.3699150" version="1">
      <tag k="highway" v="speed_camera"/>
      <tag k="maxspeed" v="100"/>
    </node>
    <node id="10000003" lat="55.7793300" lon="37.3699300" version="1"></node>

    <way id="2000000" version="1">
      <nd ref="10000001"/>
      <nd ref="10000003"/>
      <tag k="highway" v="primary"/>
    </way>

  </osm>
  )";

  // Geometry:
  // Feature number 0: <node>--------<node>
  //                           ^___ somewhere here camera, but it is not the point of segment.
  //                                Coef was calculated by this program and checked by eye.
  // Result:
  // {(0, 0), (0.25, 100)} - featureId - 0, segmentId - 0,
  //                         coef - position on segment - 0.25,
  //                         maxSpeed - 100.
  auto epsilon = mercator::DistanceOnEarth({0, 0}, {kMwmPointAccuracy, kMwmPointAccuracy}) /
                 mercator::DistanceOnEarth(mercator::FromLatLon(55.7793100, 37.3699100),
                                           mercator::FromLatLon(55.7793300, 37.3699300));
  epsilon = math::Clamp(epsilon, 0.0, 1.0);

  CameraMap const answer = {
    {SegmentCoord(0, 0), std::vector<RouteSegment::SpeedCamera>{{0.25, 100}}}
  };
  TestSpeedCameraSectionBuilding(osmContent, answer, epsilon);
}

UNIT_TEST(RoadCategoryToSpeedTest)
{
  SpeedInUnits speed;

  TEST(RoadCategoryToSpeed("RU:rural", speed), ());
  TEST_EQUAL(speed, SpeedInUnits(90, Units::Metric), ());
  TEST(speed.IsNumeric(), ());

  TEST(RoadCategoryToSpeed("DE:motorway", speed), ());
  TEST_EQUAL(speed, SpeedInUnits(kNoneMaxSpeed, Units::Metric), ());
  TEST(!speed.IsNumeric(), ());

  TEST(RoadCategoryToSpeed("UK:motorway", speed), ());
  TEST_EQUAL(speed, SpeedInUnits(70, Units::Imperial), ());
  TEST(speed.IsNumeric(), ());

  TEST(!RoadCategoryToSpeed("UNKNOWN:unknown", speed), ());
}
}  // namespace speed_cameras_test
