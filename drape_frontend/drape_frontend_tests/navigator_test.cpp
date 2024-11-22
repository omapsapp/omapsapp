#include "testing/testing.hpp"

#include "drape_frontend/navigator.hpp"
#include "drape_frontend/visual_params.hpp"

#include "geometry/screenbase.hpp"

#include <cmath>

// -3 -2 -1  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
//
// 7      +-----------------------------------------------+
//        |                                               |
// 6      |                                               |
//        |                                               |
// 5      |                                               |
//        |                                               |
// 4      |  +-----------------------+                    |
//        |  |                       |                    |
// 3      |  |                       |                    |
//        |  |                       |                    |
// 2      |  |                       |                    |
//        |  |                       |                    |
// 1      |  |  a        B<-------b  |                    |
//        |  |                       |                    |
// 0      |  +-----------------------+                    |
//        |                                               |
//-1      +-----------------------------------------------+

UNIT_TEST(Navigator_Scale2Points)
{
  df::VisualParams::Init(1.0, 1024);
  df::Navigator navigator;

  navigator.OnSize(200, 100);
  navigator.SetFromRect(m2::AnyRectD(m2::RectD(0, 0, 8, 4)));

  ScreenBase const & screen = navigator.Screen();
  TEST_EQUAL(screen.ClipRect(), m2::RectD(0, 0, 8, 4), ());

  navigator.StartScale(screen.GtoP(m2::PointD(1, 1)),
                       screen.GtoP(m2::PointD(7, 1)));
  navigator.StopScale(screen.GtoP(m2::PointD(1, 1)),
                      screen.GtoP(m2::PointD(4, 1)));
  TEST_EQUAL(screen.ClipRect(), m2::RectD(-1, -1, 15, 7), ());
}

namespace
{
  void CheckNavigator(df::Navigator const & nav)
  {
    typedef m2::PointD P;
    m2::RectD clipR = nav.Screen().ClipRect();

    P arr[] = { clipR.LeftTop(), clipR.RightTop(),
                clipR.RightBottom(), clipR.LeftBottom(),
                clipR.Center() };

    for (size_t i = 0; i < ARRAY_SIZE(arr); ++i)
    {
      P const & pxP = arr[i];
      P const gP = nav.PtoG(pxP);
      P const pxP2 = nav.GtoP(gP);
      TEST(AlmostEqualAbs(pxP.x, pxP2.x, 0.00001), (pxP.x, pxP2.x));
      TEST(AlmostEqualAbs(pxP.y, pxP2.y, 0.00001), (pxP.y, pxP2.y));
    }
  }
}

UNIT_TEST(Navigator_G2P_P2G)
{
  df::VisualParams::Init(1.0, 1024);
  df::Navigator navigator;

  // Initialize.
  navigator.OnSize(200, 100);
  m2::PointD center = navigator.Screen().PixelRect().Center();
  navigator.SetFromRect(m2::AnyRectD(m2::RectD(0, 0, 8, 4)));
  TEST_EQUAL(navigator.Screen().ClipRect(), m2::RectD(0, 0, 8, 4), ());

  CheckNavigator(navigator);

  navigator.Scale(center, 3.0);
  CheckNavigator(navigator);

  navigator.Scale(center, 1/3.0);
  CheckNavigator(navigator);
}
