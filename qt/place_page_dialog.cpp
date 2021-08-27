#include "qt/place_page_dialog.hpp"

#include "map/place_page_info.hpp"

#include <string>

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

using namespace std;

string GenerateStars(int count)
{
  string stars;
  for (int i = 0; i < count; ++i)
    stars.append("★");
  return stars;
}

PlacePageDialog::PlacePageDialog(QWidget * parent, place_page::Info const & info,
                                 search::ReverseGeocoder::Address const & address)
  : QDialog(parent)
{
  QGridLayout * grid = new QGridLayout();
  int row = 0;
  {  // Coordinates.
    grid->addWidget(new QLabel("lat, lon"), row, 0);
    ms::LatLon const ll = info.GetLatLon();
    string const llstr =
        strings::to_string_dac(ll.m_lat, 7) + ", " + strings::to_string_dac(ll.m_lon, 7);
    QLabel * label = new QLabel(llstr.c_str());
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid->addWidget(label, row++, 1);
  }
  {
    grid->addWidget(new QLabel("CountryId"), row, 0);
    QLabel * label = new QLabel(QString::fromStdString(info.GetCountryId()));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid->addWidget(label, row++, 1);
  }
  // Title/Name/Custom Name.
  if (!info.GetTitle().empty())
  {
    grid->addWidget(new QLabel("Title"), row, 0);
    QLabel * label = new QLabel(QString::fromStdString(info.GetTitle()));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid->addWidget(label, row++, 1);
  }
  // Subtitle.
  if (info.IsFeature())
  {
    grid->addWidget(new QLabel("Subtitle"), row, 0);
    QLabel * label = new QLabel(QString::fromStdString(info.GetSubtitle()));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid->addWidget(label, row++, 1);
  }
  {  // Address.
    grid->addWidget(new QLabel("Address"), row, 0);
    QLabel * label = new QLabel(QString::fromStdString(address.FormatAddress()));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid->addWidget(label, row++, 1);
  }
  if (info.IsBookmark())
  {
    grid->addWidget(new QLabel("Bookmark"), row, 0);
    grid->addWidget(new QLabel("Yes"), row++, 1);
  }
  if (info.IsMyPosition())
  {
    grid->addWidget(new QLabel("MyPosition"), row, 0);
    grid->addWidget(new QLabel("Yes"), row++, 1);
  }
  if (info.HasApiUrl())
  {
    grid->addWidget(new QLabel("Api URL"), row, 0);
    grid->addWidget(new QLabel(QString::fromStdString(info.GetApiUrl())), row++, 1);
  }
  if (info.IsFeature())
  {
    grid->addWidget(new QLabel("Feature ID"), row, 0);
    auto labelF = new QLabel(QString::fromStdString(DebugPrint(info.GetID())));
    labelF->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid->addWidget(labelF, row++, 1);

    grid->addWidget(new QLabel("Raw Types"), row, 0);
    QLabel * labelT = new QLabel(QString::fromStdString(DebugPrint(info.GetTypes())));
    labelT->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid->addWidget(labelT, row++, 1);
  }
  for (auto const prop : info.AvailableProperties())
  {
    QString k;
    string v;
    bool link = false;
    switch (prop)
    {
    case osm::Props::Phone:
      k = "Phone";
      v = info.GetPhone();
      break;
    case osm::Props::Fax:
      k = "Fax";
      v = info.GetFax();
      break;
    case osm::Props::Email:
      k = "Email";
      v = info.GetEmail();
      link = true;
      break;
    case osm::Props::Website:
      k = "Website";
      v = info.GetWebsite();
      link = true;
      break;
    case osm::Props::FacebookPage:
      k = "Facebook";
      v = info.GetFacebookPage();
      link = true;
      break;
    case osm::Props::InstagramPage:
      k = "Instagram";
      v = info.GetInstagramPage();
      link = true;
      break;
    case osm::Props::TwitterPage:
      k = "Twitter";
      v = info.GetTwitterPage();
      link = true;
      break;
    case osm::Props::VkPage:
      k = "VK";
      v = info.GetVkPage();
      link = true;
      break;
    case osm::Props::Internet:
      k = "Internet";
      v = DebugPrint(info.GetInternet());
      break;
    case osm::Props::Cuisine:
      k = "Cuisine";
      v = strings::JoinStrings(info.GetCuisines(), ", ");
      break;
    case osm::Props::OpeningHours:
      k = "OpeningHours";
      v = info.GetOpeningHours();
      break;
    case osm::Props::Stars:
      k = "Stars";
      v = GenerateStars(info.GetStars());
      break;
    case osm::Props::Operator:
      k = "Operator";
      v = info.GetOperator();
      break;
    case osm::Props::Elevation:
      k = "Elevation";
      v = info.GetElevationFormatted();
      break;
    case osm::Props::Wikipedia:
      k = "Wikipedia";
      v = info.GetWikipediaLink();
      link = true;
      break;
    case osm::Props::Flats:
      k = "Flats";
      v = info.GetFlats();
      break;
    case osm::Props::BuildingLevels:
      k = "Building Levels";
      v = info.GetBuildingLevels();
      break;
    case osm::Props::Level:
      k = "Level";
      v = info.GetLevel();
      break;
    }
    grid->addWidget(new QLabel(k), row, 0);
    QLabel * label = new QLabel(QString::fromStdString(v));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    if (link)
    {
      label->setOpenExternalLinks(true);
      label->setTextInteractionFlags(Qt::TextBrowserInteraction);
      label->setText(QString::fromStdString("<a href=\"" + v + "\">" + v + "</a>"));
    }
    grid->addWidget(label, row++, 1);
  }

  QDialogButtonBox * dbb = new QDialogButtonBox();
  QPushButton * closeButton = new QPushButton("Close");
  closeButton->setDefault(true);
  connect(closeButton, SIGNAL(clicked()), this, SLOT(OnClose()));
  dbb->addButton(closeButton, QDialogButtonBox::RejectRole);

  if (info.ShouldShowEditPlace())
  {
    QPushButton * editButton = new QPushButton("Edit Place");
    connect(editButton, SIGNAL(clicked()), this, SLOT(OnEdit()));
    dbb->addButton(editButton, QDialogButtonBox::AcceptRole);
  }
  grid->addWidget(dbb);
  setLayout(grid);

  string const title = string("Place Page") + (info.IsBookmark() ? " (bookmarked)" : "");
  setWindowTitle(title.c_str());
}

void PlacePageDialog::OnClose() { reject(); }
void PlacePageDialog::OnEdit() { accept(); }
