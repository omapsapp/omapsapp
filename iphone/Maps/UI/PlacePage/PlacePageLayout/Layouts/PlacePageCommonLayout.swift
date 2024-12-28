class PlacePageCommonLayout: NSObject, IPlacePageLayout {
  
  private let distanceFormatter = DistanceFormatter.self
  private let altitudeFormatter = AltitudeFormatter.self

  private var placePageData: PlacePageData
  private var interactor: PlacePageInteractor
  private let storyboard: UIStoryboard
  weak var presenter: PlacePagePresenterProtocol?

  fileprivate var lastLocation: CLLocation?

  lazy var headerViewControllers: [UIViewController] = {
    [headerViewController, previewViewController]
  }()

  lazy var bodyViewControllers: [UIViewController] = {
    return configureViewControllers()
  }()

  var actionBar: ActionBarViewController? {
    return actionBarViewController
  }

  var navigationBar: UIViewController? {
    return placePageNavigationViewController
  }
  
  lazy var headerViewController: PlacePageHeaderViewController = {
    PlacePageHeaderBuilder.build(data: placePageData.previewData, delegate: interactor, headerType: .flexible)
  }()

  lazy var previewViewController: PlacePagePreviewViewController = {
    let vc = storyboard.instantiateViewController(ofType: PlacePagePreviewViewController.self)
    vc.placePagePreviewData = placePageData.previewData
    return vc
  } ()

  lazy var wikiDescriptionViewController: WikiDescriptionViewController = {
    let vc = storyboard.instantiateViewController(ofType: WikiDescriptionViewController.self)
    vc.view.isHidden = true
    vc.delegate = interactor
    return vc
  } ()

  lazy var editBookmarkViewController: PlacePageEditBookmarkOrTrackViewController = {
    let vc = storyboard.instantiateViewController(ofType: PlacePageEditBookmarkOrTrackViewController.self)
    vc.view.isHidden = true
    vc.delegate = interactor
    return vc
  } ()

  lazy var infoViewController: PlacePageInfoViewController = {
    let vc = storyboard.instantiateViewController(ofType: PlacePageInfoViewController.self)
    vc.placePageInfoData = placePageData.infoData
    vc.delegate = interactor
    return vc
  } ()

  private func productsViewController() -> ProductsViewController? {
    let productsManager = FrameworkHelper.self
    guard let configuration = productsManager.getProductsConfiguration() else { return nil }
    let viewModel = ProductsViewModel(manager: productsManager, configuration: configuration)
    return ProductsViewController(viewModel: viewModel)
  }

  lazy var buttonsViewController: PlacePageButtonsViewController = {
    let vc = storyboard.instantiateViewController(ofType: PlacePageButtonsViewController.self)
    vc.buttonsData = placePageData.buttonsData!
    vc.delegate = interactor
    return vc
  } ()

  lazy var actionBarViewController: ActionBarViewController = {
    let vc = storyboard.instantiateViewController(ofType: ActionBarViewController.self)
    vc.placePageData = placePageData
    vc.canAddStop = MWMRouter.canAddIntermediatePoint()
    vc.isRoutePlanning = MWMNavigationDashboardManager.shared().state != .hidden
    vc.delegate = interactor
    return vc
  } ()

  lazy var placePageNavigationViewController: PlacePageHeaderViewController = {
    return PlacePageHeaderBuilder.build(data: placePageData.previewData, delegate: interactor, headerType: .fixed)
  } ()

  init(interactor: PlacePageInteractor, storyboard: UIStoryboard, data: PlacePageData) {
    self.interactor = interactor
    self.storyboard = storyboard
    self.placePageData = data
  }

  private func configureViewControllers() -> [UIViewController] {
    var viewControllers = [UIViewController]()

    viewControllers.append(wikiDescriptionViewController)
    if let wikiDescriptionHtml = placePageData.wikiDescriptionHtml {
      wikiDescriptionViewController.descriptionHtml = wikiDescriptionHtml
      if placePageData.bookmarkData?.bookmarkDescription == nil {
        wikiDescriptionViewController.view.isHidden = false
      }
    }

    viewControllers.append(editBookmarkViewController)
    if let bookmarkData = placePageData.bookmarkData {
      editBookmarkViewController.data = .bookmark(bookmarkData)
      editBookmarkViewController.view.isHidden = false
    }

    if placePageData.infoData != nil {
      viewControllers.append(infoViewController)
    }

    if let productsViewController = productsViewController() {
      viewControllers.append(productsViewController)
    }

    if placePageData.buttonsData != nil {
      viewControllers.append(buttonsViewController)
    }

    placePageData.onBookmarkStatusUpdate = { [weak self] in
      guard let self = self else { return }
      self.actionBarViewController.updateBookmarkButtonState(isSelected: self.placePageData.bookmarkData != nil)
      self.previewViewController.placePagePreviewData = self.placePageData.previewData
      self.updateBookmarkRelatedSections()
    }

    LocationManager.add(observer: self)
    if let lastLocation = LocationManager.lastLocation() {
      onLocationUpdate(lastLocation)
      self.lastLocation = lastLocation
    }
    if let lastHeading = LocationManager.lastHeading() {
      onHeadingUpdate(lastHeading)
    }

    placePageData.onMapNodeStatusUpdate = { [weak self] in
      guard let self = self else { return }
      self.actionBarViewController.updateDownloadButtonState(self.placePageData.mapNodeAttributes!.nodeStatus)
      switch self.placePageData.mapNodeAttributes!.nodeStatus {
      case .onDisk, .onDiskOutOfDate, .undefined:
        self.actionBarViewController.resetButtons()
        if self.placePageData.buttonsData != nil {
          self.buttonsViewController.buttonsEnabled = true
        }
      default:
        break
      }
    }
    placePageData.onMapNodeProgressUpdate = { [weak self] (downloadedBytes, totalBytes) in
      guard let self = self, let downloadButton = self.actionBarViewController.downloadButton else { return }
      downloadButton.mapDownloadProgress?.progress = CGFloat(downloadedBytes) / CGFloat(totalBytes)
    }

    return viewControllers
  }

  func calculateSteps(inScrollView scrollView: UIScrollView, compact: Bool) -> [PlacePageState] {
    var steps: [PlacePageState] = []
    let scrollHeight = scrollView.height
    steps.append(.closed(-scrollHeight))
    guard let preview = previewViewController.view else {
      return steps
    }
    let previewFrame = scrollView.convert(preview.bounds, from: preview)
    steps.append(.preview(previewFrame.maxY - scrollHeight))
    if !compact {
      if placePageData.isPreviewPlus {
        steps.append(.previewPlus(-scrollHeight * 0.55))
      }
      steps.append(.expanded(-scrollHeight * 0.3))
    }
    steps.append(.full(0))
    return steps
  }
}


