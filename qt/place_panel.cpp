#include "qt/place_panel.hpp"

#include "qt/qt_common/text_dialog.hpp"

#include "map/place_page_info.hpp"
#include "indexer/validate_and_format_contacts.hpp"
#include "platform/settings.hpp"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

#include <sstream>
#include <string>

namespace
{
constexpr int kMaximumPanelWidthPixels = 390;
constexpr int kMinimumPanelWidthPixels = 200;
constexpr int kMaxLengthOfPlacePageDescription = 500;

std::string getShortDescription(const std::string & description)
{
  std::string_view view(description);

  auto const paragraphStart = view.find("<p>");
  auto const paragraphEnd = view.find("</p>");

  if (paragraphStart == 0 && paragraphEnd != std::string::npos)
    view = view.substr(3, paragraphEnd - 3);

  if (view.length() > kMaxLengthOfPlacePageDescription)
    return std::string(view.substr(0, kMaxLengthOfPlacePageDescription - 3)) + "...";

  return std::string(view);
}

std::string_view stripSchemeFromURI(std::string_view uri) {
  for (std::string_view prefix : {"https://", "http://"})
  {
    if (uri.starts_with(prefix))
      return uri.substr(prefix.size());
  }
  return uri;
}
}  // namespace

class QHLine : public QFrame
{
public:
  QHLine(QWidget * parent = nullptr) : QFrame(parent)
  {
    setFrameShape(QFrame::HLine);
    setFrameShadow(QFrame::Sunken);
  }
};

void PlacePanel::addCommonButtons(QDialogButtonBox * dbb, place_page::Info const & info)
{
  dbb->setCenterButtons(true);

  QPushButton * fromButton = new QPushButton("From");
  fromButton->setIcon(QIcon(":/navig64/point-start.png"));
  fromButton->setAutoDefault(false);
  connect(fromButton, &QAbstractButton::clicked, this, [this, info](){ routeFrom(info); });
  dbb->addButton(fromButton, QDialogButtonBox::ActionRole);

  QPushButton * addStopButton = new QPushButton("Stop");
  addStopButton->setIcon(QIcon(":/navig64/point-intermediate.png"));
  addStopButton->setAutoDefault(false);
  connect(addStopButton, &QAbstractButton::clicked, this, [this, info](){ addStop(info); });
  dbb->addButton(addStopButton, QDialogButtonBox::ActionRole);

  QPushButton * routeToButton = new QPushButton("To");
  routeToButton->setIcon(QIcon(":/navig64/point-finish.png"));
  routeToButton->setAutoDefault(false);
  connect(routeToButton, &QAbstractButton::clicked, this, [this, info](){ routeTo(info); });
  dbb->addButton(routeToButton, QDialogButtonBox::ActionRole);

  if (info.ShouldShowEditPlace())
  {
    QPushButton * editButton = new QPushButton("Edit Place");
    connect(editButton, &QAbstractButton::clicked, this, [this, info](){ editPlace(info); });
    dbb->addButton(editButton, QDialogButtonBox::AcceptRole);
  }
}

PlacePanel::PlacePanel(QWidget * parent)
  : QWidget(parent)
{
  setMaximumWidth(kMaximumPanelWidthPixels);
  setMinimumWidth(kMinimumPanelWidthPixels);
}

void PlacePanel::setPlace(place_page::Info const & info, search::ReverseGeocoder::Address const & address)
{
  bool developerMode;
  if (settings::Get(settings::kDeveloperMode, developerMode) && developerMode)
    updateInterfaceDeveloper(info, address);
  else
    updateInterfaceUser(info, address);
}

