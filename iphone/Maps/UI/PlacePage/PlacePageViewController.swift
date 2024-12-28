protocol PlacePageViewProtocol: AnyObject {
  var interactor: PlacePageInteractorProtocol! { get set }

  func setLayout(_ layout: IPlacePageLayout)
  func closeAnimated(completion: (() -> Void)?)
  func updatePreviewOffset()
  func showNextStop()
  func layoutIfNeeded()
  func updateWithLayout(_ layout: IPlacePageLayout)
  func showAlert(_ alert: UIAlertController)
}

final class PlacePageScrollView: UIScrollView {
  override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
    return point.y > 0
  }
}

@objc final class PlacePageViewController: UIViewController {
  
  private enum Constants {
    static let actionBarHeight: CGFloat = 50
    static let additionalPreviewOffset: CGFloat = 80
  }
  
  @IBOutlet var scrollView: UIScrollView!
  @IBOutlet var stackView: UIStackView!
  @IBOutlet var actionBarContainerView: UIView!
  @IBOutlet var actionBarHeightConstraint: NSLayoutConstraint!
  @IBOutlet var panGesture: UIPanGestureRecognizer!

  var headerStackView: UIStackView = {
    let stackView = UIStackView()
    stackView.axis = .vertical
    stackView.distribution = .fill
    return stackView
  }()
  var interactor: PlacePageInteractorProtocol!
  var beginDragging = false
  var rootViewController: MapViewController {
    MapViewController.shared()!
  }

  private var previousTraitCollection: UITraitCollection?
  private var layout: IPlacePageLayout!
  private var scrollSteps: [PlacePageState] = []
  var isPreviewPlus: Bool = false
  private var isNavigationBarVisible = false

  // MARK: - VC Lifecycle

  override func viewDidLoad() {
    super.viewDidLoad()

    setupView()
    setupLayout(layout)
  }

