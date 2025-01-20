#pragma once

#include "search/reverse_geocoder.hpp"
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <qwidget.h>

namespace place_page
{
class Info;
}

namespace place_panel
{
enum PressedButton : int 
{
  Close = QDialog::Rejected,
  RouteFrom,
  AddStop,
  RouteTo,
  EditPlace
};

}

class PlacePanel : public QWidget
{
  Q_OBJECT

public:
  PlacePanel(QWidget * parent);

  void setPlace(place_page::Info const & info, search::ReverseGeocoder::Address const & address);

signals:
  void showPlace();
  void hidePlace();
  void routeFrom(place_page::Info const & info);
  void addStop(place_page::Info const & info);
  void routeTo(place_page::Info const & info);
  void editPlace(place_page::Info const & info);

protected:
  void addCommonButtons(QDialogButtonBox * dbb, place_page::Info const & info);

private:
  void updateInterfaceDeveloper(place_page::Info const & info, search::ReverseGeocoder::Address const & address);
  void updateInterfaceUser(place_page::Info const & info, search::ReverseGeocoder::Address const & address);

  void DeleteExistingLayout();
};
