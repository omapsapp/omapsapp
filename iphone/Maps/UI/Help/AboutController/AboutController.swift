import OSLog

final class AboutController: MWMViewController {

  fileprivate struct AboutInfoTableViewCellModel {
    let title: String
    let image: UIImage?
    let didTapHandler: (() -> Void)?
  }

  fileprivate struct SocialMediaCollectionViewCellModel {
    let image: UIImage
    let didTapHandler: (() -> Void)?
  }

  private enum Constants {
    static let infoTableViewCellHeight: CGFloat = 40
    static let socialMediaCollectionViewCellMaxWidth: CGFloat = 50
    static let socialMediaCollectionViewSpacing: CGFloat = 25
    static let socialMediaCollectionNumberOfItemsInRowCompact: CGFloat = 5
    static let socialMediaCollectionNumberOfItemsInRowRegular: CGFloat = 10
  }

  private let scrollView = UIScrollView()
  private let stackView = UIStackView()
  private let logoImageView = UIImageView()
  private let headerTitleLabel = UILabel()
  private let additionalInfoStackView = UIStackView()
  private let donationView = DonationView()
  private let osmView = OSMView()
  private let infoTableView = UITableView(frame: .zero, style: .plain)
  private var infoTableViewHeightAnchor: NSLayoutConstraint?
  private let socialMediaHeaderLabel = UILabel()
  private let socialMediaCollectionView = UICollectionView(frame: .zero, collectionViewLayout: UICollectionViewFlowLayout())
  private lazy var socialMediaCollectionViewHeighConstraint = socialMediaCollectionView.heightAnchor.constraint(equalToConstant: .zero)
  private let termsOfUseAndPrivacyPolicyView = ButtonsStackView()
  private var infoTableViewData = [AboutInfoTableViewCellModel]()
  private var socialMediaCollectionViewData = [SocialMediaCollectionViewCellModel]()
  private var onDidAppearCompletionHandler: (() -> Void)?

  init(onDidAppearCompletionHandler: (() -> Void)? = nil) {
    self.onDidAppearCompletionHandler = onDidAppearCompletionHandler
    super.init(nibName: nil, bundle: nil)
  }

  @available(*, unavailable)
  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  // MARK: - Lifecycle
  override func viewDidLoad() {
    super.viewDidLoad()
    setupViews()
    arrangeViews()
    layoutViews()
  }

  override func viewWillAppear(_ animated: Bool) {
    super.viewWillAppear(animated)
    updateCollection()
  }

  override func viewDidAppear(_ animated: Bool) {
    super.viewDidAppear(animated)
    if let completionHandler = onDidAppearCompletionHandler {
      completionHandler()
      onDidAppearCompletionHandler = nil
    }
  }

  override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    updateCollection()
  }
}

