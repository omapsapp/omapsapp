import CoreApi

class ElevationProfileBuilder {
  static func build(elevationProfileData: ElevationProfileData,
                    delegate: ElevationProfileViewControllerDelegate?) -> ElevationProfileViewController {
    let storyboard = UIStoryboard.instance(.placePage)
    let viewController = storyboard.instantiateViewController(ofType: ElevationProfileViewController.self);
    let presenter = ElevationProfilePresenter(view: viewController,
                                              data: elevationProfileData,
                                              delegate: delegate)
    
    viewController.presenter = presenter

    return viewController
  }
}
