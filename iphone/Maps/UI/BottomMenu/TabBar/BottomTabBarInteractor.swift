protocol BottomTabBarInteractorProtocol: AnyObject {
  func openSearch()
  func openHelp()
  func openFaq()
  func openBookmarks()
  func openMenu()
}

class BottomTabBarInteractor {
  weak var presenter: BottomTabBarPresenterProtocol?
  private weak var viewController: UIViewController?
  private weak var mapViewController: MapViewController?
  private weak var controlsManager: MWMMapViewControlsManager?
  private weak var searchManager = MWMSearchManager.manager()
  
  init(viewController: UIViewController, mapViewController: MapViewController, controlsManager: MWMMapViewControlsManager) {
    self.viewController = viewController
    self.mapViewController = mapViewController
    self.controlsManager = controlsManager
  }
}

extension BottomTabBarInteractor: BottomTabBarInteractorProtocol {
  func openSearch() {
//    if searchManager?.state == .hidden {
//      searchManager?.state = .default
//    } else {
//      searchManager?.state = .hidden
//    }
//    let vc = SearchViewController()
//    vc.modalPresentationStyle = .overCurrentContext
//    vc.modalTransitionStyle = .coverVertical
//    MapViewController.shared()?.navigationController?.present(vc, animated: true)

    let vc = SearchViewController()
    vc.modalPresentationStyle = .custom
    let trans = SearchCoverVerticalModalTransitioning()
    vc.transitioningDelegate = trans
    MapViewController.shared()?.navigationController?.present(vc, animated: true)
  }
  
  func openHelp() {
    MapViewController.shared()?.navigationController?.pushViewController(AboutController(), animated: true)
  }
  
  func openFaq() {
    guard let navigationController = MapViewController.shared()?.navigationController else { return }
    let aboutController = AboutController(onDidAppearCompletionHandler: {
      navigationController.pushViewController(FaqController(), animated: true)
    })
    navigationController.pushViewController(aboutController, animated: true)
  }
  
  func openBookmarks() {
    mapViewController?.bookmarksCoordinator.open()
  }
  
  func openMenu() {
    guard let state = controlsManager?.menuState else {
      fatalError("ERROR: Failed to retrieve the current MapViewControlsManager's state.")
    }
    switch state {
    case .inactive: controlsManager?.menuState = .active
    case .active: controlsManager?.menuState = .inactive
    case .hidden:
      // When the current controls manager's state is hidden, accidental taps on the menu button during the hiding animation should be skipped.
      break;
    case .layers: fallthrough
    @unknown default: fatalError("ERROR: Unexpected MapViewControlsManager's state: \(state)")
    }
  }
}