// MARK: - Private
private extension AboutController {
  func setupViews() {
    func setupTitle() {
      let titleView = UILabel()
      titleView.text = Self.formattedAppVersion()
      titleView.textColor = .white
      titleView.font = UIFont.systemFont(ofSize: 17, weight: .semibold)
      titleView.isUserInteractionEnabled = true
      titleView.numberOfLines = 1
      titleView.allowsDefaultTighteningForTruncation = true
      titleView.adjustsFontSizeToFitWidth = true
      titleView.minimumScaleFactor = 0.5
      let titleDidTapGestureRecognizer = UITapGestureRecognizer(target: self, action: #selector(appVersionButtonTapped))
      titleView.addGestureRecognizer(titleDidTapGestureRecognizer)
      navigationItem.titleView = titleView
    }

    func setupScrollAndStack() {
      scrollView.delaysContentTouches = false
      scrollView.contentInset = UIEdgeInsets(top: 20, left: 0, bottom: 20, right: 0)

      stackView.axis = .vertical
      stackView.distribution = .fill
      stackView.alignment = .center
      stackView.spacing = 15
    }

    func setupLogo() {
      logoImageView.contentMode = .scaleAspectFit
      logoImageView.image = UIImage(named: "logo")
    }

    func setupHeaderTitle() {
      headerTitleLabel.setStyleNameAndApply("semibold18:blackPrimaryText")
      headerTitleLabel.text = L("about_headline")
      headerTitleLabel.textAlignment = .center
      headerTitleLabel.numberOfLines = 1
      headerTitleLabel.allowsDefaultTighteningForTruncation = true
      headerTitleLabel.adjustsFontSizeToFitWidth = true
      headerTitleLabel.minimumScaleFactor = 0.5
    }

    func setupAdditionalInfo() {
      additionalInfoStackView.axis = .vertical
      additionalInfoStackView.spacing = 15

      [AboutInfo.noTracking, .noWifi, .community].forEach({ additionalInfoStackView.addArrangedSubview(InfoView(image: nil, title: $0.title)) })
    }

    func setupDonation() {
      donationView.donateButtonDidTapHandler = { [weak self] in
        guard let self else { return }
        self.openUrl(self.isDonateEnabled() ? Settings.donateUrl() : L("translated_om_site_url") + "support-us/")
      }
    }

    func setupOSM() {
      osmView.setMapDate(Self.formattedMapsDataVersion())
      osmView.didTapHandler = { [weak self] in
        self?.openUrl("https://www.openstreetmap.org/")
      }
    }

    func setupInfoTable() {
      infoTableView.setStyleNameAndApply("ClearBackground")
      infoTableView.delegate = self
      infoTableView.dataSource = self
      infoTableView.separatorStyle = .none
      infoTableView.isScrollEnabled = false
      infoTableView.showsVerticalScrollIndicator = false
      infoTableView.contentInset = .zero
      infoTableView.register(cell: InfoTableViewCell.self)
    }

    func setupSocialMediaCollection() {
      socialMediaHeaderLabel.setStyleNameAndApply("regular16:blackPrimaryText")
      socialMediaHeaderLabel.text = L("follow_us")
      socialMediaHeaderLabel.numberOfLines = 1
      socialMediaHeaderLabel.allowsDefaultTighteningForTruncation = true
      socialMediaHeaderLabel.adjustsFontSizeToFitWidth = true
      socialMediaHeaderLabel.minimumScaleFactor = 0.5

      socialMediaCollectionView.backgroundColor = .clear
      socialMediaCollectionView.isScrollEnabled = false
      socialMediaCollectionView.dataSource = self
      socialMediaCollectionView.delegate = self
      socialMediaCollectionView.register(cell: SocialMediaCollectionViewCell.self)
    }

    func setupTermsAndPrivacy() {
      termsOfUseAndPrivacyPolicyView.addButton(title: L("privacy_policy"), didTapHandler: { [weak self] in
        self?.openUrl(L("translated_om_site_url") + "privacy/")
      })
      termsOfUseAndPrivacyPolicyView.addButton(title: L("terms_of_use"), didTapHandler: { [weak self] in
        self?.openUrl(L("translated_om_site_url") + "terms/")
      })
      termsOfUseAndPrivacyPolicyView.addButton(title: L("copyright"), didTapHandler: { [weak self] in
        self?.showCopyright()
      })
    }

    view.setStyleNameAndApply("PressBackground")
    
    setupTitle()
    setupScrollAndStack()
    setupLogo()
    setupHeaderTitle()
    setupAdditionalInfo()
    setupDonation()
    setupOSM()
    setupInfoTable()
    setupSocialMediaCollection()
    setupTermsAndPrivacy()

    infoTableViewData = buildInfoTableViewData()
    socialMediaCollectionViewData = buildSocialMediaCollectionViewData()
  }

  func arrangeViews() {
    view.addSubview(scrollView)
    scrollView.addSubview(stackView)
    stackView.addArrangedSubview(logoImageView)
    stackView.addArrangedSubview(headerTitleLabel)
    stackView.addArrangedSubviewWithSeparator(additionalInfoStackView)
    if isDonateEnabled() {
      stackView.addArrangedSubviewWithSeparator(donationView)
    }
    stackView.addArrangedSubviewWithSeparator(osmView)
    stackView.addArrangedSubviewWithSeparator(infoTableView)
    stackView.addArrangedSubviewWithSeparator(socialMediaHeaderLabel)
    stackView.addArrangedSubview(socialMediaCollectionView)
    stackView.addArrangedSubviewWithSeparator(termsOfUseAndPrivacyPolicyView)
  }

  func layoutViews() {
    scrollView.translatesAutoresizingMaskIntoConstraints = false
    stackView.translatesAutoresizingMaskIntoConstraints = false
    logoImageView.translatesAutoresizingMaskIntoConstraints = false
    additionalInfoStackView.translatesAutoresizingMaskIntoConstraints = false
    donationView.translatesAutoresizingMaskIntoConstraints = false
    infoTableView.translatesAutoresizingMaskIntoConstraints = false
    socialMediaCollectionView.translatesAutoresizingMaskIntoConstraints = false
    termsOfUseAndPrivacyPolicyView.translatesAutoresizingMaskIntoConstraints = false

    NSLayoutConstraint.activate([
      scrollView.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor),
      scrollView.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor),
      scrollView.topAnchor.constraint(equalTo: view.topAnchor),
      scrollView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
      scrollView.contentLayoutGuide.widthAnchor.constraint(equalTo: scrollView.frameLayoutGuide.widthAnchor),

      stackView.leadingAnchor.constraint(equalTo: scrollView.contentLayoutGuide.leadingAnchor, constant: 20),
      stackView.trailingAnchor.constraint(equalTo: scrollView.contentLayoutGuide.trailingAnchor, constant: -20),
      stackView.topAnchor.constraint(equalTo: scrollView.contentLayoutGuide.topAnchor),
      stackView.bottomAnchor.constraint(equalTo: scrollView.contentLayoutGuide.bottomAnchor),

      logoImageView.heightAnchor.constraint(equalToConstant: 64),
      logoImageView.widthAnchor.constraint(equalTo: logoImageView.heightAnchor),

      additionalInfoStackView.widthAnchor.constraint(equalTo: stackView.widthAnchor),

      osmView.widthAnchor.constraint(equalTo: stackView.widthAnchor),

      infoTableView.widthAnchor.constraint(equalTo: stackView.widthAnchor),
      infoTableView.heightAnchor.constraint(equalToConstant: Constants.infoTableViewCellHeight * CGFloat(infoTableViewData.count)),

      socialMediaHeaderLabel.leadingAnchor.constraint(equalTo: socialMediaCollectionView.leadingAnchor),

      socialMediaCollectionView.widthAnchor.constraint(equalTo: stackView.widthAnchor),
      socialMediaCollectionView.contentLayoutGuide.widthAnchor.constraint(equalTo: stackView.widthAnchor),
      socialMediaCollectionViewHeighConstraint,

      termsOfUseAndPrivacyPolicyView.widthAnchor.constraint(equalTo: stackView.widthAnchor),
    ])
    donationView.widthAnchor.constraint(equalTo: stackView.widthAnchor).isActive = isDonateEnabled()

