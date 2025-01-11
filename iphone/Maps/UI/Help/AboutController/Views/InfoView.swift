final class InfoView: UIView {

  private let stackView = UIStackView()
  private let imageView = UIImageView()
  private let titleLabel = UILabel()
  private lazy var imageViewWidthConstrain = imageView.widthAnchor.constraint(equalToConstant: 0)

  init() {
    super.init(frame: .zero)
    self.setupView()
    self.arrangeViews()
    self.layoutViews()
  }

  convenience init(image: UIImage?, title: String) {
    self.init()
    self.set(image: image, title: title)
  }

  override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    if #available(iOS 13.0, *), traitCollection.hasDifferentColorAppearance(comparedTo: previousTraitCollection) {
      imageView.applyTheme()
    }
  }

  @available(*, unavailable)
  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  private func setupView() {
    stackView.axis = .horizontal
    stackView.distribution = .fill
    stackView.alignment = .center
    stackView.spacing = 16

    titleLabel.setStyleNameAndApply("regular16:blackPrimaryText")
    titleLabel.lineBreakMode = .byWordWrapping
    titleLabel.numberOfLines = .zero

    imageView.setStyleNameAndApply("MWMBlack")
    imageView.contentMode = .scaleAspectFit
  }

  private func arrangeViews() {
    addSubview(stackView)
    stackView.addArrangedSubview(imageView)
    stackView.addArrangedSubview(titleLabel)
  }

  private func layoutViews() {
    stackView.translatesAutoresizingMaskIntoConstraints = false
    imageView.translatesAutoresizingMaskIntoConstraints = false
    titleLabel.translatesAutoresizingMaskIntoConstraints = false
    imageView.setContentHuggingPriority(.defaultHigh, for: .vertical)
    imageView.setContentHuggingPriority(.defaultHigh, for: .horizontal)
    titleLabel.setContentHuggingPriority(.defaultLow, for: .horizontal)
    NSLayoutConstraint.activate([
      stackView.leadingAnchor.constraint(equalTo: leadingAnchor),
      stackView.trailingAnchor.constraint(equalTo: trailingAnchor),
      stackView.topAnchor.constraint(equalTo: topAnchor),
      stackView.bottomAnchor.constraint(equalTo: bottomAnchor),
      imageView.heightAnchor.constraint(equalToConstant: 24),
      imageViewWidthConstrain
    ])
    updateImageWidth()
  }

  private func updateImageWidth() {
    imageViewWidthConstrain.constant = imageView.image == nil ? 0 : 24
    imageView.isHidden = imageView.image == nil
  }

  // MARK: - Public
  func set(image: UIImage?, title: String) {
    imageView.image = image
    titleLabel.text = title
    updateImageWidth()
  }
}
