@objc class ElevationDetailsBuilder: NSObject {
  @objc static func build(data: PlacePageData) -> UIViewController {
    guard let elevationProfileData = data.trackData?.elevationProfileData else {
      LOG(.critical, "Elevation profile data should not be nil when building elevation details")
      fatalError()
    }
    let viewController = ElevationDetailsViewController(nibName: toString(ElevationDetailsViewController.self), bundle: nil)
    let router = ElevationDetailsRouter(viewController: viewController)
    let presenter = ElevationDetailsPresenter(view: viewController, router: router, data: elevationProfileData)
    viewController.presenter = presenter
    return viewController
  }
}