    view.layoutIfNeeded()
    updateCollection()
  }

  func updateCollection() {
    socialMediaCollectionView.collectionViewLayout.invalidateLayout()
    // On devices with the iOS 12 the actual collectionView layout update not always occurs during the current layout update cycle.
    // So constraints update should be performed on the next layout update cycle.
    DispatchQueue.main.async {
      self.socialMediaCollectionViewHeighConstraint.constant = self.socialMediaCollectionView.collectionViewLayout.collectionViewContentSize.height
    }
  }

  func isDonateEnabled() -> Bool {
    return Settings.donateUrl() != nil
  }

  func buildInfoTableViewData() -> [AboutInfoTableViewCellModel] {
    let infoContent: [AboutInfo] = [.faq, .reportMapDataProblem, .reportABug, .news, .volunteer, .rateTheApp]
    let data = infoContent.map { [weak self] aboutInfo in
      return AboutInfoTableViewCellModel(title: aboutInfo.title, image: aboutInfo.image, didTapHandler: {
        switch aboutInfo {
        case .faq:
          self?.navigationController?.pushViewController(FaqController(), animated: true)
        case .reportABug:
          MailComposer.sendBugReportWith(title:"Organic Maps Bug Report")
        case .reportMapDataProblem, .volunteer, .news:
          self?.openUrl(aboutInfo.link)
        case .rateTheApp:
          UIApplication.shared.rateApp()
        default:
          break
        }
      })
    }
    return data
  }

  func buildSocialMediaCollectionViewData() -> [SocialMediaCollectionViewCellModel] {
    let socialMediaContent: [SocialMedia] = [.telegram, .github, .instagram, .twitter, .linkedin, .organicMapsEmail, .reddit, .matrix, .facebook, .fosstodon]
    let data = socialMediaContent.map { [weak self] socialMedia in
      return SocialMediaCollectionViewCellModel(image: socialMedia.image, didTapHandler: {
        switch socialMedia {
        case .telegram: fallthrough
        case .github: fallthrough
        case .reddit: fallthrough
        case .matrix: fallthrough
        case .fosstodon: fallthrough
        case .facebook: fallthrough
        case .twitter: fallthrough
        case .instagram: fallthrough
        case .linkedin:
          self?.openUrl(socialMedia.link, externally: true)
        case .organicMapsEmail:
          MailComposer.sendEmail(toRecipients: [socialMedia.link])
        }
      })
    }
    return data
  }

  // Returns a human-readable maps data version.
  static func formattedMapsDataVersion() -> String {
    // First, convert version code like 220131 to a date.
    let df = DateFormatter()
    df.locale = Locale(identifier:"en_US_POSIX")
    df.dateFormat = "yyMMdd"
    let mapsVersionInt = FrameworkHelper.dataVersion()
    let mapsDate = df.date(from: String(mapsVersionInt))!
    // Second, print the date in the local user's format.
    df.locale = Locale.current
    df.dateStyle = .long
    df.timeStyle = .none
    return df.string(from:mapsDate)
  }

  static func formattedAppVersion() -> String {
    let appInfo = AppInfo.shared();
    // Use strong left-to-right unicode direction characters for the app version.
    return String(format: L("version"), "\u{2066}\(appInfo.bundleVersion)-\(appInfo.buildNumber)\u{2069}")
  }

  func showCopyright() {
    let path = Bundle.main.path(forResource: "copyright", ofType: "html")!
    let html = try! String(contentsOfFile: path, encoding: String.Encoding.utf8)
    let webViewController = WebViewController.init(html: html, baseUrl: nil, title: L("copyright"))!
    webViewController.openInSafari = true
    self.navigationController?.pushViewController(webViewController, animated: true)
  }

  func copyToClipboard(_ content: String) {
    UIPasteboard.general.string = content
    let message = String(format: L("copied_to_clipboard"), content)
    UIImpactFeedbackGenerator(style: .medium).impactOccurred()
    Toast.toast(withText: message).show(withAlignment: .bottom, pinToSafeArea: false)
  }
}

