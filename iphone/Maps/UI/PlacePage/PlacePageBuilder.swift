@objc class PlacePageBuilder: NSObject {    
  @objc static func build() -> PlacePageViewController {
    let storyboard = UIStoryboard.instance(.placePage)
    guard let viewController = storyboard.instantiateInitialViewController() as? PlacePageViewController else {
      fatalError()
    }
    let data = PlacePageData(localizationProvider: OpeinigHoursLocalization())
    viewController.isPreviewPlus = data.isPreviewPlus
    let interactor = PlacePageInteractor(viewController: viewController,
                                         data: data,
                                         mapViewController: MapViewController.shared()!)
    let layout: IPlacePageLayout
    if data.isTrack {
      layout = PlacePageElevationLayout(interactor: interactor, storyboard: storyboard, data: data)
    } else {
      layout = PlacePageCommonLayout(interactor: interactor, storyboard: storyboard, data: data)
    }
    let presenter = PlacePagePresenter(view: viewController)
    viewController.setLayout(layout)
    viewController.interactor = interactor
    interactor.presenter = presenter
    layout.presenter = presenter
    return viewController
	}

  @objc static func update(_ viewController: PlacePageViewController) {
    let data = PlacePageData(localizationProvider: OpeinigHoursLocalization())
    viewController.isPreviewPlus = data.isPreviewPlus
    let interactor = PlacePageInteractor(viewController: viewController,
                                         data: data,
                                         mapViewController: MapViewController.shared()!)
    let layout: IPlacePageLayout
    if data.isTrack {
      layout = PlacePageElevationLayout(interactor: interactor, storyboard: viewController.storyboard!, data: data)
    } else {
      layout = PlacePageCommonLayout(interactor: interactor, storyboard: viewController.storyboard!, data: data)
    }
    let presenter = PlacePagePresenter(view: viewController)
    viewController.interactor = interactor
    interactor.presenter = presenter
    layout.presenter = presenter
    viewController.updateWithLayout(layout)
    viewController.updatePreviewOffset()
  }
}
