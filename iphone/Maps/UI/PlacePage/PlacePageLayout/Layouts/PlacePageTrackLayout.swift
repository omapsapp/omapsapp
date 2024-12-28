class PlacePageTrackLayout: IPlacePageLayout {
  private var placePageData: PlacePageData
  private var trackData: PlacePageTrackData
  private var interactor: PlacePageInteractor
  private let storyboard: UIStoryboard
  weak var presenter: PlacePagePresenterProtocol?

  lazy var bodyViewControllers: [UIViewController] = {
    return configureViewControllers()
  }()

  var actionBar: ActionBarViewController? {
    actionBarViewController
  }

  var navigationBar: UIViewController? {
    placePageNavigationViewController
  }

  lazy var headerViewControllers: [UIViewController] = {
    [headerViewController, previewViewController]
  }()

  lazy var headerViewController: PlacePageHeaderViewController = {
    PlacePageHeaderBuilder.build(data: placePageData.previewData, delegate: interactor, headerType: .flexible)
  }()

  lazy var previewViewController: PlacePagePreviewViewController = {
    let vc = storyboard.instantiateViewController(ofType: PlacePagePreviewViewController.self)
    vc.placePagePreviewData = placePageData.previewData
    return vc
  }()

  lazy var placePageNavigationViewController: PlacePageHeaderViewController = {
    return PlacePageHeaderBuilder.build(data: placePageData.previewData, delegate: interactor, headerType: .fixed)
  }()

  lazy var editTrackViewController: PlacePageEditBookmarkOrTrackViewController = {
    let vc = storyboard.instantiateViewController(ofType: PlacePageEditBookmarkOrTrackViewController.self)
    vc.view.isHidden = true
    vc.delegate = interactor
    return vc
  }()

  lazy var elevationMapViewController: ElevationProfileViewController? = {
    guard trackData.trackInfo.hasElevationInfo(),
          let elevationProfileData = trackData.elevationProfileData else {
      return nil
    }
    return ElevationProfileBuilder.build(trackInfo: trackData.trackInfo,
                                         elevationProfileData: elevationProfileData,
                                         delegate: interactor)
  }()

  lazy var actionBarViewController: ActionBarViewController = {
    let vc = storyboard.instantiateViewController(ofType: ActionBarViewController.self)
    vc.placePageData = placePageData
    vc.canAddStop = MWMRouter.canAddIntermediatePoint()
    vc.isRoutePlanning = MWMNavigationDashboardManager.shared().state != .hidden
    vc.delegate = interactor
    return vc
  }()

  init(interactor: PlacePageInteractor, storyboard: UIStoryboard, data: PlacePageData) {
    self.interactor = interactor
    self.storyboard = storyboard
    self.placePageData = data
    guard let trackData = data.trackData else {
      fatalError("PlacePageData must contain trackData for the PlacePageTrackLayout")
    }
    self.trackData = trackData
  }

  private func configureViewControllers() -> [UIViewController] {
    var viewControllers = [UIViewController]()

    viewControllers.append(editTrackViewController)
    editTrackViewController.view.isHidden = false
    editTrackViewController.data = .track(trackData)

    placePageData.onBookmarkStatusUpdate = { [weak self] in
      guard let self = self else { return }
      self.previewViewController.placePagePreviewData = self.placePageData.previewData
      self.updateTrackRelatedSections()
    }

    if let elevationMapViewController {
      viewControllers.append(elevationMapViewController)
    }

    return viewControllers
  }

  func calculateSteps(inScrollView scrollView: UIScrollView, compact: Bool) -> [PlacePageState] {
    var steps: [PlacePageState] = []
    let scrollHeight = scrollView.height
    steps.append(.closed(-scrollHeight))
    guard elevationMapViewController != nil else {
      steps.append(.full(0))
      return steps
    }
    guard let previewView = previewViewController.view else {
      return steps
    }
    let previewFrame = scrollView.convert(previewView.bounds, from: previewView)
    steps.append(.preview(previewFrame.maxY - scrollHeight))
    if !compact {
      steps.append(.expanded(-scrollHeight * 0.55))
    }
    steps.append(.full(0))
    return steps
  }
}

private extension PlacePageTrackLayout {
  func updateTrackRelatedSections() {
    guard let trackData = placePageData.trackData else {
      presenter?.closeAnimated()
      return
    }
    editTrackViewController.data = .track(trackData)
    let previewData = placePageData.previewData
    if let headerViewController = headerViewControllers.compactMap({ $0 as? PlacePageHeaderViewController }).first {
      headerViewController.setTitle(previewData.title, secondaryTitle: previewData.secondaryTitle)
      placePageNavigationViewController.setTitle(previewData.title, secondaryTitle: previewData.secondaryTitle)
    }
    if let previewViewController = headerViewControllers.compactMap({ $0 as? PlacePagePreviewViewController }).first {
      previewViewController.placePagePreviewData = previewData
      previewViewController.updateViews()
    }
    presenter?.layoutIfNeeded()
  }
}
