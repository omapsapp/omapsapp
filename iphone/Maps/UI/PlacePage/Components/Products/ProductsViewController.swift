final class ProductsViewController: UIViewController {

  private enum Constants {
    static let spacing: CGFloat = 10
    static let titleLeadingPadding: CGFloat = 12
    static let titleTrailingPadding: CGFloat = 10
    static let descriptionTopPadding: CGFloat = 10
    static let closeButtonSize: CGFloat = 24
    static let closeButtonTrailingPadding: CGFloat = -12
    static let closeButtonTopPadding: CGFloat = 12
    static let stackViewTopPadding: CGFloat = 12
    static let subtitleButtonTopPadding: CGFloat = 4
    static let subtitleButtonBottomPadding: CGFloat = -4
  }

  private let viewModel: ProductsViewModel
  private let titleLabel = UILabel()
  private let descriptionLabel = UILabel()
  private let closeButton = UIButton(type: .system)
  private let stackView = UIStackView()
  private let leadingSubtitleButton = UIButton(type: .system)
  private let trailingSubtitleButton = UIButton(type: .system)

  init(viewModel: ProductsViewModel) {
    self.viewModel = viewModel
    super.init(nibName: nil, bundle: nil)
  }

  @available(*, unavailable)
  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }
  
  override func viewDidLoad() {
    super.viewDidLoad()
    setupViews()
    layout()
  }

  private func setupViews() {
    view.setStyleAndApply("Background")
    setupTitleLabel()
    setupDescriptionLabel()
    setupCloseButton()
    setupProductsStackView()
    setupSubtitleButtons()
  }

  private func setupTitleLabel() {
    titleLabel.text = viewModel.title
    titleLabel.font = UIFont.semibold16()
    titleLabel.numberOfLines = 1
    titleLabel.translatesAutoresizingMaskIntoConstraints = false
  }

  private func setupDescriptionLabel() {
    descriptionLabel.text = viewModel.description
    descriptionLabel.font = UIFont.regular14()
    descriptionLabel.numberOfLines = 0
    descriptionLabel.translatesAutoresizingMaskIntoConstraints = false
  }

  private func setupCloseButton() {
    closeButton.setStyleAndApply("MWMGray")
    closeButton.setImage(UIImage(resource: .icSearchClear), for: .normal)
    closeButton.translatesAutoresizingMaskIntoConstraints = false
    closeButton.addTarget(self, action: #selector(closeButtonDidTap), for: .touchUpInside)
  }

  private func setupProductsStackView() {
    stackView.axis = .horizontal
    stackView.alignment = .fill
    stackView.distribution = .fillEqually
    stackView.spacing = Constants.spacing
    stackView.translatesAutoresizingMaskIntoConstraints = false
    viewModel.products.forEach { product in
      let button = ProductButton(title: product.title) { [weak self] in
        self?.productButtonDidTap(product)
      }
      stackView.addArrangedSubview(button)
    }
  }

  private func setupSubtitleButtons() {
    leadingSubtitleButton.setTitle(viewModel.leadingSubtitle, for: .normal)
    leadingSubtitleButton.backgroundColor = .clear
    leadingSubtitleButton.setTitleColor(.linkBlue(), for: .normal)
    leadingSubtitleButton.translatesAutoresizingMaskIntoConstraints = false
    leadingSubtitleButton.addTarget(self, action: #selector(leadingSubtitleButtonDidTap), for: .touchUpInside)

    trailingSubtitleButton.setTitle(viewModel.trailingSubtitle, for: .normal)
    trailingSubtitleButton.backgroundColor = .clear
    trailingSubtitleButton.setTitleColor(.linkBlue(), for: .normal)
    trailingSubtitleButton.translatesAutoresizingMaskIntoConstraints = false
    trailingSubtitleButton.addTarget(self, action: #selector(trailingSubtitleButtonDidTap), for: .touchUpInside)
  }

  private func layout() {
    view.addSubview(titleLabel)
    view.addSubview(descriptionLabel)
    view.addSubview(closeButton)
    view.addSubview(stackView)
    view.addSubview(leadingSubtitleButton)
    view.addSubview(trailingSubtitleButton)
    
    NSLayoutConstraint.activate([
      titleLabel.topAnchor.constraint(equalTo: closeButton.topAnchor),
      titleLabel.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: Constants.titleLeadingPadding),
      titleLabel.trailingAnchor.constraint(equalTo: closeButton.leadingAnchor),

      descriptionLabel.topAnchor.constraint(equalTo: titleLabel.bottomAnchor, constant: Constants.descriptionTopPadding),
      descriptionLabel.leadingAnchor.constraint(equalTo: titleLabel.leadingAnchor),
      descriptionLabel.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -Constants.titleTrailingPadding),

      closeButton.widthAnchor.constraint(equalToConstant: Constants.closeButtonSize),
      closeButton.heightAnchor.constraint(equalToConstant: Constants.closeButtonSize),
      closeButton.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: Constants.closeButtonTrailingPadding),
      closeButton.topAnchor.constraint(equalTo: view.topAnchor, constant: Constants.closeButtonTopPadding),

      stackView.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: Constants.titleLeadingPadding),
      stackView.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -Constants.titleLeadingPadding),
      stackView.topAnchor.constraint(equalTo: descriptionLabel.bottomAnchor, constant: Constants.stackViewTopPadding),

      leadingSubtitleButton.topAnchor.constraint(equalTo: stackView.bottomAnchor, constant: Constants.subtitleButtonTopPadding),
      leadingSubtitleButton.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: Constants.titleLeadingPadding),
      leadingSubtitleButton.trailingAnchor.constraint(equalTo: view.centerXAnchor),
      leadingSubtitleButton.bottomAnchor.constraint(equalTo: view.bottomAnchor, constant: Constants.subtitleButtonBottomPadding),

      trailingSubtitleButton.topAnchor.constraint(equalTo: leadingSubtitleButton.topAnchor, constant: Constants.subtitleButtonTopPadding),
      trailingSubtitleButton.leadingAnchor.constraint(equalTo: view.centerXAnchor),
      trailingSubtitleButton.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -Constants.titleLeadingPadding),
      trailingSubtitleButton.bottomAnchor.constraint(equalTo: leadingSubtitleButton.bottomAnchor)
    ])
  }

  @objc private func closeButtonDidTap() {
    viewModel.didClose(reason: .close)
    hide()
  }

  private func productButtonDidTap(_ product: Product) {
    viewModel.didSelectProduct(product)
    viewModel.didClose(reason: .selectProduct)
    hide()
  }

  @objc private func leadingSubtitleButtonDidTap() {
    viewModel.didClose(reason: .alreadyDonated)
    hide()
  }

  @objc private func trailingSubtitleButtonDidTap() {
    viewModel.didClose(reason: .remindLater)
    hide()
  }

  func hide() {
    UIView.transition(with: view, duration: kDefaultAnimationDuration / 2, options: .transitionCrossDissolve) {
      self.view.isHidden = true
    }
  }
}
