final class ElevationProfileDescriptionCell: UICollectionViewCell {

  private enum Constants {
    static let insets = UIEdgeInsets(top: 2, left: 0, bottom: -2, right: 0)
    static let valueSpacing: CGFloat = 8.0
    static let imageSize: CGSize = CGSize(width: 20, height: 20)
  }

  private let valueLabel = UILabel()
  private let subtitleLabel = UILabel()
  private let imageView = UIImageView()

  override init(frame: CGRect) {
    super.init(frame: frame)
    setupViews()
    layoutViews()
  }

  @available(*, unavailable)
  required init?(coder: NSCoder) {
    super.init(coder: coder)
    setupViews()
    layoutViews()
  }

  private func setupViews() {
    valueLabel.font = .medium14()
    valueLabel.styleName = "blackSecondaryText"
    valueLabel.numberOfLines = 1
    valueLabel.minimumScaleFactor = 0.1
    valueLabel.adjustsFontSizeToFitWidth = true
    valueLabel.allowsDefaultTighteningForTruncation = true

    subtitleLabel.font = .regular10()
    subtitleLabel.styleName = "blackSecondaryText"
    subtitleLabel.numberOfLines = 1
    subtitleLabel.minimumScaleFactor = 0.1
    subtitleLabel.adjustsFontSizeToFitWidth = true
    subtitleLabel.allowsDefaultTighteningForTruncation = true

    imageView.contentMode = .scaleAspectFit
    imageView.styleName = "MWMBlack"
  }

  private func layoutViews() {
    contentView.addSubview(imageView)
    contentView.addSubview(valueLabel)
    contentView.addSubview(subtitleLabel)
    imageView.translatesAutoresizingMaskIntoConstraints = false
    valueLabel.translatesAutoresizingMaskIntoConstraints = false
    subtitleLabel.translatesAutoresizingMaskIntoConstraints = false

    NSLayoutConstraint.activate([
      imageView.topAnchor.constraint(equalTo: contentView.topAnchor, constant: Constants.insets.top),
      imageView.leadingAnchor.constraint(equalTo: contentView.leadingAnchor),
      imageView.widthAnchor.constraint(equalToConstant: Constants.imageSize.width),
      imageView.heightAnchor.constraint(equalToConstant: Constants.imageSize.height),

      valueLabel.leadingAnchor.constraint(equalTo: imageView.trailingAnchor, constant: Constants.valueSpacing),
      valueLabel.trailingAnchor.constraint(equalTo: contentView.trailingAnchor),
      valueLabel.centerYAnchor.constraint(equalTo: imageView.centerYAnchor),

      subtitleLabel.topAnchor.constraint(equalTo: imageView.bottomAnchor),
      subtitleLabel.leadingAnchor.constraint(equalTo: imageView.leadingAnchor),
      subtitleLabel.bottomAnchor.constraint(lessThanOrEqualTo: contentView.bottomAnchor, constant: Constants.insets.bottom)
    ])
    subtitleLabel.setContentHuggingPriority(.defaultHigh, for: .vertical)
  }

  func configure(subtitle: String, value: String, imageName: String) {
    subtitleLabel.text = subtitle
    valueLabel.text = value
    imageView.image = UIImage(named: imageName)
  }

  override func prepareForReuse() {
    super.prepareForReuse()
    valueLabel.text = ""
    subtitleLabel.text = ""
    imageView.image = nil
  }
}
