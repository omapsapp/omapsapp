#pragma once

#include "kml/type_utils.hpp"

#include "coding/point_coding.hpp"

#include "base/assert.hpp"
#include "base/internal/message.hpp"  // DebugPrint(Timestamp)
#include "base/visitor.hpp"

#include "drape/color.hpp"

#include <string>
#include <vector>

namespace kml
{
/// @note Important! Should be synced with android/app/src/main/java/app/organicmaps/bookmarks/data/Icon.java
enum class PredefinedColor : uint8_t
{
  None = 0,
  Red,
  Blue,
  Purple,
  Yellow,
  Pink,
  Brown,
  Green,
  Orange,

  // Extended colors.
  DeepPurple,
  LightBlue,
  Cyan,
  Teal,
  Lime,
  DeepOrange,
  Gray,
  BlueGray,

  Count
};

inline std::string DebugPrint(PredefinedColor color)
{
  switch (color)
  {
  using enum kml::PredefinedColor;
  case None: return "None";
  case Red: return "Red";
  case Blue: return "Blue";
  case Purple: return "Purple";
  case Yellow: return "Yellow";
  case Pink: return "Pink";
  case Brown: return "Brown";
  case Green: return "Green";
  case Orange: return "Orange";
  case DeepPurple: return "DeepPurple";
  case LightBlue: return "LightBlue";
  case Cyan: return "Cyan";
  case Teal: return "Teal";
  case Lime: return "Lime";
  case DeepOrange: return "DeepOrange";
  case Gray: return "Gray";
  case BlueGray: return "BlueGray";
  case Count: return {};
  }
  UNREACHABLE();
}

inline dp::Color ColorFromPredefinedColor(PredefinedColor color)
{
  switch (color)
  {
  using enum kml::PredefinedColor;
  case Red: return dp::Color(229, 27, 35, 255);
  case Pink: return dp::Color(255, 65, 130, 255);
  case Purple: return dp::Color(155, 36, 178, 255);
  case DeepPurple: return dp::Color(102, 57, 191, 255);
  case Blue: return dp::Color(0, 102, 204, 255);
  case LightBlue: return dp::Color(36, 156, 242, 255);
  case Cyan: return dp::Color(20, 190, 205, 255);
  case Teal: return dp::Color(0, 165, 140, 255);
  case Green: return dp::Color(60, 140, 60, 255);
  case Lime: return dp::Color(147, 191, 57, 255);
  case Yellow: return dp::Color(255, 200, 0, 255);
  case Orange: return dp::Color(255, 150, 0, 255);
  case DeepOrange: return dp::Color(240, 100, 50, 255);
  case Brown: return dp::Color(128, 70, 51, 255);
  case Gray: return dp::Color(115, 115, 115, 255);
  case BlueGray: return dp::Color(89, 115, 128, 255);
  case None:
  case Count: return ColorFromPredefinedColor(kml::PredefinedColor::Red);
  }
  UNREACHABLE();
}

kml::PredefinedColor GetRandomPredefinedColor();

enum class AccessRules : uint8_t
{
  // Do not change the order because of binary serialization.
  Local = 0,
  Public,
  DirectLink,
  P2P,
  Paid,
  AuthorOnly,

  Count
};

inline std::string DebugPrint(AccessRules accessRules)
{
  switch (accessRules)
  {
  using enum kml::AccessRules;
  case Local: return "Local";
  case Public: return "Public";
  case DirectLink: return "DirectLink";
  case P2P: return "P2P";
  case Paid: return "Paid";
  case AuthorOnly: return "AuthorOnly";
  case Count: return {};
  }
  UNREACHABLE();
}

enum class CompilationType : uint8_t
{
  // Do not change the order because of binary serialization.
  Category = 0,
  Collection,
  Day,
};

inline std::string DebugPrint(CompilationType compilationType)
{
  switch (compilationType)
  {
  using enum kml::CompilationType;
  case Category: return "Category";
  case Collection: return "Collection";
  case Day: return "Day";
  }
  UNREACHABLE();
}

/// @note Important! Should be synced with android/app/src/main/java/app/organicmaps/bookmarks/data/Icon.java
enum class BookmarkIcon : uint16_t
{
  None = 0,
  Hotel,
  Animals,
  Buddhism,
  Building,
  Christianity,
  Entertainment,
  Exchange,
  Food,
  Gas,
  Judaism,
  Medicine,
  Mountain,
  Museum,
  Islam,
  Park,
  Parking,
  Shop,
  Sights,
  Swim,
  Water,

  // Extended icons.
  Bar,
  Transport,
  Viewpoint,
  Sport,
  Pub,
  Art,
  Bank,
  Cafe,
  Pharmacy,
  Stadium,
  Theatre,
  Information,

