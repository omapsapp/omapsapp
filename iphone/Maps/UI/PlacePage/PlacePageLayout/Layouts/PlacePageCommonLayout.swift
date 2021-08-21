class PlacePageCommonLayout: NSObject, IPlacePageLayout {
  private lazy var distanceFormatter: MKDistanceFormatter = {
    let formatter =  MKDistanceFormatter()
    formatter.unitStyle = .abbreviated
    formatter.units = Settings.measurementUnits() == .imperial ? .imperial : .metric
    return formatter
  }()

  private lazy var unitsFormatter: MeasurementFormatter = {
    let formatter = MeasurementFormatter()
    formatter.unitOptions = [.providedUnit]
    return formatter
  }()

  private var placePageData: PlacePageData
  private var interactor: PlacePageInteractor
  private let storyboard: UIStoryboard
  weak var presenter: PlacePagePresenterProtocol?

  fileprivate var lastLocation: CLLocation?

  lazy var viewControllers: [UIViewController] = {
    return configureViewControllers()
  }()

  var actionBar: ActionBarViewController? {
    return actionBarViewController
  }

  var navigationBar: UIViewController? {
    return placePageNavigationViewController
  }

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

  lazy var bookmarkViewController: PlacePageBookmarkViewController = {
    let vc = storyboard.instantiateViewController(ofType: PlacePageBookmarkViewController.self)
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
  
  lazy var buttonsViewController: PlacePageButtonsViewController = {
    let vc = storyboard.instantiateViewController(ofType: PlacePageButtonsViewController.self)
    vc.buttonsData = placePageData.buttonsData!
    vc.buttonsEnabled = placePageData.mapNodeAttributes?.nodeStatus == .onDisk
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

  lazy var header: PlacePageHeaderViewController? = {
    return PlacePageHeaderBuilder.build(data: placePageData.previewData, delegate: interactor, headerType: .flexible)
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
    viewControllers.append(previewViewController)
    viewControllers.append(wikiDescriptionViewController)
    if let wikiDescriptionHtml = placePageData.wikiDescriptionHtml {
      wikiDescriptionViewController.descriptionHtml = wikiDescriptionHtml
      if placePageData.bookmarkData?.bookmarkDescription == nil {
        wikiDescriptionViewController.view.isHidden = false
      }
    }

    viewControllers.append(bookmarkViewController)
    if let bookmarkData = placePageData.bookmarkData {
      bookmarkViewController.bookmarkData = bookmarkData
      bookmarkViewController.view.isHidden = false
    }

    if placePageData.infoData != nil {
      viewControllers.append(infoViewController)
    }

    if placePageData.buttonsData != nil {
      viewControllers.append(buttonsViewController)
    }
    
    placePageData.onBookmarkStatusUpdate = { [weak self] in
      guard let self = self else { return }
      if self.placePageData.bookmarkData == nil {
        self.actionBarViewController.resetButtons()
      }
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
      bookmarkViewController.bookmarkData = bookmarkData
      isBookmark = true
    }
    if let title = placePageData.previewData.title {
      header?.setTitle(title)
      placePageNavigationViewController.setTitle(title)
    }
    self.presenter?.layoutIfNeeded()
    UIView.animate(withDuration: kDefaultAnimationDuration) { [unowned self] in
      self.bookmarkViewController.view.isHidden = !isBookmark
      self.presenter?.layoutIfNeeded()
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
      let imperial = Settings.measurementUnits() == .imperial
      let alt = imperial ? location.altitude / 0.3048 : location.altitude
      let altMeasurement = Measurement(value: alt.rounded(), unit: imperial ? UnitLength.feet : UnitLength.meters)
      let altString = "▲ \(unitsFormatter.string(from: altMeasurement))"

      if location.speed > 0 && location.timestamp.timeIntervalSinceNow >= -2 {
        let speed = imperial ? location.speed * 2.237 : location.speed * 3.6
        let speedMeasurement = Measurement(value: speed.rounded(), unit: imperial ? UnitSpeed.milesPerHour: UnitSpeed.kilometersPerHour)
        let speedString = "\(LocationManager.speedSymbolFor(location.speed))\(unitsFormatter.string(from: speedMeasurement))"
        previewViewController.updateSpeedAndAltitude("\(altString)  \(speedString)")
      } else {
        previewViewController.updateSpeedAndAltitude(altString)
      }
    } else {
      let ppLocation = CLLocation(latitude: placePageData.locationCoordinate.latitude,
                                  longitude: placePageData.locationCoordinate.longitude)
      let distance = location.distance(from: ppLocation)
      let formattedDistance = distanceFormatter.string(fromDistance: distance)
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