  override func viewDidLayoutSubviews() {
    super.viewDidLayoutSubviews()
    if #available(iOS 13.0, *) {
      // See https://github.com/organicmaps/organicmaps/issues/6917 for the details.
    } else if previousTraitCollection == nil {
      scrollView.contentInset = alternativeSizeClass(iPhone: UIEdgeInsets(top: scrollView.height, left: 0, bottom: 0, right: 0),
                                                     iPad: UIEdgeInsets.zero)
      updateSteps()
    }
    panGesture.isEnabled = alternativeSizeClass(iPhone: false, iPad: true)
    previousTraitCollection = traitCollection
  }

  override func viewWillAppear(_ animated: Bool) {
    super.viewWillAppear(animated)
    interactor?.viewWillAppear()
  }

  override func viewDidAppear(_ animated: Bool) {
    super.viewDidAppear(animated)
    updatePreviewOffset()
  }

  override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    // Update layout when the device was rotated but skip when the appearance was changed.
    if self.previousTraitCollection != nil, previousTraitCollection?.userInterfaceStyle == traitCollection.userInterfaceStyle, previousTraitCollection?.verticalSizeClass != traitCollection.verticalSizeClass {
      DispatchQueue.main.async {
        self.updateSteps()
        self.showLastStop()
        self.scrollView.contentInset = self.alternativeSizeClass(iPhone: UIEdgeInsets(top: self.scrollView.height, left: 0, bottom: 0, right: 0),
                                                                 iPad: UIEdgeInsets.zero)
      }
    }
  }


  // MARK: - Actions

  @IBAction func onPan(gesture: UIPanGestureRecognizer) {
    let xOffset = gesture.translation(in: view.superview).x
    gesture.setTranslation(CGPoint.zero, in: view.superview)
    view.minX += xOffset
    view.minX = min(view.minX, 0)
    let alpha = view.maxX / view.width
    view.alpha = alpha

    let state = gesture.state
    if state == .ended || state == .cancelled {
      if alpha < 0.8 {
        closeAnimated()
      } else {
        UIView.animate(withDuration: kDefaultAnimationDuration) {
          self.view.minX = 0
          self.view.alpha = 1
        }
      }
    }
  }

  // MARK: - Private methods

  private func updateSteps() {
    layoutIfNeeded()
    scrollSteps = layout.calculateSteps(inScrollView: scrollView,
                                        compact: traitCollection.verticalSizeClass == .compact)
  }

  private func findNextStop(_ offset: CGFloat, velocity: CGFloat) -> PlacePageState {
    if velocity == 0 {
      return findNearestStop(offset)
    }

    var result: PlacePageState
    if velocity < 0 {
      guard let first = scrollSteps.first else { return .closed(-scrollView.height) }
      result = first
      scrollSteps.suffix(from: 1).forEach {
        if offset > $0.offset {
          result = $0
        }
      }
    } else {
      guard let last = scrollSteps.last else { return .closed(-scrollView.height) }
      result = last
      scrollSteps.reversed().suffix(from: 1).forEach {
        if offset < $0.offset {
          result = $0
        }
      }
    }

    return result
  }

  private func setupView() {
    let bgView = UIView()
    bgView.styleName = "PPBackgroundView"
    stackView.insertSubview(bgView, at: 0)
    bgView.alignToSuperview()

    scrollView.decelerationRate = .fast
    scrollView.backgroundColor = .clear

    stackView.backgroundColor = .clear

    let cornersToMask: CACornerMask = alternativeSizeClass(iPhone: [], iPad: [.layerMinXMaxYCorner, .layerMaxXMaxYCorner])
    actionBarContainerView.layer.setCorner(radius: 16, corners: cornersToMask)
    actionBarContainerView.layer.masksToBounds = true

    // See https://github.com/organicmaps/organicmaps/issues/6917 for the details.
    if #available(iOS 13.0, *), previousTraitCollection == nil {
      scrollView.contentInset = alternativeSizeClass(iPhone: UIEdgeInsets(top: view.height, left: 0, bottom: 0, right: 0),
                                                     iPad: UIEdgeInsets.zero)
      scrollView.layoutIfNeeded()
    }
  }

  private func setupLayout(_ layout: IPlacePageLayout) {
    setLayout(layout)

    fillHeader(with: layout.headerViewControllers)
    fillBody(with: layout.bodyViewControllers)

    beginDragging = false
    if let actionBar = layout.actionBar {
      hideActionBar(false)
      addActionBar(actionBar)
    } else {
      hideActionBar(true)
    }
  }

  private func fillHeader(with viewControllers: [UIViewController]) {
    viewControllers.forEach { [self] viewController in
      if !stackView.arrangedSubviews.contains(headerStackView) {
        stackView.addArrangedSubview(headerStackView)
      }
      headerStackView.addArrangedSubview(viewController.view)
    }
    headerStackView.addSeparator(.bottom)
  }

  private func fillBody(with viewControllers: [UIViewController]) {
    viewControllers.forEach { [self] viewController in
      addChild(viewController)
      stackView.addArrangedSubview(viewController.view)
      viewController.didMove(toParent: self)
      viewController.view.addSeparator(.top)
      viewController.view.addSeparator(.bottom)
    }
  }

  private func cleanupLayout() {
    layout?.actionBar?.view.removeFromSuperview()
    layout?.navigationBar?.view.removeFromSuperview()
    headerStackView.arrangedSubviews.forEach { $0.removeFromSuperview() }
    stackView.arrangedSubviews.forEach { $0.removeFromSuperview() }
  }

  private func findNearestStop(_ offset: CGFloat) -> PlacePageState {
    var result = scrollSteps[0]
    scrollSteps.suffix(from: 1).forEach { ppState in
      if abs(result.offset - offset) > abs(ppState.offset - offset) {
        result = ppState
      }
    }
    return result
  }

  private func addActionBar(_ actionBarViewController: UIViewController) {
    addChild(actionBarViewController)
    actionBarViewController.view.translatesAutoresizingMaskIntoConstraints = false
    actionBarContainerView.addSubview(actionBarViewController.view)
    actionBarViewController.didMove(toParent: self)
    NSLayoutConstraint.activate([
      actionBarViewController.view.leadingAnchor.constraint(equalTo: actionBarContainerView.leadingAnchor),
      actionBarViewController.view.topAnchor.constraint(equalTo: actionBarContainerView.topAnchor),
      actionBarViewController.view.trailingAnchor.constraint(equalTo: actionBarContainerView.trailingAnchor),
      actionBarViewController.view.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor)
    ])
  }

  private func addNavigationBar(_ header: UIViewController) {
    header.view.translatesAutoresizingMaskIntoConstraints = false
    view.addSubview(header.view)
    addChild(header)
    NSLayoutConstraint.activate([
      header.view.leadingAnchor.constraint(equalTo: view.leadingAnchor),
      header.view.topAnchor.constraint(equalTo: view.topAnchor),
      header.view.trailingAnchor.constraint(equalTo: view.trailingAnchor)
    ])
  }

  private func scrollTo(_ point: CGPoint, animated: Bool = true, forced: Bool = false, completion: (() -> Void)? = nil) {
    if alternativeSizeClass(iPhone: beginDragging, iPad: true) && !forced {
      return
    }
    if forced {
      beginDragging = true
    }
    let scrollPosition = CGPoint(x: point.x, y: min(scrollView.contentSize.height - scrollView.height, point.y))
    let bound = view.height + scrollPosition.y
    if animated {
      updateTopBound(bound, duration: kDefaultAnimationDuration)
      UIView.animate(withDuration: kDefaultAnimationDuration, animations: { [weak scrollView] in
        scrollView?.contentOffset = scrollPosition
        self.layoutIfNeeded()
      }) { complete in
        if complete {
          completion?()
        }
      }
    } else {
      scrollView?.contentOffset = scrollPosition
      completion?()
    }
  }

  private func showLastStop() {
    if let lastStop = scrollSteps.last {
      scrollTo(CGPoint(x: 0, y: lastStop.offset), forced: true)
    }
  }

  private func updateTopBound(_ bound: CGFloat, duration: TimeInterval) {
    alternativeSizeClass(iPhone: {
      interactor.updateTopBound(bound, duration: duration)
    }, iPad: {})
  }
}

