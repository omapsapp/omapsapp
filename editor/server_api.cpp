#include "editor/server_api.hpp"

#include "coding/url.hpp"

#include "geometry/mercator.hpp"

#include "base/logging.hpp"
#include "base/math.hpp"
#include "base/string_utils.hpp"
#include "base/timer.hpp"

#include <algorithm>
#include <sstream>

#include <pugixml.hpp>

namespace
{
std::string KeyValueTagsToXML(osm::ServerApi06::KeyValueTags const & kvTags)
{
  std::ostringstream stream;
  stream << "<osm>\n"
  "<changeset>\n";
  for (auto const & tag : kvTags)
    stream << "  <tag k=\"" << tag.first << "\" v=\"" << tag.second << "\"/>\n";
  stream << "</changeset>\n"
  "</osm>\n";
  return stream.str();
}
}  // namespace

namespace osm
{

ServerApi06::ServerApi06(OsmOAuth const & auth)
  : m_auth(auth)
{
}

uint64_t ServerApi06::CreateChangeSet(KeyValueTags const & kvTags) const
{
  if (!m_auth.IsAuthorized())
    MYTHROW(NotAuthorized, ("Not authorized."));

  OsmOAuth::Response const response = m_auth.Request("/changeset/create", "PUT", KeyValueTagsToXML(kvTags));
  if (response.first != OsmOAuth::HTTP::OK)
    MYTHROW(CreateChangeSetHasFailed, ("CreateChangeSet request has failed:", response));

  uint64_t id;
  if (!strings::to_uint64(response.second, id))
    MYTHROW(CantParseServerResponse, ("Can't parse changeset ID from server response."));
  return id;
}

uint64_t ServerApi06::CreateElement(editor::XMLFeature const & element) const
{
  OsmOAuth::Response const response = m_auth.Request("/" + element.GetTypeString() + "/create",
                                                     "PUT", element.ToOSMString());
  if (response.first != OsmOAuth::HTTP::OK)
    MYTHROW(CreateElementHasFailed, ("CreateElement request has failed:", response, "for", element));
  uint64_t id;
  if (!strings::to_uint64(response.second, id))
    MYTHROW(CantParseServerResponse, ("Can't parse created node ID from server response."));
  return id;
}

void ServerApi06::CreateElementAndSetAttributes(editor::XMLFeature & element) const
{
  uint64_t const id = CreateElement(element);
  element.SetAttribute("id", strings::to_string(id));
  element.SetAttribute("version", "1");
}

uint64_t ServerApi06::ModifyElement(editor::XMLFeature const & element) const
{
  std::string const id = element.GetAttribute("id");
  CHECK(!id.empty(), ("id attribute is missing for", element));

  OsmOAuth::Response const response = m_auth.Request("/" + element.GetTypeString() + "/" + id,
                                                     "PUT", element.ToOSMString());
  if (response.first != OsmOAuth::HTTP::OK)
    MYTHROW(ModifyElementHasFailed, ("ModifyElement request has failed:", response, "for", element));
  uint64_t version;
  if (!strings::to_uint64(response.second, version))
    MYTHROW(CantParseServerResponse, ("Can't parse element version from server response", response.second));
  return version;
}

void ServerApi06::ModifyElementAndSetVersion(editor::XMLFeature & element) const
{
  uint64_t const version = ModifyElement(element);
  element.SetAttribute("version", strings::to_string(version));
}

void ServerApi06::DeleteElement(editor::XMLFeature const & element) const
{
  std::string const id = element.GetAttribute("id");
  if (id.empty())
    MYTHROW(DeletedElementHasNoIdAttribute, ("Please set id attribute for", element));

  OsmOAuth::Response const response = m_auth.Request("/" + element.GetTypeString() + "/" + id,
                                                     "DELETE", element.ToOSMString());
  if (response.first != OsmOAuth::HTTP::OK && response.first != OsmOAuth::HTTP::Gone)
    MYTHROW(ErrorDeletingElement, ("Could not delete an element:", response));
}

void ServerApi06::UpdateChangeSet(uint64_t changesetId, KeyValueTags const & kvTags) const
{
  OsmOAuth::Response const response = m_auth.Request("/changeset/" + strings::to_string(changesetId), "PUT", KeyValueTagsToXML(kvTags));
  if (response.first != OsmOAuth::HTTP::OK)
    MYTHROW(UpdateChangeSetHasFailed, ("UpdateChangeSet request has failed:", response));
}

void ServerApi06::CloseChangeSet(uint64_t changesetId) const
{
  OsmOAuth::Response const response = m_auth.Request("/changeset/" + strings::to_string(changesetId) + "/close", "PUT");
  if (response.first != OsmOAuth::HTTP::OK)
    MYTHROW(ErrorClosingChangeSet, ("CloseChangeSet request has failed:", response));
}

uint64_t ServerApi06::CreateNote(ms::LatLon const & ll, std::string const & message) const
{
  CHECK(!message.empty(), ("Note content should not be empty."));
  std::string const params = "?lat=" + strings::to_string_dac(ll.m_lat, 7) +
                             "&lon=" + strings::to_string_dac(ll.m_lon, 7) +
                             "&text=" + url::UrlEncode(message + " #organicmaps " + OMIM_OS_NAME);
  OsmOAuth::Response const response = m_auth.Request("/notes" + params, "POST");
  if (response.first != OsmOAuth::HTTP::OK)
    MYTHROW(ErrorAddingNote, ("Could not post a new note:", response));
  pugi::xml_document details;
  if (!details.load_string(response.second.c_str()))
    MYTHROW(CantParseServerResponse, ("Could not parse a note XML response", response));
  pugi::xml_node const uid = details.child("osm").child("note").child("id");
  if (!uid)
    MYTHROW(CantParseServerResponse, ("Caould not find a note id", response));
  return uid.text().as_ullong();
}

void ServerApi06::CloseNote(uint64_t const id) const
{
  OsmOAuth::Response const response = m_auth.Request("/notes/" + strings::to_string(id) + "/close", "POST");
  if (response.first != OsmOAuth::HTTP::OK)
    MYTHROW(ErrorDeletingElement, ("Could not close a note:", response));
}

bool ServerApi06::TestOSMUser(std::string const & userName)
{
  std::string const method = "/user/" + url::UrlEncode(userName);
  return m_auth.DirectRequest(method, false).first == OsmOAuth::HTTP::OK;
}

UserPreferences ServerApi06::GetUserPreferences() const
{
  OsmOAuth::Response const response = m_auth.Request("/user/details");
  if (response.first != OsmOAuth::HTTP::OK)
    MYTHROW(CantGetUserPreferences, (response));

  pugi::xml_document details;
  if (!details.load_string(response.second.c_str()))
    MYTHROW(CantParseUserPreferences, (response));

  pugi::xml_node const user = details.child("osm").child("user");
  if (!user || !user.attribute("id"))
    MYTHROW(CantParseUserPreferences, ("No <user> or 'id' attribute", response));

  UserPreferences pref;
  pref.m_id = user.attribute("id").as_ullong();
  pref.m_displayName = user.attribute("display_name").as_string();
  pref.m_accountCreated = base::StringToTimestamp(user.attribute("account_created").as_string());
  pref.m_imageUrl = user.child("img").attribute("href").as_string();
  pref.m_changesets = user.child("changesets").attribute("count").as_uint();
  return pref;
}

OsmOAuth::Response ServerApi06::GetXmlFeaturesInRect(double minLat, double minLon, double maxLat, double maxLon) const
{
  using strings::to_string_dac;

  // Digits After Comma.
  static constexpr double kDAC = 7;
  std::string const url = "/map?bbox=" + to_string_dac(minLon, kDAC) + ',' +
                          to_string_dac(minLat, kDAC) + ',' + to_string_dac(maxLon, kDAC) + ',' +
                          to_string_dac(maxLat, kDAC);

  return m_auth.DirectRequest(url);
}

OsmOAuth::Response ServerApi06::GetXmlFeaturesAtLatLon(double lat, double lon, double radiusInMeters) const
{
  double const latDegreeOffset = radiusInMeters * mercator::Bounds::kDegreesInMeter;
  double const minLat = std::max(-90.0, lat - latDegreeOffset);
  double const maxLat = std::min( 90.0, lat + latDegreeOffset);
  double const cosL = std::max(cos(math::DegToRad(std::max(fabs(minLat), fabs(maxLat)))), 0.00001);
  double const lonDegreeOffset = radiusInMeters * mercator::Bounds::kDegreesInMeter / cosL;
  double const minLon = std::max(-180.0, lon - lonDegreeOffset);
  double const maxLon = std::min( 180.0, lon + lonDegreeOffset);
  return GetXmlFeaturesInRect(minLat, minLon, maxLat, maxLon);
}

OsmOAuth::Response ServerApi06::GetXmlFeaturesAtLatLon(ms::LatLon const & ll, double radiusInMeters) const
{
  return GetXmlFeaturesAtLatLon(ll.m_lat, ll.m_lon, radiusInMeters);
}

} // namespace osm
