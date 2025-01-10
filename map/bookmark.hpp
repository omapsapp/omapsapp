#pragma once

#include "map/user_mark.hpp"
#include "map/user_mark_layer.hpp"

#include "kml/types.hpp"

#include "search/reverse_geocoder.hpp"

#include <string>
#include <vector>

class Bookmark : public UserMark
{
  using Base = UserMark;
public:
  explicit Bookmark(m2::PointD const & ptOrg);

  explicit Bookmark(kml::BookmarkData && data);

  void SetData(kml::BookmarkData const & data);
  kml::BookmarkData const & GetData() const;

  search::ReverseGeocoder::RegionAddress const & GetAddress() const;
  void SetAddress(search::ReverseGeocoder::RegionAddress const & address);

  bool IsVisible() const override { return m_isVisible; }
  void SetIsVisible(bool isVisible);

  bool HasCreationAnimation() const override;

  std::string GetPreferredName() const;

  kml::LocalizableString GetName() const;
  void SetName(kml::LocalizableString const & name);
  void SetName(std::string const & name, int8_t langCode);

  std::string GetCustomName() const;
  void SetCustomName(std::string const & customName);

  kml::PredefinedColor GetColor() const;
  void SetColor(kml::PredefinedColor color);

  m2::RectD GetViewport() const;

  std::string GetDescription() const;
  void SetDescription(std::string const & description);

  kml::Timestamp GetTimeStamp() const;
  void SetTimeStamp(kml::Timestamp timeStamp);

  uint8_t GetScale() const;
  void SetScale(uint8_t scale);

  dp::Anchor GetAnchor() const override;
  drape_ptr<SymbolNameZoomInfo> GetSymbolNames() const override;

  df::ColorConstant GetColorConstant() const override;

  kml::MarkGroupId GetGroupId() const override;

  int GetMinZoom() const override { return m_data.m_minZoom; }

  // Whether m_data.m_properties suitable to fill "Key info" part of placepage.
  bool CanFillPlacePageMetadata() const;

  void Attach(kml::MarkGroupId groupId);
  void AttachCompilation(kml::MarkGroupId groupId);
  void Detach();

  kml::GroupIdCollection const & GetCompilations() const { return m_compilationIds; }

private:
  drape_ptr<df::UserPointMark::SymbolNameZoomInfo> GetCustomSymbolNames() const;

  kml::BookmarkData m_data;
  kml::MarkGroupId m_groupId;
  kml::GroupIdCollection m_compilationIds;
  bool m_isVisible = true;
  search::ReverseGeocoder::RegionAddress m_address;
};

class BookmarkCategory : public UserMarkLayer
{
  using Base = UserMarkLayer;

public:
  BookmarkCategory(std::string const & name, kml::MarkGroupId groupId, bool autoSave);
  BookmarkCategory(kml::CategoryData && data, bool autoSave);

  static kml::PredefinedColor GetDefaultColor();

  kml::MarkGroupId GetID() const { return m_data.m_id; }
  kml::MarkGroupId GetParentID() const { return m_parentId; }
  void SetParentId(kml::MarkGroupId parentId) { m_parentId = parentId; }

  void SetIsVisible(bool isVisible) override;
  void SetName(std::string const & name);
  void SetDescription(std::string const & desc);
  void SetFileName(std::string const & fileName) { m_file = fileName; }
  std::string GetName() const;
  std::string const & GetFileName() const { return m_file; }
  std::string GetFileNameWithoutPath() const;

  void EnableAutoSave(bool enable) { m_autoSave = enable; }
  bool IsAutoSaveEnabled() const { return m_autoSave; }

  kml::CategoryData const & GetCategoryData() const { return m_data; }

  void SetServerId(std::string const & serverId);
  std::string const & GetServerId() const { return m_serverId; }

  bool HasElevationProfile() const;

  void SetAuthor(std::string const & name, std::string const & id);
  void SetAccessRules(kml::AccessRules accessRules);
  void SetTags(std::vector<std::string> const & tags);
  void SetCustomProperty(std::string const & key, std::string const & value);

  void SetDirty(bool updateModificationDate) override;

  kml::Timestamp GetLastModifiedTime() const { return m_data.m_lastModified; }

  // For serdes to access protected UserMarkLayer sets.
  friend class BookmarkManager;

private:
  // Stores file name from which bookmarks were loaded.
  std::string m_file;
  bool m_autoSave = true;
  kml::CategoryData m_data;
  std::string m_serverId;
  kml::MarkGroupId m_parentId = kml::kInvalidMarkGroupId;
};
