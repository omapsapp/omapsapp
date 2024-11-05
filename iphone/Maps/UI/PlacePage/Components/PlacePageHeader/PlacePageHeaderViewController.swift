protocol PlacePageHeaderViewProtocol: AnyObject {
  var presenter: PlacePageHeaderPresenterProtocol?  { get set }
  var isExpandViewHidden: Bool { get set }
  var isShadowViewHidden: Bool { get set }
  var isShareButtonHidden: Bool { get set }

  func setTitle(_ title: String?, secondaryTitle: String?)
}

class PlacePageHeaderViewController: UIViewController {
  var presenter: PlacePageHeaderPresenterProtocol?

  @IBOutlet private var headerView: PlacePageHeaderView!
  @IBOutlet private var titleLabel: UILabel?
  @IBOutlet private var expandView: UIView!
  @IBOutlet private var shadowView: UIView!
  @IBOutlet private var grabberView: UIView!

  @IBOutlet weak var closeButton: CircleImageButton!
  @IBOutlet weak var shareButton: CircleImageButton!

  private var titleText: String?
  private var secondaryText: String?

  override func viewDidLoad() {
    super.viewDidLoad()
    presenter?.configure()
    let tap = UITapGestureRecognizer(target: self, action: #selector(onExpandPressed(sender:)))
    expandView.addGestureRecognizer(tap)
    headerView.layer.maskedCorners = [.layerMinXMinYCorner, .layerMaxXMinYCorner]
    iPadSpecific { [weak self] in
      self?.grabberView.isHidden = true
    }
    closeButton.setImage(UIImage(named: "ic_close")!)
    shareButton.setImage(UIImage(named: "ic_share")!)
  }

  @objc func onExpandPressed(sender: UITapGestureRecognizer) {
    presenter?.onExpandPress()
  }

  @IBAction private func onCloseButtonPressed(_ sender: Any) {
    presenter?.onClosePress()
  }

  @IBAction func onShareButtonPressed(_ sender: Any) {
    presenter?.onShareButtonPress(from: shareButton)
  }
  
  override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    guard traitCollection.userInterfaceStyle != previousTraitCollection?.userInterfaceStyle else { return }
    setTitle(titleText, secondaryTitle: secondaryText)
  }
}

extension PlacePageHeaderViewController: PlacePageHeaderViewProtocol {
  var isExpandViewHidden: Bool {
    get {
      expandView.isHidden
    }
    set {
      expandView.isHidden = newValue
    }
  }

  var isShadowViewHidden: Bool {
    get {
      shadowView.isHidden
    }
    set {
      shadowView.isHidden = newValue
    }
  }

  var isShareButtonHidden: Bool {
    get {
      shareButton.isHidden
    }
    set {
      shareButton.isHidden = newValue
    }
  }

  func setTitle(_ title: String?, secondaryTitle: String?) {
    titleText = title
    secondaryText = secondaryTitle
    // XCode 13 is not smart enough to detect that title is used below, and requires explicit unwrapped variable.
    guard let unwrappedTitle = title else {
      titleLabel?.attributedText = nil
      return
    }

    let titleAttributes: [NSAttributedString.Key: Any] = [
      .font: StyleManager.shared.theme!.fonts.medium20,
      .foregroundColor: UIColor.blackPrimaryText()
    ]

    let attributedText = NSMutableAttributedString(string: unwrappedTitle, attributes: titleAttributes)

    guard let unwrappedSecondaryTitle = secondaryTitle else {
      titleLabel?.attributedText = attributedText
      return
    }

    let paragraphStyle = NSMutableParagraphStyle()
    paragraphStyle.paragraphSpacingBefore = 2
    let secondaryTitleAttributes: [NSAttributedString.Key: Any] = [
      .font: StyleManager.shared.theme!.fonts.medium16,
      .foregroundColor: UIColor.blackPrimaryText(),
      .paragraphStyle: paragraphStyle
    ]

    attributedText.append(NSAttributedString(string: "\n" + unwrappedSecondaryTitle, attributes: secondaryTitleAttributes))
    titleLabel?.attributedText = attributedText
  }
}