// MARK: - Actions
private extension AboutController {
  @objc func appVersionButtonTapped() {
    copyToClipboard(Self.formattedAppVersion())
  }

  @objc func osmMapsDataButtonTapped() {
    copyToClipboard(Self.formattedMapsDataVersion())
  }
}

// MARK: - UITableViewDelegate
extension AboutController: UITableViewDelegate {
  func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
    tableView.deselectRow(at: indexPath, animated: true)
    infoTableViewData[indexPath.row].didTapHandler?()
  }

  func tableView(_ tableView: UITableView, heightForRowAt indexPath: IndexPath) -> CGFloat {
    return Constants.infoTableViewCellHeight
  }
}

// MARK: - UITableViewDataSource
extension AboutController: UITableViewDataSource {
  func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
    return infoTableViewData.count
  }

  func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
    let cell = tableView.dequeueReusableCell(cell: InfoTableViewCell.self, indexPath: indexPath)
    let aboutInfo = infoTableViewData[indexPath.row]
    cell.set(image: aboutInfo.image, title: aboutInfo.title)
    return cell
  }
}

// MARK: - UICollectionViewDataSource
extension AboutController: UICollectionViewDataSource {
  func collectionView(_ collectionView: UICollectionView, numberOfItemsInSection section: Int) -> Int {
    return socialMediaCollectionViewData.count
  }

