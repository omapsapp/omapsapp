protocol ElevationDetailsPresenterProtocol: AnyObject {
  func configure()
  func onOkButtonPressed()
}

class ElevationDetailsPresenter {
  private weak var view: ElevationDetailsViewProtocol?
  private let router: ElevationDetailsRouterProtocol
  private let data: ElevationProfileData

  init(view: ElevationDetailsViewProtocol,
       router: ElevationDetailsRouterProtocol,
       data: ElevationProfileData) {
    self.view = view
    self.router = router
    self.data = data
  }
}

extension ElevationDetailsPresenter: ElevationDetailsPresenterProtocol {
  func configure() {
    view?.setDifficulty(data.difficulty)
  }

  func onOkButtonPressed() {
    router.close()
  }
}
