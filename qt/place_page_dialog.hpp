#pragma once

#include "map/routing_mark.hpp"
#include "search/reverse_geocoder.hpp"

#include <QtWidgets/QDialog>

namespace place_page
{
class Info;
}

class PlacePageDialog : public QDialog
{
  Q_OBJECT
public:
  enum PressedButton : int {
    RouteFrom,
    AddStop,
    RouteTo,
    Close,
    EditPlace
  };

  PlacePageDialog(QWidget * parent, place_page::Info const & info,
                  search::ReverseGeocoder::Address const & address);
};
