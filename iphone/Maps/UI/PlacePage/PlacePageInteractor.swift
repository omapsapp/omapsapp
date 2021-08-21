protocol PlacePageInteractorProtocol: AnyObject {
  func updateTopBound(_ bound: CGFloat, duration: TimeInterval)
}

class PlacePageInteractor {
  weak var presenter: PlacePagePresenterProtocol?
  weak var viewController: UIViewController?
  weak var mapViewController: MapViewController?

  private var placePageData: PlacePageData

  init (viewController: UIViewController, data: PlacePageData, mapViewController: MapViewController) {
    self.placePageData = data
    self.viewController = viewController
    self.mapViewController = mapViewController
  }
}

extension PlacePageInteractor: PlacePageInteractorProtocol {
  func updateTopBound(_ bound: CGFloat, duration: TimeInterval) {
    mapViewController?.setPlacePageTopBound(bound, duration: duration)
  }
}

// MARK: - PlacePageInfoViewControllerDelegate

extension PlacePageInteractor: PlacePageInfoViewControllerDelegate {
  func didPressCall() {
    MWMPlacePageManagerHelper.call(placePageData)
  }

  func didPressWebsite() {
    MWMPlacePageManagerHelper.openWebsite(placePageData)
  }

  func didPressEmail() {

  }
}

// MARK: - WikiDescriptionViewControllerDelegate

extension PlacePageInteractor: WikiDescriptionViewControllerDelegate {
  func didPressMore() {
    MWMPlacePageManagerHelper.showPlaceDescription(placePageData.wikiDescriptionHtml)
  }
}

// MARK: - PlacePageButtonsViewControllerDelegate

extension PlacePageInteractor: PlacePageButtonsViewControllerDelegate {
  func didPressHotels() {
    MWMPlacePageManagerHelper.openDescriptionUrl(placePageData)
  }

  func didPressAddPlace() {
    MWMPlacePageManagerHelper.addPlace(placePageData.locationCoordinate)
  }

  func didPressEditPlace() {
    MWMPlacePageManagerHelper.editPlace()
  }

  func didPressAddBusiness() {
    MWMPlacePageManagerHelper.addBusiness()
  }
}

// MARK: - PlacePageBookmarkViewControllerDelegate

extension PlacePageInteractor: PlacePageBookmarkViewControllerDelegate {
  func bookmarkDidPressEdit() {
    MWMPlacePageManagerHelper.editBookmark(placePageData)
  }
}

// MARK: - ActionBarViewControllerDelegate

extension PlacePageInteractor: ActionBarViewControllerDelegate {
  func actionBar(_ actionBar: ActionBarViewController, didPressButton type: ActionBarButtonType) {
    switch type {
    case .booking:
      MWMPlacePageManagerHelper.book(placePageData)
    case .bookingSearch:
      MWMPlacePageManagerHelper.searchBookingHotels(placePageData)
    case .bookmark:
      if placePageData.bookmarkData != nil {
        MWMPlacePageManagerHelper.removeBookmark(placePageData)
      } else {
        MWMPlacePageManagerHelper.addBookmark(placePageData)
      }
    case .call:
      MWMPlacePageManagerHelper.call(placePageData)
    case .download:
      guard let mapNodeAttributes = placePageData.mapNodeAttributes else {
        fatalError("Download button can't be displayed if mapNodeAttributes is empty")
      }
      switch mapNodeAttributes.nodeStatus {
      case .downloading, .inQueue, .applying:
        Storage.shared().cancelDownloadNode(mapNodeAttributes.countryId)
      case .notDownloaded, .partly, .error:
        Storage.shared().downloadNode(mapNodeAttributes.countryId)
      case .undefined, .onDiskOutOfDate, .onDisk:
        fatalError("Download button shouldn't be displayed when node is in these states")
      @unknown default:
        fatalError()
      }
    case .opentable:
      fatalError("Opentable is not supported and will be deleted")
    case .routeAddStop:
      MWMPlacePageManagerHelper.routeAddStop(placePageData)
    case .routeFrom:
      MWMPlacePageManagerHelper.route(from: placePageData)
    case .routeRemoveStop:
      MWMPlacePageManagerHelper.routeRemoveStop(placePageData)
    case .routeTo:
      MWMPlacePageManagerHelper.route(to: placePageData)
    case .share:
      if let shareVC = ActivityViewController.share(forPlacePage: placePageData), let mvc = MapViewController.shared() {
        shareVC.present(inParentViewController: mvc, anchorView: actionBar.popoverSourceView)
      }
    case .avoidToll:
      MWMPlacePageManagerHelper.avoidToll()
    case .avoidDirty:
      MWMPlacePageManagerHelper.avoidDirty()
    case .avoidFerry:
      MWMPlacePageManagerHelper.avoidFerry()
    case .more:
      fatalError("More button should've been handled in ActionBarViewContoller")
    @unknown default:
      fatalError()
    }
  }
}

// MARK: - ElevationProfileViewControllerDelegate

extension PlacePageInteractor: ElevationProfileViewControllerDelegate {
  func openDifficultyPopup() {
    MWMPlacePageManagerHelper.openElevationDifficultPopup(placePageData)
  }

  func updateMapPoint(_ distance: Double) {
    BookmarksManager.shared().setElevationActivePoint(distance, trackId: placePageData.elevationProfileData!.trackId)
  }
}

// MARK: - PlacePageHeaderViewController

extension PlacePageInteractor: PlacePageHeaderViewControllerDelegate {
  func previewDidPressClose() {
    presenter?.closeAnimated()
  }

  func previewDidPressExpand() {
    presenter?.showNextStop()
  }
}
