@objc(MWMTransportRoutePreviewStatus)
final class TransportRoutePreviewStatus: SolidTouchView {
  @IBOutlet private weak var etaLabel: UILabel!
  @IBOutlet private weak var stepsCollectionView: TransportTransitStepsCollectionView!
  @IBOutlet private weak var stepsCollectionViewHeight: NSLayoutConstraint!

  @objc weak var ownerView: UIView!

  weak var navigationInfo: MWMNavigationDashboardEntity?

  private var isVisible = false {
    didSet {
      guard isVisible != oldValue else { return }
      if isVisible {
        addView()
      } else {
        self.removeFromSuperview()
      }
    }
  }

  private func addView() {
    guard superview != ownerView else { return }
    ownerView.addSubview(self)

    let lg = ownerView.safeAreaLayoutGuide
    leadingAnchor.constraint(equalTo: lg.leadingAnchor).isActive = true
    trailingAnchor.constraint(equalTo: lg.trailingAnchor).isActive = true
    bottomAnchor.constraint(equalTo: lg.bottomAnchor).isActive = true
  }

  @objc func hide() {
    isVisible = false
  }

  @objc func showReady() {
    isVisible = true
    updateHeight()
  }

  @objc func onNavigationInfoUpdated(_ info: MWMNavigationDashboardEntity) {
    navigationInfo = info
    etaLabel.attributedText = info.estimate
    stepsCollectionView.steps = info.transitSteps
  }

  private func updateHeight() {
    guard stepsCollectionViewHeight.constant != stepsCollectionView.contentSize.height else { return }
    DispatchQueue.main.async {
      self.animateConstraints(animations: {
        self.stepsCollectionViewHeight.constant = self.stepsCollectionView.contentSize.height
      })
    }
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    updateHeight()
  }

  override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    updateHeight()
  }

  override var sideButtonsAreaAffectDirections: MWMAvailableAreaAffectDirections {
    return .bottom
  }

  override var visibleAreaAffectDirections: MWMAvailableAreaAffectDirections {
    return .bottom
  }

  override var widgetsAreaAffectDirections: MWMAvailableAreaAffectDirections {
    return .bottom
  }
}
