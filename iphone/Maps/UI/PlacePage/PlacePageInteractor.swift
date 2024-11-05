protocol PlacePageInteractorProtocol: AnyObject {
  func viewWillAppear()
  func updateTopBound(_ bound: CGFloat, duration: TimeInterval)
}

class PlacePageInteractor: NSObject {
  var presenter: PlacePagePresenterProtocol?
  weak var viewController: UIViewController?
  weak var mapViewController: MapViewController?
  private let bookmarksManager = BookmarksManager.shared()
  private var placePageData: PlacePageData
  private var viewWillAppearIsCalledForTheFirstTime = false

  init(viewController: UIViewController, data: PlacePageData, mapViewController: MapViewController) {
    self.placePageData = data
    self.viewController = viewController
    self.mapViewController = mapViewController
    super.init()
    addToBookmarksManagerObserverList()
  }

  deinit {
    removeFromBookmarksManagerObserverList()
  }

  private func updateBookmarkIfNeeded() {
    if placePageData.isTrack {
      guard let trackId = placePageData.trackData?.trackId, bookmarksManager.hasTrack(trackId) else {
        presenter?.closeAnimated()
        return
      }
    } else {
      guard let bookmarkId = placePageData.bookmarkData?.bookmarkId, bookmarksManager.hasBookmark(bookmarkId) else {
        return
      }
      FrameworkHelper.updatePlacePageData()
      placePageData.updateBookmarkStatus()
    }
  }

  private func addToBookmarksManagerObserverList() {
    bookmarksManager.add(self)
  }

  private func removeFromBookmarksManagerObserverList() {
    bookmarksManager.remove(self)
  }
}

extension PlacePageInteractor: PlacePageInteractorProtocol {
  func viewWillAppear() {
    // Skip data reloading on the first appearance, to avoid unnecessary updates.
    guard viewWillAppearIsCalledForTheFirstTime else {
      viewWillAppearIsCalledForTheFirstTime = true
      return
    }
    updateBookmarkIfNeeded()
  }

  func updateTopBound(_ bound: CGFloat, duration: TimeInterval) {
    mapViewController?.setPlacePageTopBound(bound, duration: duration)
  }
}

// MARK: - PlacePageInfoViewControllerDelegate

extension PlacePageInteractor: PlacePageInfoViewControllerDelegate {
  var shouldShowOpenInApp: Bool {
    !OpenInApplication.availableApps.isEmpty
  }

  func didPressCall() {
    MWMPlacePageManagerHelper.call(placePageData)
  }

  func didPressWebsite() {
    MWMPlacePageManagerHelper.openWebsite(placePageData)
  }

  func didPressWebsiteMenu() {
    MWMPlacePageManagerHelper.openWebsiteMenu(placePageData)
  }

  func didPressKayak() {
    let kUDDidShowKayakInformationDialog = "kUDDidShowKayakInformationDialog"
    
    if UserDefaults.standard.bool(forKey: kUDDidShowKayakInformationDialog) {
      MWMPlacePageManagerHelper.openKayak(placePageData)
    } else { 
      let alert = UIAlertController(title: nil, message: L("dialog_kayak_disclaimer"), preferredStyle: .alert)
      alert.addAction(UIAlertAction(title: L("cancel"), style: .cancel))
      alert.addAction(UIAlertAction(title: L("dialog_kayak_button"), style: .default, handler: { _ in
        UserDefaults.standard.set(true, forKey: kUDDidShowKayakInformationDialog)
        MWMPlacePageManagerHelper.openKayak(self.placePageData)
      }))
      presenter?.showAlert(alert)
    }
  }

  func didPressWikipedia() {
    MWMPlacePageManagerHelper.openWikipedia(placePageData)
  }
  
  func didPressWikimediaCommons() {
    MWMPlacePageManagerHelper.openWikimediaCommons(placePageData)
  }
  
  func didPressFacebook() {
    MWMPlacePageManagerHelper.openFacebook(placePageData)
  }
  
  func didPressInstagram() {
    MWMPlacePageManagerHelper.openInstagram(placePageData)
  }

  func didPressTwitter() {
    MWMPlacePageManagerHelper.openTwitter(placePageData)
  }
  
  func didPressVk() {
    MWMPlacePageManagerHelper.openVk(placePageData)
  }
  
  func didPressLine() {
    MWMPlacePageManagerHelper.openLine(placePageData)
  }
  
  func didPressEmail() {
    MWMPlacePageManagerHelper.openEmail(placePageData)
  }
  
  func didCopy(_ content: String) {
    UIPasteboard.general.string = content
    let message = String(format: L("copied_to_clipboard"), content)
    UIImpactFeedbackGenerator(style: .medium).impactOccurred()
    Toast.toast(withText: message).show(withAlignment: .bottom)
  }

  func didPressOpenInApp(from sourceView: UIView) {
    let availableApps = OpenInApplication.availableApps
    guard !availableApps.isEmpty else {
      LOG(.warning, "Applications selection sheet should not be presented when the list of available applications is empty.")
      return
    }
    let openInAppActionSheet = UIAlertController.presentInAppActionSheet(from: sourceView, apps: availableApps) { [weak self] selectedApp in
      guard let self else { return }
      let link = selectedApp.linkWith(coordinates: self.placePageData.locationCoordinate, destinationName: self.placePageData.previewData.title)
      self.mapViewController?.openUrl(link, externally: true)
    }
    presenter?.showAlert(openInAppActionSheet)
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

  func trackDidPressEdit() {
    MWMPlacePageManagerHelper.editTrack(placePageData)
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
    case .avoidToll:
      MWMPlacePageManagerHelper.avoidToll()
    case .avoidDirty:
      MWMPlacePageManagerHelper.avoidDirty()
    case .avoidFerry:
      MWMPlacePageManagerHelper.avoidFerry()
    case .more:
      fatalError("More button should've been handled in ActionBarViewContoller")
    case .track:
      guard placePageData.trackData != nil else { return }
      MWMPlacePageManagerHelper.removeTrack(placePageData)
      presenter?.closeAnimated()
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

  func updateMapPoint(_ point: CLLocationCoordinate2D, distance: Double) {
    guard let trackId = placePageData.trackData?.trackId else { return }
    BookmarksManager.shared().setElevationActivePoint(point, distance: distance, trackId: trackId)
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

  func previewDidPressShare(from sourceView: UIView) {
    guard let mapViewController else { return }
    let shareViewController = ActivityViewController.share(forPlacePage: placePageData)
    shareViewController.present(inParentViewController: mapViewController, anchorView: sourceView)
  }
}

// MARK: - BookmarksObserver
extension PlacePageInteractor: BookmarksObserver {
  func onBookmarksLoadFinished() {
    updateBookmarkIfNeeded()
  }

  func onBookmarksCategoryDeleted(_ groupId: MWMMarkGroupID) {
    guard let bookmarkGroupId = placePageData.bookmarkData?.bookmarkGroupId else { return }
    if bookmarkGroupId == groupId {
      presenter?.closeAnimated()
    }
  }
}

