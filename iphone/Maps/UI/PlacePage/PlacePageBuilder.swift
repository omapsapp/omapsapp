@objc class PlacePageBuilder: NSObject {    
  @objc static func build(for data: PlacePageData) -> PlacePageViewController {
    let storyboard = UIStoryboard.instance(.placePage)
    guard let viewController = storyboard.instantiateInitialViewController() as? PlacePageViewController else {
      fatalError()
    }
    viewController.isPreviewPlus = data.isPreviewPlus
    let interactor = PlacePageInteractor(viewController: viewController,
                                         data: data,
                                         mapViewController: MapViewController.shared()!)
    let layout: IPlacePageLayout
    switch data.objectType {
    case .POI, .bookmark:
      layout = PlacePageCommonLayout(interactor: interactor, storyboard: storyboard, data: data)
    case .track:
      layout = PlacePageTrackLayout(interactor: interactor, storyboard: storyboard, data: data)
    case .trackRecording:
      // TODO: Implement PlacePageTrackRecordingLayout
      fatalError("PlacePageTrackRecordingLayout is not implemented")
    @unknown default:
      fatalError()
    }
    let presenter = PlacePagePresenter(view: viewController)
    viewController.setLayout(layout)
    viewController.interactor = interactor
    interactor.presenter = presenter
    layout.presenter = presenter
    return viewController
	}

  @objc static func update(_ viewController: PlacePageViewController, with data: PlacePageData) {
    viewController.isPreviewPlus = data.isPreviewPlus
    let interactor = PlacePageInteractor(viewController: viewController,
                                         data: data,
                                         mapViewController: MapViewController.shared()!)
    let layout: IPlacePageLayout
    switch data.objectType {
    case .POI, .bookmark:
      layout = PlacePageCommonLayout(interactor: interactor, storyboard: viewController.storyboard!, data: data)
    case .track:
      layout = PlacePageTrackLayout(interactor: interactor, storyboard: viewController.storyboard!, data: data)
    case .trackRecording:
      // TODO: Implement PlacePageTrackRecordingLayout
      fatalError("PlacePageTrackRecordingLayout is not implemented")
    @unknown default:
      fatalError()
    }
    let presenter = PlacePagePresenter(view: viewController)
    viewController.interactor = interactor
    interactor.presenter = presenter
    layout.presenter = presenter
    viewController.updateWithLayout(layout)
    viewController.updatePreviewOffset()
  }
}