// MARK: - PlacePageData async callbacks for loaders

extension PlacePageCommonLayout {
  func updateBookmarkRelatedSections() {
    var isBookmark = false
    if let bookmarkData = placePageData.bookmarkData {
      editBookmarkViewController.data = .bookmark(bookmarkData)
      isBookmark = true
    }
    if let title = placePageData.previewData.title, let headerViewController = headerViewControllers.compactMap({ $0 as? PlacePageHeaderViewController }).first {
      let secondaryTitle = placePageData.previewData.secondaryTitle
      headerViewController.setTitle(title, secondaryTitle: secondaryTitle)
      placePageNavigationViewController.setTitle(title, secondaryTitle: secondaryTitle)
    }
    presenter?.layoutIfNeeded()
    UIView.animate(withDuration: kDefaultAnimationDuration) { [unowned self] in
      self.editBookmarkViewController.view.isHidden = !isBookmark
    }
  }
}

// MARK: - MWMLocationObserver

extension PlacePageCommonLayout: MWMLocationObserver {
  func onHeadingUpdate(_ heading: CLHeading) {
    if !placePageData.isMyPosition {
      updateHeading(heading.trueHeading)
    }
  }

  func onLocationUpdate(_ location: CLLocation) {
    if placePageData.isMyPosition {
      let altString = "▲ \(altitudeFormatter.altitudeString(fromMeters: location.altitude))"
      if location.speed > 0 && location.timestamp.timeIntervalSinceNow >= -2 {
        let speedMeasure = Measure.init(asSpeed: location.speed)
        let speedString = "\(LocationManager.speedSymbolFor(location.speed))\(speedMeasure.valueAsString) \(speedMeasure.unit)"
        previewViewController.updateSpeedAndAltitude("\(altString)  \(speedString)")
      } else {
        previewViewController.updateSpeedAndAltitude(altString)
      }
    } else {
      let ppLocation = CLLocation(latitude: placePageData.locationCoordinate.latitude,
                                  longitude: placePageData.locationCoordinate.longitude)
      let distance = location.distance(from: ppLocation)
      let formattedDistance = distanceFormatter.distanceString(fromMeters: distance)
      previewViewController.updateDistance(formattedDistance)

      lastLocation = location
    }
  }

  func onLocationError(_ locationError: MWMLocationStatus) {

  }

  private func updateHeading(_ heading: CLLocationDirection) {
    guard let location = lastLocation, heading > 0 else {
      return
    }

    let rad = heading * Double.pi / 180
    let angle = GeoUtil.angle(atPoint: location.coordinate, toPoint: placePageData.locationCoordinate)
    previewViewController.updateHeading(CGFloat(angle) + CGFloat(rad))
  }
}
