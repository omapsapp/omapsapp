@objc(MWMNavigationControlView)
final class NavigationControlView: SolidTouchView, MWMTextToSpeechObserver, MapOverlayManagerObserver {
  @IBOutlet private weak var distanceLabel: UILabel!
  @IBOutlet private weak var distanceLegendLabel: UILabel!
  @IBOutlet private weak var distanceWithLegendLabel: UILabel!
  @IBOutlet private weak var progressView: UIView!
  @IBOutlet private weak var routingProgress: NSLayoutConstraint!
  @IBOutlet private weak var speedLabel: UILabel!
  @IBOutlet private weak var speedBackground: UIView!
  @IBOutlet private weak var speedLegendLabel: UILabel!
  @IBOutlet private weak var speedWithLegendLabel: UILabel!
  @IBOutlet private weak var timeLabel: UILabel!
  @IBOutlet private weak var timePageControl: UIPageControl!

  @IBOutlet private weak var extendButton: UIButton! {
    didSet {
      setExtendButtonImage()
    }
  }

  @IBOutlet private weak var ttsButton: UIButton! {
    didSet {
      ttsButton.setImage(#imageLiteral(resourceName: "ic_voice_off"), for: .normal)
      ttsButton.setImage(#imageLiteral(resourceName: "ic_voice_on"), for: .selected)
      ttsButton.setImage(#imageLiteral(resourceName: "ic_voice_on"), for: [.selected, .highlighted])
      onTTSStatusUpdated()
    }
  }

  private lazy var dimBackground: DimBackground = {
    DimBackground(mainView: self, tapAction: { [weak self] in
      self?.diminish()
    })
  }()

  @objc weak var ownerView: UIView!
  @IBOutlet private weak var extendedView: UIView!

  private weak var navigationInfo: MWMNavigationDashboardEntity?

  private var extendedConstraint: NSLayoutConstraint!
  private var notExtendedConstraint: NSLayoutConstraint!
  @objc var isVisible = false {
    didSet {
      guard isVisible != oldValue else { return }
      if isVisible {
        addView()
      } else {
        removeView()
      }
    }
  }

  private var isExtended = false {
    willSet {
      guard isExtended != newValue else { return }
      morphExtendButton()
    }
    didSet {
      guard isVisible && superview != nil else { return }
      guard isExtended != oldValue else { return }

      dimBackground.setVisible(isExtended, completion: nil)
      extendedView.isHidden = !isExtended
      superview!.animateConstraints(animations: {
        if (self.isExtended) {
          self.notExtendedConstraint.isActive = false
          self.extendedConstraint.isActive = true
        } else {
          self.extendedConstraint.isActive = false
          self.notExtendedConstraint.isActive = true
        }
      })
    }
  }

  private func addView() {
    guard superview != ownerView else { return }
    ownerView.addSubview(self)

    let lg = ownerView.safeAreaLayoutGuide
    leadingAnchor.constraint(equalTo: lg.leadingAnchor).isActive = true
    trailingAnchor.constraint(equalTo: lg.trailingAnchor).isActive = true

    extendedConstraint = bottomAnchor.constraint(equalTo: lg.bottomAnchor)
    extendedConstraint.isActive = false

    notExtendedConstraint = progressView.bottomAnchor.constraint(equalTo: lg.bottomAnchor)
    notExtendedConstraint.isActive = true
  }

  private func removeView() {
    dimBackground.setVisible(false, completion: {
      self.removeFromSuperview()
    })
  }

  override func awakeFromNib() {
    super.awakeFromNib()

    updateLegendSize()

    MWMTextToSpeech.add(self)
    MapOverlayManager.add(self)
  }

  override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    updateLegendSize()
  }

  func updateLegendSize() {
    let isCompact = traitCollection.verticalSizeClass == .compact
    distanceLabel.isHidden = isCompact
    distanceLegendLabel.isHidden = isCompact
    distanceWithLegendLabel.isHidden = !isCompact
    speedLabel.isHidden = isCompact
    speedLegendLabel.isHidden = isCompact
    speedWithLegendLabel.isHidden = !isCompact

    let pgScale: CGFloat = isCompact ? 0.7 : 1
    timePageControl.transform = CGAffineTransform(scaleX: pgScale, y: pgScale)
  }

  @objc func onNavigationInfoUpdated(_ info: MWMNavigationDashboardEntity) {
    navigationInfo = info
    let routingNumberAttributes: [NSAttributedString.Key: Any] =
      [
        NSAttributedString.Key.foregroundColor: UIColor.blackPrimaryText(),
        NSAttributedString.Key.font: UIFont.bold24()
      ]
    let routingLegendAttributes: [NSAttributedString.Key: Any] =
      [
        NSAttributedString.Key.foregroundColor: UIColor.blackSecondaryText(),
        NSAttributedString.Key.font: UIFont.bold14()
      ]

    if timePageControl.currentPage == 0 {
      timeLabel.text = DurationFormatter.durationString(from: TimeInterval(info.timeToTarget))
    } else {
      timeLabel.text = info.arrival
    }

    var distanceWithLegend: NSMutableAttributedString?
    if let targetDistance = info.targetDistance {
      distanceLabel.text = targetDistance
      distanceWithLegend = NSMutableAttributedString(string: targetDistance, attributes: routingNumberAttributes)
    }

    if let targetUnits = info.targetUnits {
      distanceLegendLabel.text = targetUnits
      if let distanceWithLegend = distanceWithLegend {
        distanceWithLegend.append(NSAttributedString(string: targetUnits, attributes: routingLegendAttributes))
        distanceWithLegendLabel.attributedText = distanceWithLegend
      }
    }

    var speedMps = 0.0
    if let s = LocationManager.lastLocation()?.speed, s > 0 {
      speedMps = s
    }
    let speedMeasure = Measure(asSpeed: speedMps)
    var speed = speedMeasure.valueAsString;
    /// @todo Draw speed limit sign similar to the CarPlay implemenation.
    // speedLimitMps >= 0 means known limited speed.
    if (info.speedLimitMps >= 0) {
      // Short delimeter to not overlap with timeToTarget longer than an hour.
      let delimeter = info.timeToTarget < 60 * 60 ? " / " : "/"
      let speedLimitMeasure = Measure(asSpeed: info.speedLimitMps)
      // speedLimitMps == 0 means unlimited speed.
      speed += delimeter + (info.speedLimitMps == 0 ? "∞" : speedLimitMeasure.valueAsString)
    }

    speedLabel.text = speed
    speedLegendLabel.text = speedMeasure.unit
    let speedWithLegend = NSMutableAttributedString(string: speed, attributes: routingNumberAttributes)
    speedWithLegend.append(NSAttributedString(string: speedMeasure.unit, attributes: routingLegendAttributes))
    speedWithLegendLabel.attributedText = speedWithLegend

    if MWMRouter.isSpeedCamLimitExceeded() {
      speedLabel.textColor = UIColor.white()
      speedBackground.backgroundColor = UIColor.buttonRed()
    } else {
      let isSpeedLimitExceeded = info.speedLimitMps > 0 && speedMps > info.speedLimitMps
      speedLabel.textColor = isSpeedLimitExceeded ? UIColor.buttonRed() : UIColor.blackPrimaryText()
      speedBackground.backgroundColor = UIColor.clear
    }
    speedLegendLabel.textColor = speedLabel.textColor
    speedWithLegendLabel.textColor = speedLabel.textColor

    routingProgress.constant = progressView.width * info.progress / 100
  }

  @IBAction
  private func toggleInfoAction() {
    if let navigationInfo = navigationInfo {
      timePageControl.currentPage = (timePageControl.currentPage + 1) % timePageControl.numberOfPages
      onNavigationInfoUpdated(navigationInfo)
    }
    refreshDiminishTimer()
  }

  @IBAction
  private func extendAction() {
    isExtended = !isExtended
    refreshDiminishTimer()
  }

  private func morphExtendButton() {
    guard let imageView = extendButton.imageView else { return }
    let morphImagesCount = 6
    let startValue = isExtended ? morphImagesCount : 1
    let endValue = isExtended ? 0 : morphImagesCount + 1
    let stepValue = isExtended ? -1 : 1
    var morphImages: [UIImage] = []
    let nightMode = UIColor.isNightMode() ? "dark" : "light"
    for i in stride(from: startValue, to: endValue, by: stepValue) {
      let imageName = "ic_menu_\(i)_\(nightMode)"
      morphImages.append(UIImage(named: imageName)!)
    }
    imageView.animationImages = morphImages
    imageView.animationRepeatCount = 1
    imageView.image = morphImages.last
    imageView.startAnimating()
    setExtendButtonImage()
  }

  private func setExtendButtonImage() {
    DispatchQueue.main.async {
      guard let imageView = self.extendButton.imageView else { return }
      if imageView.isAnimating {
        self.setExtendButtonImage()
      } else {
        self.extendButton.setImage(self.isExtended ? #imageLiteral(resourceName: "ic_menu_down") : #imageLiteral(resourceName: "ic_menu"), for: .normal)
      }
    }
  }

  private func refreshDiminishTimer() {
    let sel = #selector(diminish)
    NSObject.cancelPreviousPerformRequests(withTarget: self, selector: sel, object: self)
    perform(sel, with: self, afterDelay: 5)
  }

  @objc
  private func diminish() {
    isExtended = false
  }

  func onTTSStatusUpdated() {
    guard MWMRouter.isRoutingActive() else { return }
    ttsButton.isHidden = !MWMTextToSpeech.isTTSEnabled()
    if !ttsButton.isHidden {
      ttsButton.isSelected = MWMTextToSpeech.tts().active
    }
    refreshDiminishTimer()
  }

  override func applyTheme() {
    super.applyTheme()
    onTTSStatusUpdated()
  }

  override var sideButtonsAreaAffectDirections: MWMAvailableAreaAffectDirections {
    return .bottom
  }

  override var widgetsAreaAffectDirections: MWMAvailableAreaAffectDirections {
    return alternative(iPhone: .bottom, iPad: [])
  }

  override var trackRecordingButtonAreaAffectDirections: MWMAvailableAreaAffectDirections {
    return .bottom
  }
}
