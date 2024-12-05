#include "geometry/screenbase.hpp"
#include "geometry/angles.hpp"
#include "geometry/transformations.hpp"

#include "base/assert.hpp"

#include <cmath>

double constexpr kPerspectiveAngleFOV = math::pi / 3.0;
double constexpr kMaxPerspectiveAngle1 = math::pi4;
double constexpr kMaxPerspectiveAngle2 = math::pi * 55.0 / 180.0;

double constexpr kStartPerspectiveScale1 = 1.7e-5;
double constexpr kEndPerspectiveScale1 = 0.3e-5;
double constexpr kEndPerspectiveScale2 = 0.13e-5;

ScreenBase::ScreenBase()
  : m_Org(320, 240)
  , m_GlobalRect(m_Org, ang::AngleD(0), m2::RectD(-320, -240, 320, 240))
  , m_ClipRect(m_GlobalRect.GetGlobalRect())
  , m_ViewportRect(0, 0, 640, 480)
  , m_PixelRect(m_ViewportRect)
  , m_Scale(0.1)
  , m_Angle(0.0)
  , m_3dFOV(kPerspectiveAngleFOV)
  , m_3dNearZ(0.1)
  , m_3dFarZ(0.0)
  , m_3dAngleX(0.0)
  , m_3dMaxAngleX(0.0)
  , m_3dScale(1.0)
  , m_isPerspective(false)
  , m_isAutoPerspective(false)
{
  m_GtoP = math::Identity<double, 3>();
  m_PtoG = math::Identity<double, 3>();
}

ScreenBase::ScreenBase(m2::RectI const & pxRect, m2::AnyRectD const & glbRect)
{
  OnSize(pxRect);
  SetFromRect(glbRect);
}

ScreenBase::ScreenBase(ScreenBase const & s, m2::PointD const & org, double scale, double angle)
  : m_Org(org), m_ViewportRect(s.m_ViewportRect), m_Scale(scale), m_Angle(angle)
{
  ASSERT_GREATER(m_Scale, 0.0, ());
  UpdateDependentParameters();
}

void ScreenBase::UpdateDependentParameters()
{
  m_PixelRect = CalculatePixelRect(m_Scale);

  m_PtoG = math::Shift(     /// 5. shifting on (E0, N0)
      math::Rotate(         /// 4. rotating on the screen angle
          math::Scale(      /// 3. scaling to translate pixel sizes to global
              math::Scale(  /// 2. swapping the Y axis??? why??? supposed to be a rotation on -pi /
                            /// 2 here.
                  math::Shift(  /// 1. shifting for the pixel center to become (0, 0)
                      math::Identity<double, 3>(), -m_PixelRect.Center()),
                  1, -1),
              m_Scale, m_Scale),
          m_Angle.cos(), m_Angle.sin()),
      m_Org);

  m_GtoP = math::Inverse(m_PtoG);

  m2::PointD const pxC = m_PixelRect.Center();
  m2::PointD const glbC = PtoG(pxC);
  double const szX = PtoG(m2::PointD(m_PixelRect.maxX(), pxC.y)).Length(glbC);
  double const szY = PtoG(m2::PointD(pxC.x, m_PixelRect.minY())).Length(glbC);

  m_GlobalRect = m2::AnyRectD(m_Org, m_Angle, m2::RectD(-szX, -szY, szX, szY));
  m_ClipRect = m_GlobalRect.GetGlobalRect();

  double constexpr kEps = 1.0E-5;
  double const angle = CalculatePerspectiveAngle(m_Scale);
  m_isPerspective = angle > 0.0;
  if (std::fabs(angle - m_3dAngleX) > kEps)
  {
    m_3dMaxAngleX = angle;
    m_3dScale = CalculateScale3d(angle);
    SetRotationAngle(angle);
  }
}

// static
double ScreenBase::CalculateAutoPerspectiveAngle(double scale)
{
  if (scale > kStartPerspectiveScale1)
    return 0.0;

  if (scale > kEndPerspectiveScale1)
  {
    double const k =
        (kStartPerspectiveScale1 - scale) / (kStartPerspectiveScale1 - kEndPerspectiveScale1);
    return kMaxPerspectiveAngle1 * k;
  }

  if (scale > kEndPerspectiveScale2)
  {
    double const k =
        (kEndPerspectiveScale1 - scale) / (kEndPerspectiveScale1 - kEndPerspectiveScale2);
    return kMaxPerspectiveAngle1 + (kMaxPerspectiveAngle2 - kMaxPerspectiveAngle1) * k;
  }

  return kMaxPerspectiveAngle2 * 0.99;
}

