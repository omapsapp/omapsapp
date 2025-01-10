final class RouteManagerCell: MWMTableViewCell {
  @IBOutlet private weak var typeImage: UIImageView!
  @IBOutlet private weak var titleLabel: UILabel!
  @IBOutlet weak var subtitleLabel: UILabel!
  @IBOutlet private weak var dragImage: UIImageView! {
    didSet {
      dragImage.image = #imageLiteral(resourceName: "ic_route_manager_move")
      dragImage.tintColor = UIColor.blackHintText()
    }
  }

  @IBOutlet private weak var separator1: UIView! {
    didSet {
      separator1.layer.cornerRadius = 2
    }
  }

  @IBOutlet weak var separator2: UIView!
  private var index: Int!
  private var model: MWMRoutePoint!
  @IBOutlet var subtitleConstraints: [NSLayoutConstraint]!

  override var snapshot: UIView {
    let skipViews: [UIView] = [typeImage, separator1, separator2]
    skipViews.forEach { $0.isHidden = true }
    let snapshot = super.snapshot
    setStyle(.global(.background))
    skipViews.forEach { $0.isHidden = false }
    return snapshot
  }

  func set(model: MWMRoutePoint, atIndex index: Int) {
    self.model = model
    self.index = index

    setupTypeImage()
    setupLabels()
    setupSeparators()
  }

  private func setupTypeImage() {
    if model.isMyPosition && index == 0 {
      typeImage.image = #imageLiteral(resourceName: "ic_route_manager_my_position")
      typeImage.tintColor = UIColor.linkBlue()
    } else {
      switch model.type {
      case .start:
        typeImage.image = #imageLiteral(resourceName: "ic_route_manager_start")
        typeImage.tintColor = UIColor.linkBlue()
      case .intermediate:
        let i = model.intermediateIndex + 1
        // TODO: Properly support more than 20 icons.
        var iconName = "route-point-20"
        if (i >= 1 && i < 20) {
          iconName = "route-point-" + String(i)
        }
        typeImage.image = #imageLiteral(resourceName: iconName)
        typeImage.tintColor = UIColor.primary()
      case .finish:
        typeImage.image = #imageLiteral(resourceName: "ic_route_manager_finish")
        typeImage.tintColor = UIColor.blackPrimaryText()
      }
    }
  }

  private func setupLabels() {
    let subtitle: String?
    if model.isMyPosition && index != 0 {
      titleLabel.text = model.latLonString
      subtitle = model.title
    } else {
      titleLabel.text = model.title
      subtitle = model.subtitle
    }
    var subtitleConstraintsActive = false
    if let subtitle = subtitle, !subtitle.isEmpty {
      subtitleLabel.text = subtitle
      subtitleConstraintsActive = true
    }
    subtitleLabel.isHidden = !subtitleConstraintsActive
    subtitleConstraints.forEach { $0.isActive = subtitleConstraintsActive }
  }

  private func setupSeparators() {
    let isSeparatorsHidden = model.type == .finish
    separator1.isHidden = isSeparatorsHidden
    separator2.isHidden = isSeparatorsHidden
  }

  override func applyTheme() {
    super.applyTheme()
    self.setupTypeImage()
  }
}