  Count
};

inline std::string ToString(BookmarkIcon icon)
{
  switch (icon)
  {
  using enum kml::BookmarkIcon;
  case None: return "None";
  case Hotel: return "Hotel";
  case Animals: return "Animals";
  case Buddhism: return "Buddhism";
  case Building: return "Building";
  case Christianity: return "Christianity";
  case Entertainment: return "Entertainment";
  case Exchange: return "Exchange";
  case Food: return "Food";
  case Gas: return "Gas";
  case Judaism: return "Judaism";
  case Medicine: return "Medicine";
  case Mountain: return "Mountain";
  case Museum: return "Museum";
  case Islam: return "Islam";
  case Park: return "Park";
  case Parking: return "Parking";
  case Shop: return "Shop";
  case Sights: return "Sights";
  case Swim: return "Swim";
  case Water: return "Water";
  case Bar: return "Bar";
  case Transport: return "Transport";
  case Viewpoint: return "Viewpoint";
  case Sport: return "Sport";
  case Pub: return "Pub";
  case Art: return "Art";
  case Bank: return "Bank";
  case Cafe: return "Cafe";
  case Pharmacy: return "Pharmacy";
  case Stadium: return "Stadium";
  case Theatre: return "Theatre";
  case Information: return "Information";
  case Count: return {};
  }
  UNREACHABLE();
}

// Note: any changes in binary format of this structure
// will affect previous kmb versions, where this structure was used.
struct ColorData
{
  DECLARE_VISITOR_AND_DEBUG_PRINT(ColorData, visitor(m_predefinedColor, "predefinedColor"),
                                  visitor(m_rgba, "rgba"))

  bool operator==(ColorData const & data) const
  {
    return m_predefinedColor == data.m_predefinedColor && m_rgba == data.m_rgba;
  }

  bool operator!=(ColorData const & data) const { return !operator==(data); }

  // Predefined color.
  PredefinedColor m_predefinedColor = PredefinedColor::None;
  // Color in RGBA format.
  uint32_t m_rgba = 0;
};

// This structure is used in FileDataV6 because
// its binary format is the same as in kmb version 6.
// Make a copy of this structure in a file types_v<n>.hpp
// in case of any changes of its binary format.
struct BookmarkData
{
  DECLARE_VISITOR_AND_DEBUG_PRINT(BookmarkData, visitor(m_id, "id"),
                                  visitor(m_name, "name"),
                                  visitor(m_description, "description"),
                                  visitor(m_featureTypes, "featureTypes"),
                                  visitor(m_customName, "customName"),
                                  visitor(m_color, "color"),
                                  visitor(m_icon, "icon"),
                                  visitor(m_viewportScale, "viewportScale"),
                                  visitor(m_timestamp, "timestamp"),
                                  visitor(m_point, "point"),
                                  visitor(m_boundTracks, "boundTracks"),
                                  visitor(m_visible, "visible"),
                                  visitor(m_nearestToponym, "nearestToponym"),
                                  visitor(m_minZoom, "minZoom"),
                                  visitor(m_compilations, "compilations"),
                                  visitor(m_properties, "properties"),
                                  VISITOR_COLLECTABLE)

  DECLARE_COLLECTABLE(LocalizableStringIndex, m_name, m_description, m_customName,
                      m_nearestToponym, m_properties)

  bool operator==(BookmarkData const & data) const
  {
    return m_id == data.m_id && m_name == data.m_name &&
           m_description == data.m_description &&
           m_color == data.m_color && m_icon == data.m_icon &&
           m_viewportScale == data.m_viewportScale &&
           IsEqual(m_timestamp, data.m_timestamp) &&
           m_point.EqualDxDy(data.m_point, kMwmPointAccuracy) &&
           m_featureTypes == data.m_featureTypes &&
           m_customName == data.m_customName &&
           m_boundTracks == data.m_boundTracks &&
           m_visible == data.m_visible &&
           m_nearestToponym == data.m_nearestToponym &&
           m_minZoom == data.m_minZoom &&
           m_compilations == data.m_compilations &&
           m_properties == data.m_properties;
  }

  bool operator!=(BookmarkData const & data) const { return !operator==(data); }