// static
double ScreenBase::GetStartPerspectiveScale() { return kStartPerspectiveScale1; }

double ScreenBase::CalculatePerspectiveAngle(double scale) const
{
  if (!m_isAutoPerspective)
    return m_3dAngleX;

  return CalculateAutoPerspectiveAngle(scale);
}

void ScreenBase::SetAutoPerspective(bool isAutoPerspective)
{
  m_isAutoPerspective = isAutoPerspective;
  UpdateDependentParameters();
}

void ScreenBase::SetFromRects(m2::AnyRectD const & glbRect, m2::RectD const & pxRect)
{
  m2::RectD const & lRect = glbRect.GetLocalRect();
  ASSERT(lRect.IsValid(), (lRect));
  ASSERT(!pxRect.IsEmptyInterior(), (pxRect));

  double const hScale = lRect.SizeX() / pxRect.SizeX();
  double const vScale = lRect.SizeY() / pxRect.SizeY();
  m_Scale = std::max(hScale, vScale);
  ASSERT_GREATER(m_Scale, 0.0, ());

  m_Angle = glbRect.Angle();
  m_Org = glbRect.GlobalCenter();

  UpdateDependentParameters();
}

void ScreenBase::SetFromRect(m2::AnyRectD const & glbRect) { SetFromRects(glbRect, m_PixelRect); }

void ScreenBase::SetFromParams(m2::PointD const & org, double angle, double scale)
{
  ASSERT_GREATER(scale, 0.0, ());
  m_Scale = scale;
  m_Angle = ang::Angle<double>(angle);
  m_Org = org;
  UpdateDependentParameters();
}

void ScreenBase::MatchGandP(m2::PointD const & g, m2::PointD const & p)
{
  m2::PointD g_current = PtoG(p);
  SetOrg(m_Org - g_current + g);
}

void ScreenBase::MatchGandP3d(m2::PointD const & g, m2::PointD const & p3d)
{
  m2::PointD g_current = PtoG(P3dtoP(p3d));
  SetOrg(m_Org - g_current + g);
}

void ScreenBase::SetOrg(m2::PointD const & p)
{
  m_Org = p;
  UpdateDependentParameters();
}

void ScreenBase::Move(double dx, double dy)
{
  m_Org = PtoG(GtoP(m_Org) - m2::PointD(dx, dy));
  UpdateDependentParameters();
}

void ScreenBase::MoveG(m2::PointD const & p)
{
  m_Org -= p;
  UpdateDependentParameters();
}

void ScreenBase::Scale(double scale)
{
  ASSERT_GREATER(scale, 0, ());
  m_Scale /= scale;
  ASSERT_GREATER(m_Scale, 0.0, ());
  UpdateDependentParameters();
}

void ScreenBase::Rotate(double angle)
{
  m_Angle = ang::AngleD(m_Angle.val() + angle);
  UpdateDependentParameters();
}

void ScreenBase::OnSize(m2::RectI const & r)
{
  m_ViewportRect = m2::RectD(r);
  UpdateDependentParameters();
}

void ScreenBase::OnSize(int x0, int y0, int w, int h) { OnSize(m2::RectI(x0, y0, x0 + w, y0 + h)); }

void ScreenBase::SetScale(double scale)
{
  ASSERT_GREATER(scale, 0.0, ());
  m_Scale = scale;
  UpdateDependentParameters();
}

void ScreenBase::SetAngle(double angle)
{
  m_Angle = ang::AngleD(angle);
  UpdateDependentParameters();
}

int ScreenBase::GetWidth() const { return math::SignedRound(m_PixelRect.SizeX()); }

int ScreenBase::GetHeight() const { return math::SignedRound(m_PixelRect.SizeY()); }

ScreenBase::MatrixT ScreenBase::CalcTransform(m2::PointD const & oldPt1,
                                                    m2::PointD const & oldPt2,
                                                    m2::PointD const & newPt1,
                                                    m2::PointD const & newPt2, bool allowRotate)
{
  double const s = newPt1.Length(newPt2) / oldPt1.Length(oldPt2);
  double const a = allowRotate ? ang::AngleTo(newPt1, newPt2) - ang::AngleTo(oldPt1, oldPt2) : 0.0;

  MatrixT m = math::Shift(
      math::Scale(math::Rotate(math::Shift(math::Identity<double, 3>(), -oldPt1.x, -oldPt1.y), a),
                  s, s),
      newPt1.x, newPt1.y);
  return m;
}

