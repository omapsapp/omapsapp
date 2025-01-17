final class SearchModalPresentationController: UIPresentationController {

  private enum StepChangeAnimation {
    case slide
    case slideAndBounce
  }

  private var stepHeights: [CGFloat]
  private var currentStepIndex: Int
  private var initialTranslationY: CGFloat = 0
  private var isPortraitOrientation: Bool {
    presentedViewController.traitCollection.verticalSizeClass == .regular
  }

  private lazy var panGestureRecognizer: UIPanGestureRecognizer = {
    return UIPanGestureRecognizer(target: self, action: #selector(handlePan(_:)))
  }()

  override init(presentedViewController: UIViewController, presenting presentingViewController: UIViewController?) {
    self.stepHeights = Self.stepsHeights(for: presentedViewController)
    self.currentStepIndex = stepHeights.count - 1
    super.init(presentedViewController: presentedViewController, presenting: presentingViewController)
    self.presentedViewController.view.addGestureRecognizer(panGestureRecognizer)
    self.panGestureRecognizer.delegate = self
  }

  override var frameOfPresentedViewInContainerView: CGRect {
    guard let containerView else { return .zero }
    let safeAreaInsets = containerView.safeAreaInsets
    var frame = super.frameOfPresentedViewInContainerView

    if isPortraitOrientation {
      frame.origin.y = containerView.frame.height - stepHeights.last!
    } else {
      let topSafeAreaInset: CGFloat = 10
      frame.origin.x = safeAreaInsets.left
      frame.size.width = kWidthForiPad
      frame.origin.y = topSafeAreaInset
//      frame.size.height = containerView.bounds.height - topSafeAreaInset
    }

    return frame
  }

  override func containerViewWillLayoutSubviews() {
    presentedView?.frame = frameOfPresentedViewInContainerView
  }

  override func presentationTransitionWillBegin() {
    guard let containerView, let presentedView else { return }
    containerView.backgroundColor = .clear
    iPhoneSpecific {
      presentedView.layer.setCorner(radius: 10)
      presentedView.layer.maskedCorners = [.layerMinXMinYCorner, .layerMaxXMinYCorner]
      presentedView.layer.masksToBounds = true
    }
  }
  

  override func presentationTransitionDidEnd(_ completed: Bool) {
  }

  override func dismissalTransitionWillBegin() {
  }

  override func dismissalTransitionDidEnd(_ completed: Bool) {
  }

  override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    stepHeights = Self.stepsHeights(for: presentedViewController)
    currentStepIndex = stepHeights.count - 1
  }

  @objc private func handlePan(_ gesture: UIPanGestureRecognizer) {
    guard let presentedView else { return }

    let translation = gesture.translation(in: presentedView)
    let velocity = gesture.velocity(in: presentedView)

    switch gesture.state {
    case .began:
      initialTranslationY = presentedView.frame.origin.y
    case .changed:
      let newY = max(initialTranslationY + translation.y, 0)
      presentedView.frame.origin.y = newY
    case .ended:
      let nextStepIndex: Int
      if velocity.y > 500 || translation.y > 50 {
        // drag down
        nextStepIndex = currentStepIndex - 1
      } else if velocity.y < -500 || translation.y < -50 {
        // drag up
        nextStepIndex = min(currentStepIndex + 1, stepHeights.count - 1)
      } else {
        nextStepIndex = currentStepIndex
      }

      let animation: StepChangeAnimation = abs(velocity.y) > 500 ? .slideAndBounce : .slide
      currentStepIndex = nextStepIndex
      animateToStep(stepIndex: currentStepIndex, animation: animation)
    default:
      break
    }
  }

  // TODO: configure step heights according to the device and orientation
  private static func stepsHeights(for viewController: UIViewController) -> [CGFloat] {
    let screenHeight = UIScreen.main.bounds.height
    return alternative(iPhone: {
      // TODO: calc bottom and top heights based on the content
      let isLandscape = viewController.traitCollection.verticalSizeClass == .compact
      return isLandscape ? [100, screenHeight - 10] : [100, screenHeight * 0.45, screenHeight * 0.9]
    }, iPad: {
      return [screenHeight]
    })()
  }

  private func animateToStep(stepIndex: Int, animation: StepChangeAnimation) {
    guard let presentedView, let containerView else { return }
    guard stepIndex >= 0 else {
      presentedViewController.dismiss(animated: true)
      return
    }
    let targetY = containerView.frame.height - stepHeights[stepIndex]

    switch animation {
    case .slide:
      UIView.animate(withDuration: kDefaultAnimationDuration,
                     delay: 0,
                     options: .curveEaseOut,
                     animations: {
        presentedView.frame.origin.y = targetY
      })
    case .slideAndBounce:
      UIView.animate(withDuration: kDefaultAnimationDuration,
                     delay: 0,
                     usingSpringWithDamping: 0.85,
                     initialSpringVelocity: 0.2,
                     options: .curveLinear,
                     animations: {
        presentedView.frame.origin.y = targetY
      })
    }
  }
}

extension SearchModalPresentationController: UIGestureRecognizerDelegate {
  func gestureRecognizerShouldBegin(_ gestureRecognizer: UIGestureRecognizer) -> Bool {
    true
  }
}