  // Unique id (it will not be serialized in text files).
  MarkId m_id = kInvalidMarkId;
  // Bookmark's name.
  LocalizableString m_name;
  // Bookmark's description.
  LocalizableString m_description;
  // Bound feature's types: type indices sorted by importance, the most
  // important one goes first.
  std::vector<uint32_t> m_featureTypes;
  // Custom bookmark's name.
  LocalizableString m_customName;
  // Bookmark's color.
  ColorData m_color;
  // Bookmark's icon.
  BookmarkIcon m_icon = BookmarkIcon::None;
  // Viewport scale. 0 is a default value (no scale set).
  uint8_t m_viewportScale = 0;
  // Creation timestamp.
  Timestamp m_timestamp = {};
  // Coordinates in mercator.
  m2::PointD m_point;
  // Bound tracks (vector contains local track ids).
  std::vector<LocalId> m_boundTracks;
  // Visibility.
  bool m_visible = true;
  // Nearest toponym.
  std::string m_nearestToponym;
  // Minimal zoom when bookmark is visible.
  int m_minZoom = 1;
  // List of compilationIds.
  std::vector<CompilationId> m_compilations;
  // Key-value properties.
  Properties m_properties;
};

// Note: any changes in binary format of this structure
// will affect previous kmb versions, where this structure was used.
struct TrackLayer
{
  DECLARE_VISITOR_AND_DEBUG_PRINT(TrackLayer, visitor(m_lineWidth, "lineWidth"),
                                  visitor(m_color, "color"))

  bool operator==(TrackLayer const & layer) const
  {
    double constexpr kEps = 1e-5;
    return m_color == layer.m_color && fabs(m_lineWidth - layer.m_lineWidth) < kEps;
  }

  bool operator!=(TrackLayer const & layer) const { return !operator==(layer); }

  // Line width in pixels. Valid range is [kMinLineWidth; kMaxLineWidth].
  double m_lineWidth = 5.0;
  // Layer's color.
  ColorData m_color;
};

struct MultiGeometry
{
  using LineT = std::vector<geometry::PointWithAltitude>;
  using TimeT = std::vector<double>;

  std::vector<LineT> m_lines;
  std::vector<TimeT> m_timestamps;

  void Clear();
  bool IsValid() const;

  bool operator==(MultiGeometry const & rhs) const
  {
    return IsEqual(m_lines, rhs.m_lines) && m_timestamps == rhs.m_timestamps;
  }

  friend std::string DebugPrint(MultiGeometry const & geom)
  {
    auto str = ::DebugPrint(geom.m_lines);
    if (geom.HasTimestamps())
      str.append(" ").append(::DebugPrint(geom.m_timestamps));
    return str;
  }

  void FromPoints(std::vector<m2::PointD> const & points);

  /// This method should be used for tests only.
  void AddLine(std::initializer_list<geometry::PointWithAltitude> lst);
  /// This method should be used for tests only.
  void AddTimestamps(std::initializer_list<double> lst);

  bool HasTimestamps() const;
  bool HasTimestampsFor(size_t lineIndex) const;

  size_t GetNumberOfLinesWithouTimestamps() const;
  size_t GetNumberOfLinesWithTimestamps() const;
};

struct TrackData
{
  DECLARE_VISITOR_AND_DEBUG_PRINT(TrackData, visitor(m_id, "id"),
                                  visitor(m_localId, "localId"),
                                  visitor(m_name, "name"),
                                  visitor(m_description, "description"),
                                  visitor(m_layers, "layers"),
                                  visitor(m_timestamp, "timestamp"),
                                  visitor(m_geometry, "geometry"),
                                  visitor(m_visible, "visible"),
                                  visitor(m_nearestToponyms, "nearestToponyms"),
                                  visitor(m_properties, "properties"),
                                  VISITOR_COLLECTABLE)

  DECLARE_COLLECTABLE(LocalizableStringIndex, m_name, m_description, m_nearestToponyms, m_properties)

  bool operator==(TrackData const & data) const
  {
    return m_id == data.m_id && m_localId == data.m_localId && m_name == data.m_name &&
           m_description == data.m_description && m_layers == data.m_layers &&
           IsEqual(m_timestamp, data.m_timestamp) && m_geometry == data.m_geometry &&
           m_visible == data.m_visible && m_nearestToponyms == data.m_nearestToponyms &&
           m_properties == data.m_properties;
  }

  bool operator!=(TrackData const & data) const { return !operator==(data); }

  // Unique id (it will not be serialized in text files).
  TrackId m_id = kInvalidTrackId;
  // Local track id.
  LocalId m_localId = 0;
  // Track's name.
  LocalizableString m_name;
  // Track's description.
  LocalizableString m_description;
  // Layers.
  std::vector<TrackLayer> m_layers;
  // Creation timestamp.
  Timestamp m_timestamp = {};
  MultiGeometry m_geometry;
  // Visibility.
  bool m_visible = true;
  // Nearest toponyms.
  std::vector<std::string> m_nearestToponyms;
  // Key-value properties.
  Properties m_properties;
};

// This structure is used in FileDataV6 because
// its binary format is the same as in kmb version 6.
// Make a copy of this structure in a file types_v<n>.hpp
// in case of any changes of its binary format.
struct CategoryData
{
  DECLARE_VISITOR_AND_DEBUG_PRINT(CategoryData, visitor(m_id, "id"),
                                  visitor(m_compilationId, "compilationId"),
                                  visitor(m_type, "type"),
                                  visitor(m_name, "name"),
                                  visitor(m_imageUrl, "imageUrl"),
                                  visitor(m_annotation, "annotation"),
                                  visitor(m_description, "description"),
                                  visitor(m_visible, "visible"),
                                  visitor(m_authorName, "authorName"),
                                  visitor(m_authorId, "authorId"),
                                  visitor(m_rating, "rating"),
                                  visitor(m_reviewsNumber, "reviewsNumber"),
                                  visitor(m_lastModified, "lastModified"),
                                  visitor(m_accessRules, "accessRules"),
                                  visitor(m_tags, "tags"),
                                  visitor(m_toponyms, "toponyms"),
                                  visitor(m_languageCodes, "languageCodes"),
                                  visitor(m_properties, "properties"),
                                  VISITOR_COLLECTABLE)