void ScreenBase::SetGtoPMatrix(MatrixT const & m)
{
  m_GtoP = m;
  m_PtoG = math::Inverse(m_GtoP);
  /// Extracting transformation params, assuming that the matrix
  /// somehow represent a valid screen transformation
  /// into m_PixelRectangle
  double dx, dy, a, s;
  ExtractGtoPParams(m, a, s, dx, dy);
  m_Angle = ang::AngleD(-a);
  ASSERT_GREATER(s, 0.0, ());
  m_Scale = 1 / s;
  ASSERT_GREATER(m_Scale, 0.0, ());
  m_Org = PtoG(m_PixelRect.Center());

  UpdateDependentParameters();
}

void ScreenBase::GtoP(m2::RectD const & glbRect, m2::RectD & pxRect) const
{
  pxRect = m2::RectD(GtoP(glbRect.LeftTop()), GtoP(glbRect.RightBottom()));
}

void ScreenBase::PtoG(m2::RectD const & pxRect, m2::RectD & glbRect) const
{
  glbRect = m2::RectD(PtoG(pxRect.LeftTop()), PtoG(pxRect.RightBottom()));
}

void ScreenBase::GetTouchRect(m2::PointD const & pixPoint, double pixRadius,
                              m2::AnyRectD & glbRect) const
{
  double const r = pixRadius * m_Scale;
  glbRect = m2::AnyRectD(PtoG(pixPoint), m_Angle, m2::RectD(-r, -r, r, r));
}

void ScreenBase::GetTouchRect(m2::PointD const & pixPoint, double const pxWidth,
                              double const pxHeight, m2::AnyRectD & glbRect) const
{
  double const width = pxWidth * m_Scale;
  double const height = pxHeight * m_Scale;
  glbRect = m2::AnyRectD(PtoG(pixPoint), m_Angle, m2::RectD(-width, -height, width, height));
}

bool IsPanningAndRotate(ScreenBase const & s1, ScreenBase const & s2)
{
  m2::RectD const & r1 = s1.GlobalRect().GetLocalRect();
  m2::RectD const & r2 = s2.GlobalRect().GetLocalRect();

  m2::PointD c1 = r1.Center();
  m2::PointD c2 = r2.Center();

  m2::PointD globPt(c1.x - r1.minX(), c1.y - r1.minY());

  m2::PointD p1 =
      s1.GtoP(s1.GlobalRect().ConvertFrom(c1)) - s1.GtoP(s1.GlobalRect().ConvertFrom(c1 + globPt));
  m2::PointD p2 =
      s2.GtoP(s2.GlobalRect().ConvertFrom(c2)) - s2.GtoP(s2.GlobalRect().ConvertFrom(c2 + globPt));

  return p1.EqualDxDy(p2, 0.00001);
}

void ScreenBase::ExtractGtoPParams(MatrixT const & m, double & a, double & s, double & dx,
                                   double & dy)
{
  s = sqrt(m(0, 0) * m(0, 0) + m(0, 1) * m(0, 1));

  a = ang::AngleIn2PI(atan2(-m(0, 1), m(0, 0)));

  dx = m(2, 0);
  dy = m(2, 1);
}

double ScreenBase::CalculateScale3d(double rotationAngle) const
{
  double const halfFOV = m_3dFOV / 2.0;
  double const cameraZ = 1.0 / tan(halfFOV);

  // Ratio of the expanded plane's size to the original size.
  double const y3dScale = cos(rotationAngle) + sin(rotationAngle) * tan(halfFOV + rotationAngle);
  double const x3dScale =
      1.0 + 2 * sin(rotationAngle) * cos(halfFOV) / (cameraZ * cos(halfFOV + rotationAngle));

  return std::max(x3dScale, y3dScale);
}

m2::RectD ScreenBase::CalculatePixelRect(double scale) const
{
  double const angle = CalculatePerspectiveAngle(scale);
  if (angle > 0.0)
  {
    double const scale3d = CalculateScale3d(angle);

    return m2::RectD(m2::PointD(0.0, 0.0),
                     m2::PointD(m_ViewportRect.maxX(), m_ViewportRect.maxY()) * scale3d);
  }

  return m_ViewportRect;
}

