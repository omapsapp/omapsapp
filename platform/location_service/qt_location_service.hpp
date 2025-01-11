#pragma once

#include "platform/location_service/location_service.hpp"

#include <QGeoPositionInfoSource>

class QtLocationService : public QObject, public location::LocationService
{
  Q_OBJECT
  QGeoPositionInfoSource *m_positionSource;
  // Unfortunately when the source is `geoclue2`
  // we would need access to the `Active` D-Bus property
  // https://www.freedesktop.org/software/geoclue/docs
  // /gdbus-org.freedesktop.GeoClue2.Client.html#gdbus-property-org-freedesktop-GeoClue2-Client.Active
  // But `QGeoPositionInfoSource` doesn't expose that so we have to deduce its state.
  bool m_clientIsActive = false;

public:
  explicit QtLocationService(location::LocationObserver &, std::string const &);
  virtual ~QtLocationService() {};
  virtual void Start();
  virtual void Stop();

public slots:
  void OnLocationUpdate(QGeoPositionInfo const &);
  void OnErrorOccurred(QGeoPositionInfoSource::Error);
  void OnSupportedPositioningMethodsChanged();
};
