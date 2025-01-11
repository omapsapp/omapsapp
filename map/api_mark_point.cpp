#include "map/api_mark_point.hpp"

#include "base/logging.hpp"

#include <map>

namespace style
{
std::map<std::string, std::string> kStyleToColor = {
  {"placemark-red", "BookmarkRed"},
  {"placemark-blue", "BookmarkBlue"},
  {"placemark-purple", "BookmarkPurple"},
  {"placemark-yellow", "BookmarkYellow"},
  {"placemark-pink", "BookmarkPink"},
  {"placemark-brown", "BookmarkBrown"},
  {"placemark-green", "BookmarkGreen"},
  {"placemark-orange", "BookmarkOrange"},
  {"placemark-deeppurple", "BookmarkDeepPurple"},
  {"placemark-lightblue", "BookmarkLightBlue"},
  {"placemark-cyan", "BookmarkCyan"},

  {"placemark-teal", "BookmarkTeal"},
  {"placemark-lime", "BookmarkLime"},
  {"placemark-deeporange", "BookmarkDeepOrange"},
  {"placemark-gray", "BookmarkGray"},
  {"placemark-bluegray", "BookmarkBlueGray"}
};

std::string GetSupportedStyle(std::string const & style)
{
  auto const it = kStyleToColor.find(style);
  if (it == kStyleToColor.cend())
    return "BookmarkGreen";
  return it->second;
}
}  // style

ApiMarkPoint::ApiMarkPoint(m2::PointD const & ptOrg)
  : UserMark(ptOrg, UserMark::Type::API)
{}

ApiMarkPoint::ApiMarkPoint(std::string const & name, std::string const & id, std::string const & style,
                           m2::PointD const & ptOrg)
  : UserMark(ptOrg, UserMark::Type::API)
  , m_name(name)
  , m_id(id)
  , m_style(style)
{}

drape_ptr<df::UserPointMark::SymbolNameZoomInfo> ApiMarkPoint::GetSymbolNames() const
{
  //TODO: use its own icon.
  auto symbol = make_unique_dp<SymbolNameZoomInfo>();
  symbol->insert(std::make_pair(1 /* zoomLevel */, "coloredmark-default-s"));
  return symbol;
}

df::ColorConstant ApiMarkPoint::GetColorConstant() const
{
  return m_style;
}

void ApiMarkPoint::SetName(std::string const & name)
{
  SetDirty();
  m_name = name;
}

void ApiMarkPoint::SetApiID(std::string const & id)
{
  SetDirty();
  m_id = id;
}

void ApiMarkPoint::SetStyle(std::string const & style)
{
  SetDirty();
  m_style = style;
}
