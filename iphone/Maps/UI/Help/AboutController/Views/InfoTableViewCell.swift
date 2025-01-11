final class InfoTableViewCell: UITableViewCell {

  private let infoView = InfoView()

  override init(style: UITableViewCell.CellStyle, reuseIdentifier: String?) {
    super.init(style: .default, reuseIdentifier: reuseIdentifier)
    setupView()
  }

  @available(*, unavailable)
  required init?(coder: NSCoder) {
    super.init(coder: coder)
    setupView()
  }

  private func setupView() {
    backgroundView = UIView() // Set background color to clear
    setStyleNameAndApply("ClearBackground")
    contentView.addSubview(infoView)
    infoView.translatesAutoresizingMaskIntoConstraints = false
    NSLayoutConstraint.activate([
      infoView.leadingAnchor.constraint(equalTo: contentView.leadingAnchor),
      infoView.trailingAnchor.constraint(equalTo: contentView.trailingAnchor),
      infoView.topAnchor.constraint(equalTo: contentView.topAnchor),
      infoView.bottomAnchor.constraint(equalTo: contentView.bottomAnchor),
    ])
  }

  // MARK: - Public
  func set(image: UIImage?, title: String) {
    infoView.set(image: image, title: title)
  }
}
