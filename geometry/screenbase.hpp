#pragma once

#include "geometry/any_rect2d.hpp"
#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include "base/matrix.hpp"

class ScreenBase
{
public:
  using MatrixT = math::Matrix<double, 3, 3>;
  using Matrix3dT = math::Matrix<double, 4, 4>;
  using Vector3dT = math::Matrix<double, 1, 4>;

  ScreenBase();
  ScreenBase(m2::RectI const & pxRect, m2::AnyRectD const & glbRect);
  ScreenBase(ScreenBase const & s, m2::PointD const & org, double scale, double angle);

  void SetFromRect(m2::AnyRectD const & rect);

  void SetFromParams(m2::PointD const & org, double angle, double scale);
  void SetFromRects(m2::AnyRectD const & glbRect, m2::RectD const & pxRect);
  void SetOrg(m2::PointD const & p);

  void Move(double dx, double dy);
  void MoveG(m2::PointD const & p);

  /// scale global rect
  void Scale(double scale);
  void Rotate(double angle);

  void OnSize(m2::RectI const & r);
  void OnSize(int x0, int y0, int w, int h);

  double GetScale() const
  {
    ASSERT_GREATER(m_Scale, 0.0, ());
    return m_Scale;
  }
  void SetScale(double scale);

  double GetAngle() const { return m_Angle.val(); }
  void SetAngle(double angle);

  m2::PointD const & GetOrg() const { return m_Org; }

  int GetWidth() const;
  int GetHeight() const;

  m2::PointD GtoP(m2::PointD const & pt) const { return pt * m_GtoP; }

  m2::PointD PtoG(m2::PointD const & pt) const { return pt * m_PtoG; }

  void GtoP(double & x, double & y) const
  {
    double tempX = x;
    x = tempX * m_GtoP(0, 0) + y * m_GtoP(1, 0) + m_GtoP(2, 0);
    y = tempX * m_GtoP(1, 0) + y * m_GtoP(1, 1) + m_GtoP(2, 1);
  }

  void PtoG(double & x, double & y) const
  {
    double const tempX = x;
    x = tempX * m_PtoG(0, 0) + y * m_PtoG(1, 0) + m_PtoG(2, 0);
    y = tempX * m_PtoG(0, 1) + y * m_PtoG(1, 1) + m_PtoG(2, 1);
  }

  void GtoP(m2::RectD const & gr, m2::RectD & sr) const;
  void PtoG(m2::RectD const & pr, m2::RectD & gr) const;

  void MatchGandP(m2::PointD const & g, m2::PointD const & p);
  void MatchGandP3d(m2::PointD const & g, m2::PointD const & p3d);

  void GetTouchRect(m2::PointD const & pixPoint, double pixRadius, m2::AnyRectD & glbRect) const;
  void GetTouchRect(m2::PointD const & pixPoint, double const pxWidth, double const pxHeight,
                    m2::AnyRectD & glbRect) const;

  MatrixT const & GtoPMatrix() const { return m_GtoP; }
  MatrixT const & PtoGMatrix() const { return m_PtoG; }

  m2::RectD const & PixelRect() const { return m_PixelRect; }
  m2::AnyRectD const & GlobalRect() const { return m_GlobalRect; }
  m2::RectD const & ClipRect() const { return m_ClipRect; }

  void ApplyPerspective(double currentRotationAngle, double maxRotationAngle, double angleFOV);
  void ResetPerspective();

  void SetRotationAngle(double rotationAngle);
  double GetRotationAngle() const { return m_3dAngleX; }
  double GetMaxRotationAngle() const { return m_3dMaxAngleX; }
  double GetAngleFOV() const { return m_3dFOV; }
  double GetScale3d() const { return m_3dScale; }
  double GetZScale() const;

  double GetDepth3d() const { return m_3dFarZ - m_3dNearZ; }

  m2::PointD P3dtoP(m2::PointD const & pt) const;
  m2::PointD P3dtoP(m2::PointD const & pt, double ptZ) const;

  Matrix3dT const & Pto3dMatrix() const { return m_Pto3d; }
  bool isPerspective() const { return m_isPerspective; }
  bool isAutoPerspective() const { return m_isAutoPerspective; }
  void SetAutoPerspective(bool isAutoPerspective);

  bool IsReverseProjection3d(m2::PointD const & pt) const;

  m2::PointD PtoP3d(m2::PointD const & pt) const;
  m2::PointD PtoP3d(m2::PointD const & pt, double ptZ) const;

  m2::RectD const & PixelRectIn3d() const { return m_ViewportRect; }

  double CalculateScale3d(double rotationAngle) const;
  m2::RectD CalculatePixelRect(double scale) const;
  double CalculatePerspectiveAngle(double scale) const;

  Matrix3dT GetModelView() const;
  Matrix3dT GetModelView(m2::PointD const & pivot, double scalar) const;

  static double CalculateAutoPerspectiveAngle(double scale);
  static double GetStartPerspectiveScale();

  /// Compute arbitrary pixel transformation, that translates the (oldPt1, oldPt2) -> (newPt1,
  /// newPt2)
  static MatrixT CalcTransform(m2::PointD const & oldPt1, m2::PointD const & oldPt2,
                                     m2::PointD const & newPt1, m2::PointD const & newPt2,
                                     bool allowRotate);

  /// Setting GtoP matrix extracts the Angle and m_Org parameters, leaving PixelRect intact
  void SetGtoPMatrix(MatrixT const & m);

  /// Extracting parameters from matrix, that is supposed to represent GtoP transformation
  static void ExtractGtoPParams(MatrixT const & m, double & a, double & s, double & dx,
                                double & dy);

  bool operator!=(ScreenBase const & src) const { return !(*this == src); }

  bool operator==(ScreenBase const & src) const
  {
    return (m_GtoP == src.m_GtoP) && (m_PtoG == src.m_PtoG);
  }

private:
  // Used when initializing m_GlobalRect.
  m2::PointD m_Org;

protected:
  // Update dependent parameters from base parameters.
  // Must be called when base parameters changed.
  void UpdateDependentParameters();

  /// @group Dependent parameters
  /// Global to Pixel conversion matrix.
  ///             |a11 a12 0|
  /// |x, y, 1| * |a21 a22 0| = |x', y', 1|
  ///             |a31 a32 1|
  /// @{
  MatrixT m_GtoP;

  /// Pixel to Global conversion matrix. Inverted GtoP matrix.
  MatrixT m_PtoG;

  /// Global Rect
  m2::AnyRectD m_GlobalRect;

  /// X-axis aligned global rect used for clipping
  m2::RectD m_ClipRect;

  /// @}

  Matrix3dT m_Pto3d;
  Matrix3dT m_3dtoP;

private:
  m2::RectD m_ViewportRect;
  m2::RectD m_PixelRect;

  double m_Scale;
  ang::AngleD m_Angle;

  double m_3dFOV;
  double m_3dNearZ;
  double m_3dFarZ;
  double m_3dAngleX;
  double m_3dMaxAngleX;
  double m_3dScale;
  bool m_isPerspective;
  bool m_isAutoPerspective;
};

/// checking whether the s1 transforms into s2 without scaling, only with shift and rotation
bool IsPanningAndRotate(ScreenBase const & s1, ScreenBase const & s2);