// Place the camera at the distance, where it gives the same view of plane as the
// orthogonal projection does. Calculate what part of the map would be visible,
// when it is rotated through maxRotationAngle around its near horizontal side.
void ScreenBase::ApplyPerspective(double currentRotationAngle, double maxRotationAngle,
                                  double angleFOV)
{
  ASSERT_GREATER(angleFOV, 0.0, ());
  ASSERT_LESS(angleFOV, math::pi2, ());
  ASSERT_GREATER_OR_EQUAL(maxRotationAngle, 0.0, ());
  ASSERT_LESS(maxRotationAngle, math::pi2, ());

  m_isPerspective = true;
  m_isAutoPerspective = false;

  m_3dMaxAngleX = maxRotationAngle;
  m_3dAngleX = currentRotationAngle;
  m_3dFOV = angleFOV;

  double const old_dy = m_ViewportRect.SizeY() * (m_3dScale - 1.0);

  m_3dScale = CalculateScale3d(m_3dMaxAngleX);
  double const new_dy = m_ViewportRect.SizeY() * (m_3dScale - 1.0);

  SetRotationAngle(currentRotationAngle);

  Move(0.0, (new_dy - old_dy) / 2.0);
}

// Place the camera at the distance, where it gives the same view of plane as the
// orthogonal projection does and rotate the map plane around its near horizontal side.
void ScreenBase::SetRotationAngle(double rotationAngle)
{
  ASSERT(rotationAngle == 0.0 || (rotationAngle > 0.0 && m_isPerspective), ());
  ASSERT_GREATER_OR_EQUAL(rotationAngle, 0.0, ());
  ASSERT_LESS_OR_EQUAL(rotationAngle, m_3dMaxAngleX, ());

  if (rotationAngle > m_3dMaxAngleX)
    rotationAngle = m_3dMaxAngleX;

  m_3dAngleX = rotationAngle;

  double const halfFOV = m_3dFOV / 2.0;
  double const cameraZ = 1.0 / tan(halfFOV);

  double const offsetZ = cameraZ + sin(m_3dAngleX) * m_3dScale;
  double const offsetY = cos(m_3dAngleX) * m_3dScale - 1.0;

  Matrix3dT scaleM = math::Identity<double, 4>();
  scaleM(0, 0) = m_3dScale;
  scaleM(1, 1) = m_3dScale;

  Matrix3dT rotateM = math::Identity<double, 4>();
  rotateM(1, 1) = cos(m_3dAngleX);
  rotateM(1, 2) = sin(m_3dAngleX);
  rotateM(2, 1) = -sin(m_3dAngleX);
  rotateM(2, 2) = cos(m_3dAngleX);

  Matrix3dT translateM = math::Identity<double, 4>();
  translateM(3, 1) = offsetY;
  translateM(3, 2) = offsetZ;

  Matrix3dT projectionM = math::Zero<double, 4>();
  m_3dFarZ = cameraZ + 2.0 * sin(m_3dAngleX) * m_3dScale;
  projectionM(0, 0) = projectionM(1, 1) = cameraZ;
  projectionM(2, 2) = m_3dAngleX != 0.0 ? (m_3dFarZ + m_3dNearZ) / (m_3dFarZ - m_3dNearZ) : 0.0;
  projectionM(2, 3) = 1.0;
  projectionM(3, 2) =
      m_3dAngleX != 0.0 ? -2.0 * m_3dFarZ * m_3dNearZ / (m_3dFarZ - m_3dNearZ) : 0.0;

  m_Pto3d = scaleM * rotateM * translateM * projectionM;
  m_3dtoP = math::Inverse(m_Pto3d);
}

void ScreenBase::ResetPerspective()
{
  m_isPerspective = false;
  m_isAutoPerspective = false;

  double const old_dy = m_ViewportRect.SizeY() * (m_3dScale - 1.0);

  m_3dScale = 1.0;
  m_3dAngleX = 0.0;
  m_3dMaxAngleX = 0.0;

  Move(0.0, -old_dy / 2.0);
}

m2::PointD ScreenBase::PtoP3d(m2::PointD const & pt) const { return PtoP3d(pt, 0.0); }

double ScreenBase::GetZScale() const
{
  double const averageScale3d = m_isPerspective ? 2.7 : 1.0;
  return 2.0 / (m_Scale * m_ViewportRect.SizeY() * averageScale3d);
}

