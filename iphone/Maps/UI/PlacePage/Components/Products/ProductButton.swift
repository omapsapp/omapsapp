final class ProductButton: UIButton {

  private var action: () -> Void

  init(title: String, action: @escaping () -> Void) {
    self.action = action
    super.init(frame: .zero)
    self.setup(title: title, action: action)
    self.layout()
  }

  @available(*, unavailable)
  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  private func setup(title: String, action: @escaping () -> Void) {
    setStyleAndApply("BlueBackground")
    setTitle(title, for: .normal)
    setTitleColor(.white, for: .normal)
    titleLabel?.font = UIFont.regular14()
    titleLabel?.allowsDefaultTighteningForTruncation = true
    titleLabel?.adjustsFontSizeToFitWidth = true
    titleLabel?.minimumScaleFactor = 0.5
    layer.setCorner(radius: 5.0)
    layer.masksToBounds = true
    addTarget(self, action: #selector(buttonDidTap), for: .touchUpInside)
  }

  private func layout() {
    translatesAutoresizingMaskIntoConstraints = false
    heightAnchor.constraint(equalToConstant: 30.0).isActive = true
  }

  @objc private func buttonDidTap() {
    action()
  }
}