  DECLARE_COLLECTABLE(LocalizableStringIndex, m_name, m_annotation, m_description,
                      m_imageUrl, m_authorName, m_authorId, m_tags, m_toponyms, m_properties)

  bool operator==(CategoryData const & data) const
  {
    double constexpr kEps = 1e-5;
    return m_id == data.m_id && m_compilationId == data.m_compilationId &&
           m_type == data.m_type && m_name == data.m_name && m_imageUrl == data.m_imageUrl &&
           m_annotation == data.m_annotation && m_description == data.m_description &&
           m_visible == data.m_visible && m_accessRules == data.m_accessRules &&
           m_authorName == data.m_authorName && m_authorId == data.m_authorId &&
           fabs(m_rating - data.m_rating) < kEps && m_reviewsNumber == data.m_reviewsNumber &&
           IsEqual(m_lastModified, data.m_lastModified) && m_tags == data.m_tags &&
           m_toponyms == data.m_toponyms && m_languageCodes == data.m_languageCodes &&
           m_properties == data.m_properties;
  }

  bool operator!=(CategoryData const & data) const { return !operator==(data); }

  // Unique id (it will not be serialized in text files).
  MarkGroupId m_id = kInvalidMarkGroupId;
  // Id unique within single kml (have to be serialized in text files).
  CompilationId m_compilationId = kInvalidCompilationId;
  // Unique ids of nested groups (it will not be serialized in text files).
  GroupIdCollection m_compilationIds;
  // Compilation's type
  CompilationType m_type = CompilationType::Category;
  // Category's name.
  LocalizableString m_name;
  // Image URL.
  std::string m_imageUrl;
  // Category's description.
  LocalizableString m_annotation;
  // Category's description.
  LocalizableString m_description;
  // Collection visibility.
  bool m_visible = true;
  // Author's name.
  std::string m_authorName;
  // Author's id.
  std::string m_authorId;
  // Last modification timestamp.
  Timestamp m_lastModified;
  // Rating.
  double m_rating = 0.0;
  // Number of reviews.
  uint32_t m_reviewsNumber = 0;
  // Access rules.
  AccessRules m_accessRules = AccessRules::Local;
  // Collection of tags.
  std::vector<std::string> m_tags;
  // Collection of geo ids for relevant toponyms.
  std::vector<std::string> m_toponyms;
  // Language codes.
  std::vector<int8_t> m_languageCodes;
  // Key-value properties.
  Properties m_properties;
};

struct FileData
{
  DECLARE_VISITOR_AND_DEBUG_PRINT(FileData, visitor(m_serverId, "serverId"),
                                  visitor(m_categoryData, "category"),
                                  visitor(m_bookmarksData, "bookmarks"),
                                  visitor(m_tracksData, "tracks"),
                                  visitor(m_compilationsData, "compilations"))

  bool operator==(FileData const & data) const
  {
    return m_serverId == data.m_serverId && m_categoryData == data.m_categoryData &&
           m_bookmarksData == data.m_bookmarksData && m_tracksData == data.m_tracksData &&
           m_compilationsData == data.m_compilationsData;
  }

  bool operator!=(FileData const & data) const { return !operator==(data); }

  // Device id (it will not be serialized in text files).
  std::string m_deviceId;
  // Server id.
  std::string m_serverId;
  // Category's data.
  CategoryData m_categoryData;
  // Bookmarks collection.
  std::vector<BookmarkData> m_bookmarksData;
  // Tracks collection.
  std::vector<TrackData> m_tracksData;
  // Compilation collection.
  std::vector<CategoryData> m_compilationsData;
};

void SetBookmarksMinZoom(FileData & fileData, double countPerTile, int maxZoom);

inline std::string DebugPrint(BookmarkIcon icon)
{
  return ToString(icon);
}

inline std::string DebugPrint(TimestampMillis const & ts)
{
  return ::DebugPrint(static_cast<Timestamp>(ts));
}

}  // namespace kml
