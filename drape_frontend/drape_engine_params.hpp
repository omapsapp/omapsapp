#pragma once

#include "geometry/point3d.hpp"

#include <string>

namespace df
{
// Declaration for custom Arrow3D object.
struct Arrow3dCustomDecl
{
  // Path to arrow mesh .OBJ file.
  std::string m_arrowMeshPath;
  // Path to arrow mesh texture .PNG file.
  // If it's empty, default arrow color is used.
  std::string m_arrowMeshTexturePath;
  // Path to shadow mesh .OBJ file.
  // If it's empty, no shadow or outline will be rendered.
  std::string m_shadowMeshPath;

  // Allows to load files from bundled resources.
  // You must use string identifiers of resources instead of file names.
  bool m_loadFromDefaultResourceFolder = false;

  // Layout of axes (in the plane of map): x - right, y - up,
  // -z - perpendicular to the map's plane directed towards the observer.

  // Offset is in local (model's) coordinates.
  m3::PointF m_offset = m3::PointF(0.0f, 0.0f, 0.0f);
  // Rotation angles.
  m3::PointF m_eulerAngles = m3::PointF(0.0f, 0.0f, 0.0f);
  // Scale values.
  m3::PointF m_scale = m3::PointF(1.0f, 1.0f, 1.0f);

  // Flip U texture coordinate.
  bool m_flipTexCoordU = false;
  // Flip V texture coordinate (enabled in the Drape by default).
  bool m_flipTexCoordV = true;

  // Enable shadow rendering (only in perspective mode).
  // Shadow mesh must exist, otherwise standard one will be used.
  bool m_enableShadow = false;
  // Enabled outline rendering (only in routing mode).
  // Shadow mesh must exist, otherwise standard one will be used.
  bool m_enableOutline = false;
};
}  // namespace df
