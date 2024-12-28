protocol PlacePageHeaderPresenterProtocol: AnyObject {
  func configure()
  func onClosePress()
  func onExpandPress()
  func onShareButtonPress(from sourceView: UIView)
}

protocol PlacePageHeaderViewControllerDelegate: AnyObject {
  func previewDidPressClose()
  func previewDidPressExpand()
  func previewDidPressShare(from sourceView: UIView)
}

class PlacePageHeaderPresenter {
  enum HeaderType {
    case flexible
    case fixed
  }

  private weak var view: PlacePageHeaderViewProtocol?
  private let placePagePreviewData: PlacePagePreviewData
  private weak var delegate: PlacePageHeaderViewControllerDelegate?
  private let headerType: HeaderType

  init(view: PlacePageHeaderViewProtocol,
       placePagePreviewData: PlacePagePreviewData,
       delegate: PlacePageHeaderViewControllerDelegate?,
       headerType: HeaderType) {
    self.view = view
    self.delegate = delegate
    self.placePagePreviewData = placePagePreviewData
    self.headerType = headerType
  }
}

extension PlacePageHeaderPresenter: PlacePageHeaderPresenterProtocol {
  func configure() {
    view?.setTitle(placePagePreviewData.title, secondaryTitle: placePagePreviewData.secondaryTitle)
    switch headerType {
    case .flexible:
      view?.isExpandViewHidden = false
      view?.isShadowViewHidden = true
    case .fixed:
      view?.isExpandViewHidden = true
      view?.isShadowViewHidden = false
    }
    // TODO: (KK) Enable share button for the tracks to share the whole track gpx/kml
    view?.isShareButtonHidden = placePagePreviewData.coordinates == nil
  }

  func onClosePress() {
    delegate?.previewDidPressClose()
  }

  func onExpandPress() {
    delegate?.previewDidPressExpand()
  }

  func onShareButtonPress(from sourceView: UIView) {
    delegate?.previewDidPressShare(from: sourceView)
  }
}