void PlacePanel::updateInterfaceUser(place_page::Info const & info, search::ReverseGeocoder::Address const & address)
{
  auto const & title = info.GetTitle();

  DeleteExistingLayout();

  QVBoxLayout * layout = new QVBoxLayout();

  QScrollArea * scrollArea = new QScrollArea();
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);

  QWidget *containerWidget = new QWidget();
  QVBoxLayout * innerLayout = new QVBoxLayout(containerWidget);
  containerWidget->setLayout(innerLayout);

  {
    QVBoxLayout * header = new QVBoxLayout();

    if (!title.empty())
    {
      QLabel * titleLabel = new QLabel(QString::fromStdString("<h1>" + title + "</h1>"));
      titleLabel->setWordWrap(true);
      header->addWidget(titleLabel);
    }

    if (auto const subTitle = info.GetSubtitle(); !subTitle.empty())
    {
      QLabel * subtitleLabel = new QLabel(QString::fromStdString(subTitle));
      subtitleLabel->setWordWrap(true);
      header->addWidget(subtitleLabel);
    }

    if (auto const addressFormatted = address.FormatAddress(); !addressFormatted.empty())
    {
      QLabel * addressLabel = new QLabel(QString::fromStdString(addressFormatted));
      addressLabel->setWordWrap(true);
      header->addWidget(addressLabel);
    }

    innerLayout->addLayout(header);
  }

  {
    QHLine * line = new QHLine();
    innerLayout->addWidget(line);
  }

  {
    QGridLayout * data = new QGridLayout();

    int row = 0;

    auto const addEntry = [data, &row](std::string const & key, std::string const & value, bool isLink = false)
    {
      data->addWidget(new QLabel(QString::fromStdString(key)), row, 0);
      QLabel * label = new QLabel(QString::fromStdString(value));
      label->setTextInteractionFlags(Qt::TextSelectableByMouse);
      label->setWordWrap(true);
      if (isLink)
      {
        label->setOpenExternalLinks(true);
        label->setTextInteractionFlags(Qt::TextBrowserInteraction);
        label->setText(QString::fromStdString("<a href=\"" + value + "\">" + value + "</a>"));
      }
      data->addWidget(label, row++, 1);
      return label;
    };

    if (info.IsBookmark())
      addEntry("Bookmark", "Yes");


    // Wikipedia fragment
    if (auto const & wikipedia = info.GetMetadata(feature::Metadata::EType::FMD_WIKIPEDIA); !wikipedia.empty())
    {
      QLabel * name = new QLabel("Wikipedia");
      name->setOpenExternalLinks(true);
      name->setTextInteractionFlags(Qt::TextBrowserInteraction);
      name->setText(QString::fromStdString("<a href=\"" + feature::Metadata::ToWikiURL(std::string(wikipedia)) + "\">Wikipedia</a>"));
      data->addWidget(name, row++, 0);
    }

    // Description
    if (const auto & description = info.GetWikiDescription(); !description.empty())
    {
      auto descriptionShort = getShortDescription(description);

      QLabel * value = new QLabel(QString::fromStdString(descriptionShort));
      value->setWordWrap(true);

      data->addWidget(value, row++, 0, 1, 2);

      QPushButton * wikiButton = new QPushButton("More...", value);
      wikiButton->setAutoDefault(false);
      connect(wikiButton, &QAbstractButton::clicked, this, [this, description, title]()
      {
        auto textDialog = TextDialog(this, QString::fromStdString(description), QString::fromStdString("Wikipedia: " + title));
        textDialog.exec();
      });

      data->addWidget(wikiButton, row++, 0, 1, 2, Qt::AlignLeft);
    }

    // Opening hours fragment
    if (auto openingHours = info.GetOpeningHours(); !openingHours.empty())
      addEntry("Opening hours", std::string(openingHours));

    // Cuisine fragment
    if (auto cuisines = info.FormatCuisines(); !cuisines.empty())
      addEntry("Cuisine", cuisines);

    // Entrance fragment
    // TODO

    // Phone fragment
    if (auto phoneNumber = info.GetMetadata(feature::Metadata::EType::FMD_PHONE_NUMBER); !phoneNumber.empty())
    {
      data->addWidget(new QLabel("Phone"), row, 0);

      QLabel * value = new QLabel(QString::fromStdString("<a href='tel:" + std::string(phoneNumber) + "'>" + std::string(phoneNumber) + "</a>"));
      value->setOpenExternalLinks(true);

      data->addWidget(value, row++, 1);
    }

    // Operator fragment
    if (auto operatorName = info.GetMetadata(feature::Metadata::EType::FMD_OPERATOR); !operatorName.empty())
      addEntry("Operator", std::string(operatorName));

    // Wifi fragment
    if (info.HasWifi())
      addEntry("Wi-Fi", "Yes");

    // Links fragment
    if (auto website = info.GetMetadata(feature::Metadata::EType::FMD_WEBSITE); !website.empty())
      addEntry("Website", std::string(stripSchemeFromURI(website)), true);

    if (auto email = info.GetMetadata(feature::Metadata::EType::FMD_EMAIL); !email.empty())
    {
      data->addWidget(new QLabel("Email"), row, 0);

      QLabel * value = new QLabel(QString::fromStdString("<a href='mailto:" + std::string(email) + "'>" + std::string(email) + "</a>"));
      value->setOpenExternalLinks(true);

      data->addWidget(value, row++, 1);
    }

    // Social networks
    {
      auto addSocialNetworkWidget = [data, &info, &row](const std::string label, const feature::Metadata::EType eType)
      {
        if (auto item = info.GetMetadata(eType); !item.empty())
        {
          data->addWidget(new QLabel(QString::fromStdString(label)), row, 0);

          QLabel * value = new QLabel(QString::fromStdString("<a href='" + osm::socialContactToURL(eType, std::string(item)) + "'>" + std::string(item) + "</a>"));
          value->setOpenExternalLinks(true);
          value->setTextInteractionFlags(Qt::TextBrowserInteraction);

          data->addWidget(value, row++, 1);
        }
      };

      addSocialNetworkWidget("Facebook", feature::Metadata::EType::FMD_CONTACT_FACEBOOK);
      addSocialNetworkWidget("Instagram", feature::Metadata::EType::FMD_CONTACT_INSTAGRAM);
      addSocialNetworkWidget("Twitter", feature::Metadata::EType::FMD_CONTACT_TWITTER);
      addSocialNetworkWidget("VK", feature::Metadata::EType::FMD_CONTACT_VK);
      addSocialNetworkWidget("Line", feature::Metadata::EType::FMD_CONTACT_LINE);
    }

    if (auto wikimedia_commons = info.GetMetadata(feature::Metadata::EType::FMD_WIKIMEDIA_COMMONS); !wikimedia_commons.empty())
    {
      data->addWidget(new QLabel("Wikimedia Commons"), row, 0);

      QLabel * value = new QLabel(QString::fromStdString("<a href='" + feature::Metadata::ToWikimediaCommonsURL(std::string(wikimedia_commons)) + "'>Wikimedia Commons</a>"));
      value->setOpenExternalLinks(true);
      value->setTextInteractionFlags(Qt::TextBrowserInteraction);

      data->addWidget(value, row++, 1);
    }

    // Level fragment
    if (auto level = info.GetMetadata(feature::Metadata::EType::FMD_LEVEL); !level.empty())
      addEntry("Level", std::string(level));

    // ATM fragment
    if (info.HasAtm())
      addEntry("ATM", "Yes");

    // Latlon fragment

    {
      ms::LatLon const ll = info.GetLatLon();
      addEntry("Coordinates", strings::to_string_dac(ll.m_lat, 7) + ", " + strings::to_string_dac(ll.m_lon, 7));
    }

    data->setColumnStretch(0, 0);
    data->setColumnStretch(1, 1);

    innerLayout->addLayout(data);
  }

  innerLayout->addStretch(); 

  {
    QHLine * line = new QHLine();
    innerLayout->addWidget(line);
  }

  scrollArea->setWidget(containerWidget);

  layout->addWidget(scrollArea);

  {
    QDialogButtonBox * dbb = new QDialogButtonBox();
    addCommonButtons(dbb, info);
    layout->addWidget(dbb, Qt::AlignCenter);
  }

  setLayout(layout);
}