// MARK: - PlacePageViewProtocol

extension PlacePageViewController: PlacePageViewProtocol {
  func layoutIfNeeded() {
    guard layout != nil else { return }
    view.layoutIfNeeded()
  }

  func updateWithLayout(_ layout: IPlacePageLayout) {
    setupLayout(layout)
  }
  
  func setLayout(_ layout: IPlacePageLayout) {
    if self.layout != nil {
      cleanupLayout()
    }
    self.layout = layout
  }

  func hideActionBar(_ value: Bool) {
    actionBarHeightConstraint.constant = !value ? Constants.actionBarHeight : .zero
  }

  func updatePreviewOffset() {
    updateSteps()
    if !beginDragging {
      let stateOffset = isPreviewPlus ? scrollSteps[2].offset : scrollSteps[1].offset + Constants.additionalPreviewOffset
      scrollTo(CGPoint(x: 0, y: stateOffset))
    }
  }

  func showNextStop() {
    if let nextStop = scrollSteps.last(where: { $0.offset > scrollView.contentOffset.y }) {
      scrollTo(CGPoint(x: 0, y: nextStop.offset), forced: true)
    }
  }

  @objc
  func closeAnimated(completion: (() -> Void)? = nil) {
    alternativeSizeClass(iPhone: {
      self.scrollTo(CGPoint(x: 0, y: -self.scrollView.height + 1),
                    forced: true) {
                self.rootViewController.dismissPlacePage()
                completion?()
      }
    }, iPad: {
      UIView.animate(withDuration: kDefaultAnimationDuration,
                     animations: {
                      let frame = self.view.frame
                      self.view.minX = frame.minX - frame.width
                      self.view.alpha = 0
      }) { complete in
        self.rootViewController.dismissPlacePage()
        completion?()
      }
    })
  }

  func showAlert(_ alert: UIAlertController) {
    present(alert, animated: true)
  }
}

// MARK: - UIScrollViewDelegate

extension PlacePageViewController: UIScrollViewDelegate {
  func scrollViewDidScroll(_ scrollView: UIScrollView) {
    if scrollView.contentOffset.y < -scrollView.height + 1 && beginDragging {
      rootViewController.dismissPlacePage()
    }
    onOffsetChanged(scrollView.contentOffset.y)

    let bound = view.height + scrollView.contentOffset.y
    updateTopBound(bound, duration: 0)
  }

  func scrollViewWillBeginDragging(_ scrollView: UIScrollView) {
    beginDragging = true
  }

  func scrollViewWillEndDragging(_ scrollView: UIScrollView,
                                 withVelocity velocity: CGPoint,
                                 targetContentOffset: UnsafeMutablePointer<CGPoint>) {
    let maxOffset = scrollSteps.last?.offset ?? 0
    if targetContentOffset.pointee.y > maxOffset {
      return
    }

    let targetState = findNextStop(scrollView.contentOffset.y, velocity: velocity.y)
    if targetState.offset > scrollView.contentSize.height - scrollView.contentInset.top {
      return
    }

    updateSteps()
    let nextStep = findNextStop(scrollView.contentOffset.y, velocity: velocity.y)
    targetContentOffset.pointee = CGPoint(x: 0, y: nextStep.offset)
  }

  func onOffsetChanged(_ offset: CGFloat) {
    if offset > 0 && !isNavigationBarVisible {
      setNavigationBarVisible(true)
    } else if offset <= 0 && isNavigationBarVisible {
      setNavigationBarVisible(false)
    }
  }

  private func setNavigationBarVisible(_ visible: Bool) {
    guard visible != isNavigationBarVisible, let navigationBar = layout?.navigationBar else { return }
    isNavigationBarVisible = visible
    if isNavigationBarVisible {
      addNavigationBar(navigationBar)
    } else {
      navigationBar.removeFromParent()
      navigationBar.view.removeFromSuperview()
    }
  }
}
