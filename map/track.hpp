#pragma once

#include "kml/types.hpp"

#include "map/elevation_info.hpp"

#include "drape_frontend/user_marks_provider.hpp"

#include <string>

class Track : public df::UserLineMark
{
  using Base = df::UserLineMark;
  using Lengths = std::vector<double>;

public:
  Track(kml::TrackData && data);

  kml::MarkGroupId GetGroupId() const override { return m_groupID; }

  bool IsDirty() const override { return m_isDirty; }
  void ResetChanges() const override { m_isDirty = false; }

  kml::TrackData const & GetData() const { return m_data; }
  void setData(kml::TrackData const & data);

  std::string GetName() const;
  void SetName(std::string const & name);
  std::string GetDescription() const;

  m2::RectD GetLimitRect() const;
  double GetLengthMeters() const;
  double GetDurationInSeconds() const;
  std::optional<ElevationInfo> GetElevationInfo() const;
  std::pair<m2::PointD, double> GetCenterPoint() const;

  struct TrackSelectionInfo
  {
    TrackSelectionInfo() = default;
    TrackSelectionInfo(kml::TrackId trackId, m2::PointD const & trackPoint, double distFromBegM)
      : m_trackId(trackId)
      , m_trackPoint(trackPoint)
      , m_distFromBegM(distFromBegM)
    {}

    kml::TrackId m_trackId = kml::kInvalidTrackId;
    m2::PointD m_trackPoint;
    // Distance in meters from the beginning to m_trackPoint.
    double m_distFromBegM;
    // Mercator square distance, used to select nearest track.
    double m_squareDist = std::numeric_limits<double>::max();
  };

  void UpdateSelectionInfo(m2::RectD const & touchRect, TrackSelectionInfo & info) const;

  int GetMinZoom() const override { return 1; }
  df::DepthLayer GetDepthLayer() const override;
  size_t GetLayerCount() const override;
  dp::Color GetColor(size_t layerIndex) const override;
  void SetColor(dp::Color color);
  float GetWidth(size_t layerIndex) const override;
  float GetDepth(size_t layerIndex) const override;
  void ForEachGeometry(GeometryFnT && fn) const override;

  void Attach(kml::MarkGroupId groupId);
  void Detach();

  bool GetPoint(double distanceInMeters, m2::PointD & pt) const;

  kml::MultiGeometry::LineT GetGeometry() const;
  bool HasAltitudes() const;

private:
  std::vector<Lengths> GetLengthsImpl() const;
  m2::RectD GetLimitRectImpl() const;

  void CacheDataForInteraction() const;

  double GetLengthMetersImpl(size_t lineIndex, size_t ptIdx) const;

  kml::TrackData m_data;
  kml::MarkGroupId m_groupID = kml::kInvalidMarkGroupId;
  mutable std::optional<ElevationInfo> m_elevationInfo;

  struct InteractionData
  {
    std::vector<Lengths> m_lengths;
    m2::RectD m_limitRect;
  };
  mutable std::optional<InteractionData> m_interactionData;

  mutable bool m_isDirty = true;
};
