final class PlacePageTrackRecordingLayout: IPlacePageLayout {
  private var placePageData: PlacePageData
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
    [headerViewController]
  }()

  lazy var headerViewController: PlacePageHeaderViewController = {
    return PlacePageHeaderBuilder.build(data: placePageData.previewData, delegate: interactor, headerType: .flexible)
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

  lazy var elevationProfileViewController: ElevationProfileViewController? = {
    guard let trackData = placePageData.trackData else {
      return nil
    }
    return ElevationProfileBuilder.build(trackInfo: trackData.trackInfo,
                                         elevationProfileData: trackData.elevationProfileData,
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
  }

  private func configureViewControllers() -> [UIViewController] {
    var viewControllers = [UIViewController]()

    if let elevationProfileViewController {
      viewControllers.append(elevationProfileViewController)
    }

    placePageData.onTrackRecordingProgressUpdate = { [weak self] in
      self?.updateTrackRecordingRelatedSections()
    }

    return viewControllers
  }

  func calculateSteps(inScrollView scrollView: UIScrollView, compact: Bool) -> [PlacePageState] {
    var steps: [PlacePageState] = []
    let scrollHeight = scrollView.height
    steps.append(.closed(-scrollHeight))
    steps.append(.full(0))
    return steps
  }
}

private extension PlacePageTrackRecordingLayout {
  func updateTrackRecordingRelatedSections() {
    guard let elevationProfileViewController, let trackInfo = placePageData.trackData?.trackInfo else { return }
    headerViewController.setTitle(placePageData.previewData.title, secondaryTitle: nil)
    elevationProfileViewController.presenter?.update(trackInfo: trackInfo, profileData: placePageData.trackData?.elevationProfileData)
    presenter?.layoutIfNeeded()
  }
}
