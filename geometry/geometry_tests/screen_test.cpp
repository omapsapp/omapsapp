#include "geometry/geometry_tests/equality.hpp"

#include "geometry/screenbase.hpp"
#include "geometry/transformations.hpp"

#include "testing/testing.hpp"


using namespace test;

namespace
{
  void check_set_from_rect(ScreenBase & screen, int width, int height)
  {
    screen.OnSize(0, 0, width, height);

    m2::PointD b1(0.0, 0.0);
    m2::PointD b2(300.0, 300.0);
    screen.SetFromRect(m2::AnyRectD(m2::RectD(b1, b2)));

    b1 = screen.GtoP(b1);
    b2 = screen.GtoP(b2);

    // check that we are in boundaries.
    TEST(math::Between(0, width, math::SignedRound(b1.x)), ());
    TEST(math::Between(0, width, math::SignedRound(b2.x)), ());
    TEST(math::Between(0, height, math::SignedRound(b1.y)), ());
    TEST(math::Between(0, height, math::SignedRound(b2.y)), ());
  }
}

UNIT_TEST(ScreenBase_P2G2P)
{
  ScreenBase screen;

  check_set_from_rect(screen, 1000, 500);
  check_set_from_rect(screen, 500, 1000);

  screen.OnSize(0, 0, 640, 480);
  screen.SetFromRect(m2::AnyRectD(m2::RectD(-100, -200, 500, 680)));

  // checking that PtoG(GtoP(p)) == p

  m2::PointD pp(10.0, 20.0);
  m2::PointD pg = screen.PtoG(pp);
  TEST(is_equal(pp, screen.GtoP(pg)), ());

  pg = m2::PointD(550, 440);
  pp = screen.GtoP(pg);
  TEST(is_equal(pg, screen.PtoG(pp)), ());
}

UNIT_TEST(ScreenBase_3dTransform)
{
  ScreenBase screen;

  double const rotationAngle = math::pi4;

  screen.SetFromRects(m2::AnyRectD(m2::RectD(50, 25, 55, 30)), m2::RectD(0, 0, 200, 400));
  screen.ApplyPerspective(rotationAngle, rotationAngle, math::pi / 3.0);

  TEST(screen.PixelRectIn3d().SizeX() < screen.PixelRect().SizeX(), ());
  TEST(screen.PixelRectIn3d().SizeY() < screen.PixelRect().SizeY(), ());

  double const kEps = 1.0e-3;

  m2::PointD pp(screen.PixelRect().SizeX() / 2.0, screen.PixelRect().SizeY());
  m2::PointD p3d = screen.PtoP3d(pp);
  TEST(p3d.EqualDxDy(m2::PointD(screen.PixelRectIn3d().SizeX() / 2.0, screen.PixelRectIn3d().SizeY()), kEps), ());

  p3d = m2::PointD(screen.PixelRectIn3d().SizeX() / 2.0, screen.PixelRectIn3d().SizeY() / 2.0);
  pp = screen.P3dtoP(p3d);
  TEST(pp.EqualDxDy(m2::PointD(screen.PixelRect().SizeX() / 2.0,
                               screen.PixelRect().SizeY() - screen.PixelRectIn3d().SizeY() / (2.0 * cos(rotationAngle))), kEps), ());

  p3d = m2::PointD(0, 0);
  pp = screen.P3dtoP(p3d);
  TEST(pp.x < kEps, ());

  p3d = m2::PointD(screen.PixelRectIn3d().SizeX(), 0);
  pp = screen.P3dtoP(p3d);
  TEST(fabs(pp.x - screen.PixelRect().maxX()) < kEps, ());
}

UNIT_TEST(ScreenBase_P2P3d2P)
{
  ScreenBase screen;

  screen.SetFromRects(m2::AnyRectD(m2::RectD(50, 25, 55, 30)), m2::RectD(0, 0, 600, 400));
  screen.ApplyPerspective(math::pi4, math::pi4, math::pi / 3.0);

  double const kEps = 1.0e-3;

  // checking that P3dtoP(PtoP3d(p)) == p
  m2::PointD pp(500, 300);
  m2::PointD p3d = screen.PtoP3d(pp);
  TEST(pp.EqualDxDy(screen.P3dtoP(p3d), kEps), ());

  // checking that PtoP3(P3dtoP(p)) == p
  p3d = m2::PointD(400, 300);
  pp = screen.P3dtoP(p3d);
  TEST(p3d.EqualDxDy(screen.PtoP3d(pp), kEps), ());
}

UNIT_TEST(ScreenBase_AxisOrientation)
{
  ScreenBase screen;

  screen.OnSize(0, 0, 300, 200);
  screen.SetFromRect(m2::AnyRectD(m2::RectD(0, 0, 300, 200)));

  TEST(is_equal(m2::PointD(150, 100), screen.GtoP(m2::PointD(150, 100))), ());
  TEST(is_equal(m2::PointD(0, 0), screen.GtoP(m2::PointD(0, 200))), ());
  TEST(is_equal(m2::PointD(300, 0), screen.GtoP(m2::PointD(300, 200))), ());
  TEST(is_equal(m2::PointD(300, 200), screen.GtoP(m2::PointD(300, 0))), ());
  TEST(is_equal(m2::PointD(0, 200), screen.GtoP(m2::PointD(0, 0))), ());
}