m2::PointD ScreenBase::PtoP3d(m2::PointD const & pt, double ptZ) const
{
  if (!m_isPerspective)
    return pt;
  Vector3dT const normalizedPoint{float(2.0 * pt.x / m_PixelRect.SizeX() - 1.0),
                                  -float(2.0 * pt.y / m_PixelRect.SizeY() - 1.0),
                                  float(ptZ * GetZScale()), 1.0};

  Vector3dT const perspectivePoint = normalizedPoint * m_Pto3d;

  m2::RectD const viewport = PixelRectIn3d();
  m2::PointD const pixelPointPerspective(
      (perspectivePoint(0, 0) / perspectivePoint(0, 3) + 1.0) * viewport.SizeX() / 2.0,
      (-perspectivePoint(0, 1) / perspectivePoint(0, 3) + 1.0) * viewport.SizeY() / 2.0);

  return pixelPointPerspective;
}

m2::PointD ScreenBase::P3dtoP(m2::PointD const & pt) const
{
  return P3dtoP(pt, 0.0 /* ptZ */);
}

m2::PointD ScreenBase::P3dtoP(m2::PointD const & pt, double ptZ) const
{
  if (!m_isPerspective)
    return pt;

  double const normalizedX = 2.0 * pt.x / PixelRectIn3d().SizeX() - 1.0;
  double const normalizedY = -2.0 * pt.y / PixelRectIn3d().SizeY() + 1.0;

  double normalizedZ = 0.0;

  if (m_3dAngleX != 0.0)
  {
    double const halfFOV = m_3dFOV / 2.0;
    double const cameraZ = 1.0 / tan(halfFOV);

    double const tanX = tan(m_3dAngleX);
    double cameraDistanceZ = cameraZ * (1.0 + (normalizedY + 1.0) * tanX / (cameraZ - normalizedY * tanX));

    if (ptZ != 0.0)
    {
      double const t = sqrt(cameraZ * cameraZ + normalizedY * normalizedY);
      double const cosBeta = cameraZ / t;
      double const sinBeta = normalizedY / t;
      double const dz = ptZ * GetZScale() * cosBeta / (cos(m_3dAngleX) * cosBeta - sin(m_3dAngleX) * sinBeta);

      cameraDistanceZ -= dz;
    }

    double const a = (m_3dFarZ + m_3dNearZ) / (m_3dFarZ - m_3dNearZ);
    double const b = -2.0 * m_3dFarZ * m_3dNearZ / (m_3dFarZ - m_3dNearZ);
    normalizedZ = a + b / cameraDistanceZ;
  }

  Vector3dT const normalizedPoint{normalizedX, normalizedY, normalizedZ, 1.0};

  Vector3dT const originalPoint = normalizedPoint * m_3dtoP;

  m2::PointD const pixelPointOriginal =
      m2::PointD((originalPoint(0, 0) / originalPoint(0, 3) + 1.0) * PixelRect().SizeX() / 2.0,
                 (-originalPoint(0, 1) / originalPoint(0, 3) + 1.0) * PixelRect().SizeY() / 2.0);

  return pixelPointOriginal;
}

bool ScreenBase::IsReverseProjection3d(m2::PointD const & pt) const
{
  if (!m_isPerspective)
    return false;

  Vector3dT const normalizedPoint{float(2.0 * pt.x / m_PixelRect.SizeX() - 1.0),
                                  -float(2.0 * pt.y / m_PixelRect.SizeY() - 1.0), 0.0, 1.0};

  Vector3dT const perspectivePoint = normalizedPoint * m_Pto3d;
  return perspectivePoint(0, 3) < 0.0;
}

ScreenBase::Matrix3dT ScreenBase::GetModelView() const
{
  return ScreenBase::Matrix3dT
  {
    m_GtoP(0, 0), m_GtoP(1, 0), 0, m_GtoP(2, 0),
    m_GtoP(0, 1), m_GtoP(1, 1), 0, m_GtoP(2, 1),
    0, 0, 1, 0,
    0, 0, 0, 1
  };
}

ScreenBase::Matrix3dT ScreenBase::GetModelView(m2::PointD const & pivot, double scalar) const
{
  MatrixT const & m = m_GtoP;
  double const s = 1.0 / scalar;
  return ScreenBase::Matrix3dT
  {
    s * m(0, 0), s * m(1, 0), 0, m(2, 0) + pivot.x * m(0, 0) + pivot.y * m(1, 0),
    s * m(0, 1), s * m(1, 1), 0, m(2, 1) + pivot.x * m(0, 1) + pivot.y * m(1, 1),
    0, 0, 1, 0,
    0, 0, 0, 1
  };
}
