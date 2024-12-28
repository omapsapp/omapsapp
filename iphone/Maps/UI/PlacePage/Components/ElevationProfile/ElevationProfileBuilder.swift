import CoreApi

class ElevationProfileBuilder {
  static func build(trackInfo: TrackInfo,
                    elevationProfileData: ElevationProfileData?,
                    delegate: ElevationProfileViewControllerDelegate?) -> ElevationProfileViewController {
    let storyboard = UIStoryboard.instance(.placePage)
    let viewController = storyboard.instantiateViewController(ofType: ElevationProfileViewController.self);
    let presenter = ElevationProfilePresenter(view: viewController,
                                              trackInfo: trackInfo,
                                              profileData: elevationProfileData,
                                              delegate: delegate)
    
    viewController.presenter = presenter

    return viewController
  }
}