UNIT_TEST(ScreenBase_X0Y0)
{
  ScreenBase screen;
  screen.OnSize(10, 10, 300, 200);
  screen.SetFromRect(m2::AnyRectD(m2::RectD(0, 0, 300, 200)));

  TEST(is_equal(m2::PointD(10, 210), screen.GtoP(m2::PointD(0, 0))), ());
}

UNIT_TEST(ScreenBase_ChoosingMaxScale)
{
  ScreenBase screen;
  screen.OnSize(10, 10, 300, 200);
  screen.SetFromRect(m2::AnyRectD(m2::RectD(0, 0, 200, 400)));

  TEST(is_equal(screen.GtoP(m2::PointD(100, 200)), m2::PointD(160, 110)), ());
  TEST(is_equal(screen.GtoP(m2::PointD(0, 0)), m2::PointD(110, 210)), ());
  TEST(is_equal(screen.GtoP(m2::PointD(200, 0)), m2::PointD(210, 210)), ());
  TEST(is_equal(screen.GtoP(m2::PointD(0, 400)), m2::PointD(110, 10)), ());
  TEST(is_equal(screen.GtoP(m2::PointD(200, 400)), m2::PointD(210, 10)), ());

  TEST(is_equal(screen.GtoP(m2::PointD(-200, 0)), m2::PointD(10, 210)), ());
}

UNIT_TEST(ScreenBase_CalcTransform)
{
  double s = 2;
  double a = sqrt(3.) / 2.0;
  double dx = 1;
  double dy = 2;
  double s1, a1, dx1, dy1;
  math::Matrix<double, 3, 3> m = ScreenBase::CalcTransform(
                                          m2::PointD(0, 1), m2::PointD(1, 1),
                                          m2::PointD(             s * sin(a) + dx,               s * cos(a) + dy),
                                          m2::PointD(s * cos(a) + s * sin(a) + dx, -s * sin(a) + s * cos(a) + dy),
                                          true /* allow rotate */);

  ScreenBase::ExtractGtoPParams(m, a1, s1, dx1, dy1);

  TEST(fabs(s - s1) < 0.00001, (s, s1));
  TEST(fabs(a - a1) < 0.00001, (a, a1));
  TEST(fabs(dx - dx1) < 0.00001, (dx, dx1));
  TEST(fabs(dy - dy1) < 0.00001, (dy, dy1));
}

UNIT_TEST(ScreenBase_Rotate)
{
  ScreenBase s;
  s.OnSize(0, 0, 100, 200);
  s.SetFromRect(m2::AnyRectD(m2::RectD(0, 0, 100, 200)));
  s.Rotate(math::pi / 4);

  TEST_EQUAL(s.PixelRect(), m2::RectD(0, 0, 100, 200), ());
}

UNIT_TEST(ScreenBase_CombineTransforms)
{
  ScreenBase s;
  s.OnSize(0, 0, 640, 480);
  s.SetFromRect(m2::AnyRectD(m2::RectD(50, 25, 55, 30)));
  s.SetAngle(1.0);

  m2::PointD g1(40, 50);
  m2::PointD g2(60, 70);

  m2::PointD p1 = s.GtoP(g1);
  m2::PointD p2 = s.GtoP(g2);

  m2::PointD const org = s.GtoP(m2::PointD(0, 0));
  double const angle = s.GetAngle();
  double const scale = s.GetScale();
  double const fixedScale = 666.666;

  ScreenBase sCopy(s, m2::PointD(0, 0), fixedScale, 0.0);

  {
    // GtoP matrix for scale only.
    math::Matrix<double, 3, 3> m =
        math::Shift(
          math::Scale(math::Identity<double, 3>(),
                      1.0 / fixedScale, -1.0 / fixedScale),
          sCopy.PixelRect().Center());

    TEST(is_equal(sCopy.GtoP(g1), g1 * m), ());
    TEST(is_equal(sCopy.GtoP(g2), g2 * m), ());
  }

  // GtoP matrix to make full final transformation.
  math::Matrix<double, 3, 3> m =
      math::Shift(
        math::Scale(
          math::Rotate(
            math::Shift(math::Identity<double, 3>(),
                        -sCopy.PixelRect().Center()),
            angle),
          fixedScale / scale, fixedScale / scale),
        org);

  m2::PointD pp1 = sCopy.GtoP(g1) * m;
  m2::PointD pp2 = sCopy.GtoP(g2) * m;

  TEST(is_equal(p1, pp1), (p1, pp1));
  TEST(is_equal(p2, pp2), (p2, pp2));
}