void PlacePanel::updateInterfaceDeveloper(place_page::Info const & info,
                                          search::ReverseGeocoder::Address const & address)
{
  DeleteExistingLayout();

  QVBoxLayout * layout = new QVBoxLayout();

  QScrollArea * scrollArea = new QScrollArea();
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  
  QWidget *containerWidget = new QWidget();
  QGridLayout * grid = new QGridLayout(containerWidget);
  containerWidget->setLayout(grid);

  int row = 0;

  auto const addEntry = [grid, &row](std::string const & key, std::string const & value, bool isLink = false)
  {
    grid->addWidget(new QLabel(QString::fromStdString(key)), row, 0);
    QLabel * label = new QLabel(QString::fromStdString(value));
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    if (isLink)
    {
      label->setOpenExternalLinks(true);
      label->setTextInteractionFlags(Qt::TextBrowserInteraction);
      label->setText(QString::fromStdString("<a href=\"" + value + "\">" + value + "</a>"));
    }
    grid->addWidget(label, row++, 1);
    return label;
  };

  {
    ms::LatLon const ll = info.GetLatLon();
    addEntry("lat, lon", strings::to_string_dac(ll.m_lat, 7) + ", " + strings::to_string_dac(ll.m_lon, 7));
  }

  addEntry("CountryId", info.GetCountryId());

  auto const & title = info.GetTitle();
  if (!title.empty())
    addEntry("Title", title);

  if (auto const & subTitle = info.GetSubtitle(); !subTitle.empty())
    addEntry("Subtitle", subTitle);

  addEntry("Address", address.FormatAddress());

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
    addEntry("Feature ID", DebugPrint(info.GetID()));
    addEntry("Raw Types", DebugPrint(info.GetTypes()));
  }

  auto const layer = info.GetLayer();
  if (layer != feature::LAYER_EMPTY)
    addEntry("Layer", std::to_string(layer));

  using PropID = osm::MapObject::MetadataID;

  if (auto cuisines = info.FormatCuisines(); !cuisines.empty())
    addEntry(DebugPrint(PropID::FMD_CUISINE), cuisines);

  QDialogButtonBox * dbb = new QDialogButtonBox();
  addCommonButtons(dbb, info);

  if (auto const & descr = info.GetWikiDescription(); !descr.empty())
  {
    QPushButton * wikiButton = new QPushButton("Wiki");
    connect(wikiButton, &QAbstractButton::clicked, this, [this, descr, title]()
    {
      auto textDialog = TextDialog(this, QString::fromStdString(descr), QString::fromStdString("Wikipedia: " + title));
      textDialog.exec();
    });
    dbb->addButton(wikiButton, QDialogButtonBox::ActionRole);
  }

  info.ForEachMetadataReadable([&addEntry](PropID id, std::string const & value)
  {
    bool isLink = false;
    switch (id)
    {
    case PropID::FMD_EMAIL:
    case PropID::FMD_WEBSITE:
    case PropID::FMD_CONTACT_FACEBOOK:
    case PropID::FMD_CONTACT_INSTAGRAM:
    case PropID::FMD_CONTACT_TWITTER:
    case PropID::FMD_CONTACT_VK:
    case PropID::FMD_CONTACT_LINE:
    case PropID::FMD_WIKIPEDIA:
    case PropID::FMD_WIKIMEDIA_COMMONS:
      isLink = true;
      break;
    default:
      break;
    }

    addEntry(DebugPrint(id), value, isLink);
  });

  // Stretch last row, to make grid appear at the top of the panel.
  grid->setRowStretch(row, 1);
  // Stretch 2nd column
  grid->setColumnStretch(1, 1);

  scrollArea->setWidget(containerWidget);

  layout->addWidget(scrollArea);

  layout->addWidget(dbb);

  setLayout(layout);
}

void PlacePanel::DeleteExistingLayout()
{
  if (this->layout() != nullptr)
  {
    // Delete layout, and all sub-layouts and children widgets.
    // https://stackoverflow.com/questions/7528680/how-to-delete-an-already-existing-layout-on-a-widget
    QWidget().setLayout(this->layout());
  }
}
