import CoreApi

class ElevationProfileBuilder {
  static func build(trackInfo: TrackInfo,
                    elevationProfileData: ElevationProfileData?,
                    delegate: ElevationProfileViewControllerDelegate?) -> ElevationProfileViewController {
    let viewController = ElevationProfileViewController();
    let presenter = ElevationProfilePresenter(view: viewController,
                                              trackInfo: trackInfo,
                                              profileData: elevationProfileData,
                                              delegate: delegate)
    viewController.presenter = presenter
    return viewController
  }
}
