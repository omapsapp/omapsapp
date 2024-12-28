#pragma once
#include "qt/selection.hpp"

#include "map/routing_mark.hpp"

#include "storage/storage_defines.hpp"

#include "platform/location.hpp"
#include "platform/location_service.hpp"

#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>

#include <array>
#include <string>
#include <memory>

class Framework;
class QDockWidget;
class QLabel;
class QPushButton;

namespace search { class Result; }

namespace qt
{
class DrawWidget;
class PopupMenuHolder;
struct ScreenshotParams;

class MainWindow : public QMainWindow, location::LocationObserver
{
  DrawWidget * m_pDrawWidget = nullptr;
  // TODO(mgsergio): Make indexing more informative.
  std::array<QDockWidget *, 1> m_Docks;

  QPushButton * m_downloadButton = nullptr;
  QPushButton * m_retryButton = nullptr;
  QLabel * m_downloadingStatusLabel = nullptr;

  storage::CountryId m_lastCountry;

  std::unique_ptr<location::LocationService> const m_locationService;
  bool const m_screenshotMode;

  QAction * m_pMyPositionAction = nullptr;
  QAction * m_pCreateFeatureAction = nullptr;
  QAction * m_pSearchAction = nullptr;
  QAction * m_rulerAction = nullptr;

  enum LayerType : uint8_t
  {
    /// @todo Uncomment when we will integrate a traffic provider.
    // TRAFFIC = 0,
    TRANSIT = 0,      // Metro scheme
    ISOLINES,
    OUTDOORS,
  };
  PopupMenuHolder * m_layers = nullptr;
  PopupMenuHolder * m_routing = nullptr;
  PopupMenuHolder * m_selection = nullptr;

#ifdef BUILD_DESIGNER
  QString const m_mapcssFilePath = nullptr;
  QAction * m_pBuildStyleAction = nullptr;
  QAction * m_pRecalculateGeomIndex = nullptr;
  QAction * m_pDrawDebugRectAction = nullptr;
  QAction * m_pGetStatisticsAction = nullptr;
  QAction * m_pRunTestsAction = nullptr;
  QAction * m_pBuildPhonePackAction = nullptr;
#endif // BUILD_DESIGNER

  Q_OBJECT

public:
  MainWindow(Framework & framework, std::unique_ptr<ScreenshotParams> && screenshotParams,
             QRect const & screenGeometry
#ifdef BUILD_DESIGNER
             , QString const & mapcssFilePath = QString()
#endif
            );

protected:
  Framework & GetFramework() const;

  void OnLocationError(location::TLocationError errorCode) override;
  void OnLocationUpdated(location::GpsInfo const & info) override;
  void LocationStateModeChanged(location::EMyPositionMode mode);

  void CreatePanelImpl(size_t i, Qt::DockWidgetArea area, QString const & name,
                       QKeySequence const & hotkey, char const * slot);
  void CreateNavigationBar();
  void CreateSearchBarAndPanel();
  void CreateCountryStatusControls();

  void SetLayerEnabled(LayerType type, bool enable);

#if defined(OMIM_OS_WINDOWS)
  /// to handle menu messages
  bool nativeEvent(QByteArray const & eventType, void * message, qintptr * result) override;
#endif

  void closeEvent(QCloseEvent * e) override;

protected Q_SLOTS:
#ifndef NO_DOWNLOADER
  void ShowUpdateDialog();
#endif // NO_DOWNLOADER

  void OnPreferences();
  void OnAbout();
  void OnMyPosition();
  void OnCreateFeatureClicked();
  void OnSearchButtonClicked();
  void OnLoginMenuItem();
  void OnUploadEditsMenuItem();

  void OnBeforeEngineCreation();

  void OnDownloadClicked();
  void OnRetryDownloadClicked();

  void OnSwitchSelectionMode(SelectionMode mode);
  void OnSwitchMwmsBordersSelectionMode();
  void OnClearSelection();

  void OnLayerEnabled(LayerType layer);

  void OnRulerEnabled();

  void OnRoutePointSelected(RouteMarkType type);
  void OnFollowRoute();
  void OnClearRoute();
  void OnRoutingSettings();

  void OnBookmarksAction();

#ifdef BUILD_DESIGNER
  void OnBuildStyle();
  void OnRecalculateGeomIndex();
  void OnDebugStyle();
  void OnGetStatistics();
  void OnRunTests();
  void OnBuildPhonePackage();
#endif // BUILD_DESIGNER
};
}  // namespace qt
