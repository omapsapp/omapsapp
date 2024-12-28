final class TrackRecordingViewController: MWMViewController {

  private enum Constants {
    static let buttonDiameter = CGFloat(48)
    static let topOffset = CGFloat(6)
    static let trailingOffset = CGFloat(10)
    static let blinkingDuration = 1.0
    static let color: (lighter: UIColor, darker: UIColor) = (.red, .red.darker(percent: 0.3))
  }

  private let trackRecordingManager: TrackRecordingManager = .shared
  private let button = BottomTabBarButton()
  private var blinkingTimer: Timer?
  private var topConstraint = NSLayoutConstraint()
  private var trailingConstraint = NSLayoutConstraint()

  private static var availableArea: CGRect = .zero
  private static var topConstraintValue: CGFloat {
    availableArea.origin.y + Constants.topOffset
  }
  private static var trailingConstraintValue: CGFloat {
    -(UIScreen.main.bounds.maxX - availableArea.maxX + Constants.trailingOffset)
  }

  @objc
  init() {
    super.init(nibName: nil, bundle: nil)
    let ownerViewController = MapViewController.shared()
    ownerViewController?.addChild(self)
    ownerViewController?.controlsView.addSubview(view)
    self.setupView()
    self.layout()
    self.startTimer()
  }

  @available(*, unavailable)
  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  override func viewDidAppear(_ animated: Bool) {
    super.viewDidAppear(animated)
    UIView.transition(with: self.view,
                      duration: kDefaultAnimationDuration,
                      options: .transitionCrossDissolve,
                      animations: {
      self.button.isHidden = false
    })
  }

  // MARK: - Public methods

  @objc
  func close(completion: @escaping (() -> Void)) {
    stopTimer()
    UIView.transition(with: self.view,
                      duration: kDefaultAnimationDuration,
                      options: .transitionCrossDissolve,
                      animations: {
      self.button.isHidden = true
    }, completion: { _ in
      self.removeFromParent()
      self.view.removeFromSuperview()
      completion()
    })
  }

  // MARK: - Private methods

  private func setupView() {
    view.translatesAutoresizingMaskIntoConstraints = false
    view.addSubview(button)

    button.setStyleAndApply("TrackRecordingWidgetButton")
    button.tintColor = Constants.color.darker
    button.translatesAutoresizingMaskIntoConstraints = false
    button.setImage(UIImage(resource: .icMenuBookmarkTrackRecording), for: .normal)
    button.addTarget(self, action: #selector(onTrackRecordingButtonPressed), for: .touchUpInside)
    button.isHidden = true
  }

  private func layout() {
    guard let superview = view.superview else { return }
    topConstraint = view.topAnchor.constraint(equalTo: superview.topAnchor, constant: Self.topConstraintValue)
    trailingConstraint = view.trailingAnchor.constraint(equalTo: superview.trailingAnchor, constant: Self.trailingConstraintValue)
    NSLayoutConstraint.activate([
      topConstraint,
      trailingConstraint,
      view.widthAnchor.constraint(equalToConstant: Constants.buttonDiameter),
      view.heightAnchor.constraint(equalToConstant: Constants.buttonDiameter),

      button.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      button.trailingAnchor.constraint(equalTo: view.trailingAnchor),
      button.topAnchor.constraint(equalTo: view.topAnchor),
      button.bottomAnchor.constraint(equalTo: view.bottomAnchor),
    ])
  }

  private func updateLayout() {
    guard let superview = self.view.superview else { return }
    superview.animateConstraints {
      self.topConstraint.constant = Self.topConstraintValue
      self.trailingConstraint.constant = Self.trailingConstraintValue
    }
  }

  private func startTimer() {
    guard blinkingTimer == nil else { return }
    var lighter = false
    let timer = Timer.scheduledTimer(withTimeInterval: Constants.blinkingDuration, repeats: true) { [weak self] _ in
      guard let self = self else { return }
      UIView.animate(withDuration: Constants.blinkingDuration, animations: {
        self.button.tintColor = lighter ? Constants.color.lighter : Constants.color.darker
        lighter.toggle()
      })
    }
    blinkingTimer = timer
    RunLoop.current.add(timer, forMode: .common)
  }

  private func stopTimer() {
    blinkingTimer?.invalidate()
    blinkingTimer = nil
  }

  static func updateAvailableArea(_ frame: CGRect) {
    availableArea = frame
    guard let controller = MapViewController.shared()?.controlsManager.trackRecordingButton else { return }
    DispatchQueue.main.async {
      controller.updateLayout()
    }
  }

  // MARK: - Actions

  @objc
  private func onTrackRecordingButtonPressed(_ sender: Any) {
    switch trackRecordingManager.recordingState {
    case .inactive:
      trackRecordingManager.processAction(.start)
    case .active:
      trackRecordingManager.processAction(.stop)
    }
  }
}
