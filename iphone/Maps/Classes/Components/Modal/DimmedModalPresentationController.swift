class DimmedModalPresentationController: UIPresentationController {
  private lazy var onTapGr: UITapGestureRecognizer = {
    return UITapGestureRecognizer(target: self, action: #selector(onTap))
  }()

  private lazy var dimView: UIView = {
    let view = UIView()
    view.setStyle(.blackStatusBarBackground)
    if isCancellable {
      view.addGestureRecognizer(onTapGr)
    }
    return view
  }()

  let isCancellable: Bool

  required init(presentedViewController: UIViewController, presenting presentingViewController: UIViewController?, cancellable: Bool = true) {
    isCancellable = cancellable
    super.init(presentedViewController: presentedViewController, presenting: presentingViewController)
  }

  @objc private func onTap() {
    presentingViewController.dismiss(animated: true, completion: nil)
  }

  override func presentationTransitionWillBegin() {
    guard let containerView = containerView else { return }
    containerView.addSubview(dimView)
    dimView.frame = containerView.bounds
    dimView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    dimView.alpha = 0
    presentingViewController.transitionCoordinator?.animate(alongsideTransition: { _ in
      self.dimView.alpha = 1
    })
  }

  override func presentationTransitionDidEnd(_ completed: Bool) {
    if !completed { dimView.removeFromSuperview() }
  }

  override func dismissalTransitionWillBegin() {
    presentingViewController.transitionCoordinator?.animate(alongsideTransition: { _ in
      self.dimView.alpha = 0
    })
  }

  override func dismissalTransitionDidEnd(_ completed: Bool) {
    if completed { dimView.removeFromSuperview() }
  }
}

final class SearchCoverVerticalModalTransitioning: NSObject, UIViewControllerTransitioningDelegate {
  func animationController(forPresented presented: UIViewController,
                           presenting: UIViewController,
                           source: UIViewController) -> UIViewControllerAnimatedTransitioning? {
    return CoverVerticalPresentationAnimator()
  }

  func animationController(forDismissed dismissed: UIViewController) -> UIViewControllerAnimatedTransitioning? {
    return CoverVerticalDismissalAnimator()
  }

  func presentationController(forPresented presented: UIViewController,
                              presenting: UIViewController?,
                              source: UIViewController) -> UIPresentationController? {
    return SearchModalPresentationController(presentedViewController: presented, presenting: presenting)
  }
}
