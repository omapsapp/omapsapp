#include "testing/testing.hpp"

#include "routing/checkpoint_predictor.hpp"

#include "geometry/point2d.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace checkpoint_predictor_test
{
void TestAlmostEqual(double v1, double v2)
{
  double constexpr kEpsMeters = 1.0;
  TEST(AlmostEqualAbs(v1, v2, kEpsMeters), (v1, v2));
}

using routing::CheckpointPredictor;

class CheckpointPredictorTest
{
public:
  CheckpointPredictorTest() : m_predictor({0.0, 0.0} /* start */, {4.0, 0.0} /* finish */) {}

  size_t PredictPosition(std::vector<m2::PointD> const & intermediatePoints, m2::PointD const & point) const
  {
    return m_predictor.PredictPosition(intermediatePoints, point);
  }

  CheckpointPredictor m_predictor;
};

UNIT_CLASS_TEST(CheckpointPredictorTest, CalculateDeltaMetersTest)
{
  TestAlmostEqual(CheckpointPredictor::CalculateDeltaMeters(
                      {0.0, 0.0} /* from */, {2.0, 0.0} /* to */, {1.0, 0.0} /* between */), 0.0);
  TestAlmostEqual(CheckpointPredictor::CalculateDeltaMeters(
                      {0.0, 0.0} /* from */, {2.0, 0.0} /* to */, {3.0, 0.0} /* between */), 222634.0);
  TestAlmostEqual(CheckpointPredictor::CalculateDeltaMeters(
                      {0.0, 0.0} /* from */, {2.0, 0.0} /* to */, {-1.0, 0.0} /* between */), 222634.0);
}

// Zero intermediate point test.
UNIT_CLASS_TEST(CheckpointPredictorTest, PredictPositionTest1)
{
  std::vector<m2::PointD> const intermediatePoints = {};

  TEST_EQUAL(PredictPosition(intermediatePoints, m2::PointD(-0.5, 0.5)), 0, ());
  TEST_EQUAL(PredictPosition(intermediatePoints, m2::PointD(1.5, -0.5)), 0, ());
  TEST_EQUAL(PredictPosition(intermediatePoints, m2::PointD(3.5, 0.7)), 0, ());
  TEST_EQUAL(PredictPosition(intermediatePoints, m2::PointD(5.0, 0.0)), 0, ());
}


// One intermediate point test.
UNIT_CLASS_TEST(CheckpointPredictorTest, PredictPositionTest2)
{
  std::vector<m2::PointD> const intermediatePoints = {{2.0, 0.0}};

  TEST_EQUAL(PredictPosition(intermediatePoints, m2::PointD(-0.5, 0.5)), 0, ());
  TEST_EQUAL(PredictPosition(intermediatePoints, m2::PointD(1.5, -0.5)), 0, ());
  TEST_EQUAL(PredictPosition(intermediatePoints, m2::PointD(3.5, 0.7)), 1, ());
  TEST_EQUAL(PredictPosition(intermediatePoints, m2::PointD(5.0, 0.0)), 1, ());
}

// Three intermediate points test.
UNIT_CLASS_TEST(CheckpointPredictorTest, PredictPositionTest3)
{
  std::vector<m2::PointD> const intermediatePoints = {{1.0, 0.0}, {2.0, 0.0}, {3.0, 0.0}};

  TEST_EQUAL(PredictPosition(intermediatePoints, m2::PointD(-0.5, 0.5)), 0, ());
  TEST_EQUAL(PredictPosition(intermediatePoints, m2::PointD(0.5, 0.5)), 0, ());
  TEST_EQUAL(PredictPosition(intermediatePoints, m2::PointD(1.5, -0.5)), 1, ());
  TEST_EQUAL(PredictPosition(intermediatePoints, m2::PointD(2.5, -0.5)), 2, ());
  TEST_EQUAL(PredictPosition(intermediatePoints, m2::PointD(3.5, 0.7)), 3, ());
  TEST_EQUAL(PredictPosition(intermediatePoints, m2::PointD(5.0, 0.0)), 3, ());
}
}  // namespace checkpoint_predictor_test
