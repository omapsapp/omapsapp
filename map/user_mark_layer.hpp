#pragma once

#include "map/user_mark.hpp"

#include <base/macros.hpp>


class UserMarkLayer
{
public:
  explicit UserMarkLayer(UserMark::Type type);
  virtual ~UserMarkLayer() = default;

  bool IsDirty() const { return m_isDirty; }
  void ResetChanges();

  bool IsVisible() const;
  bool IsVisibilityChanged() const;
  UserMark::Type GetType() const;

  kml::MarkIdSet const & GetUserMarks() const { return m_userMarks; }
  kml::TrackIdSet const & GetUserLines() const { return m_tracks; }

  void AttachUserMark(kml::MarkId markId);
  void DetachUserMark(kml::MarkId markId);

  void AttachTrack(kml::TrackId trackId);
  void DetachTrack(kml::TrackId trackId);

  void Clear();
  bool IsEmpty() const;

  virtual void SetIsVisible(bool isVisible);

protected:
  virtual void SetDirty(bool /* updateModificationDate */ = true) { m_isDirty = true; }

  UserMark::Type m_type;

  kml::MarkIdSet m_userMarks;
  kml::TrackIdSet m_tracks;

  bool m_isDirty = true;
  bool m_isVisible = true;
  bool m_wasVisible = false;

  DISALLOW_COPY_AND_MOVE(UserMarkLayer);
};
