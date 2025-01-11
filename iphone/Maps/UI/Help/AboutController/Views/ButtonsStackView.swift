final class ButtonsStackView: UIView {

  private let stackView = UIStackView()
  private var didTapHandlers = [UIButton: (() -> Void)?]()

  override init(frame: CGRect) {
    super.init(frame: frame)
    setupViews()
    arrangeViews()
    layoutViews()
  }

  required init?(coder: NSCoder) {
    super.init(coder: coder)
    setupViews()
    arrangeViews()
    layoutViews()
  }

  private func setupViews() {
    stackView.distribution = .fillEqually
    stackView.axis = .vertical
    stackView.spacing = 20
  }

  private func arrangeViews() {
    addSubview(stackView)
  }

  private func layoutViews() {
    stackView.translatesAutoresizingMaskIntoConstraints = false
    let offset = CGFloat(20)
    NSLayoutConstraint.activate([
      stackView.leadingAnchor.constraint(equalTo: leadingAnchor, constant: offset),
      stackView.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -offset),
      stackView.topAnchor.constraint(equalTo: topAnchor),
      stackView.bottomAnchor.constraint(equalTo: bottomAnchor)
    ])
  }

  @objc private func buttonTapped(_ sender: UIButton) {
    guard let didTapHandler = didTapHandlers[sender] else { return }
    didTapHandler?()
  }

  // MARK: - Public
  func addButton(title: String, font: UIFont = .regular14(), didTapHandler: @escaping () -> Void) {
    let button = UIButton()
    button.setStyleNameAndApply("FlatPrimaryTransButton")
    button.setTitle(title, for: .normal)
    button.titleLabel?.font = font
    button.addTarget(self, action: #selector(buttonTapped(_:)), for: .touchUpInside)
    stackView.addArrangedSubview(button)
    didTapHandlers[button] = didTapHandler
  }
}