  func collectionView(_ collectionView: UICollectionView, cellForItemAt indexPath: IndexPath) -> UICollectionViewCell {
    let cell = collectionView.dequeueReusableCell(cell: SocialMediaCollectionViewCell.self, indexPath: indexPath)
    cell.setImage(socialMediaCollectionViewData[indexPath.row].image)
    return cell
  }
}

// MARK: - UICollectionViewDelegate
extension AboutController: UICollectionViewDelegate {
  func collectionView(_ collectionView: UICollectionView, didSelectItemAt indexPath: IndexPath) {
    let model = socialMediaCollectionViewData[indexPath.row]
    model.didTapHandler?()
  }
}

// MARK: - UICollectionViewDelegateFlowLayout
extension AboutController: UICollectionViewDelegateFlowLayout {
  func collectionView(_ collectionView: UICollectionView, layout collectionViewLayout: UICollectionViewLayout, sizeForItemAt indexPath: IndexPath) -> CGSize {
    let spacing = Constants.socialMediaCollectionViewSpacing
    let numberOfItemsInRowCompact = Constants.socialMediaCollectionNumberOfItemsInRowCompact
    let numberOfItemsInRowRegular = Constants.socialMediaCollectionNumberOfItemsInRowRegular
    var totalSpacing = (Constants.socialMediaCollectionNumberOfItemsInRowCompact - 1) * spacing
    var width = (collectionView.bounds.width - totalSpacing) / numberOfItemsInRowCompact
    if traitCollection.verticalSizeClass == .compact || traitCollection.horizontalSizeClass == .regular {
      totalSpacing = (numberOfItemsInRowRegular - 1) * spacing
      width = (collectionView.bounds.width - totalSpacing) / numberOfItemsInRowRegular
    }
    let maxWidth = Constants.socialMediaCollectionViewCellMaxWidth
    width = min(width, maxWidth)
    return CGSize(width: width, height: width)
  }

  func collectionView(_ collectionView: UICollectionView, layout collectionViewLayout: UICollectionViewLayout, minimumLineSpacingForSectionAt section: Int) -> CGFloat {
    return Constants.socialMediaCollectionViewSpacing
  }

  func collectionView(_ collectionView: UICollectionView, layout collectionViewLayout: UICollectionViewLayout, minimumInteritemSpacingForSectionAt section: Int) -> CGFloat {
    return Constants.socialMediaCollectionViewSpacing
  }
}
// MARK: - UIStackView + AddArrangedSubviewWithSeparator
private extension UIStackView {
  func addArrangedSubviewWithSeparator(_ view: UIView) {
    if !arrangedSubviews.isEmpty {
      let separator = UIView()
      separator.setStyleNameAndApply("Divider")
      separator.isUserInteractionEnabled = false
      separator.translatesAutoresizingMaskIntoConstraints = false
      addArrangedSubview(separator)
      NSLayoutConstraint.activate([
        separator.heightAnchor.constraint(equalToConstant: 1.0),
        separator.leadingAnchor.constraint(equalTo: leadingAnchor),
        separator.trailingAnchor.constraint(equalTo: trailingAnchor),
      ])
    }
    addArrangedSubview(view)
  }
}
