protocol PlacePagePresenterProtocol: AnyObject {
  func updatePreviewOffset()
  func layoutIfNeeded()
  func showNextStop()
  func closeAnimated()
  func showAlert(_ alert: UIAlertController)
}

class PlacePagePresenter: NSObject {
  private weak var view: PlacePageViewProtocol!

  init(view: PlacePageViewProtocol) {
    self.view = view
  }
}

// MARK: - PlacePagePresenterProtocol

extension PlacePagePresenter: PlacePagePresenterProtocol {
  func updatePreviewOffset() {
    view.updatePreviewOffset()
  }

  func layoutIfNeeded() {
    view.layoutIfNeeded()
  }

  func showNextStop() {
    view.showNextStop()
  }

  func closeAnimated() {
    view.closeAnimated(completion: nil)
  }

  func showAlert(_ alert: UIAlertController) {
    view.showAlert(alert)
  }
}
