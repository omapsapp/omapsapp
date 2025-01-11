final class RouteManagerDimView: UIView {
  @IBOutlet private weak var image: UIImageView!
  @IBOutlet private weak var label: UILabel!
  @IBOutlet private weak var messageView: UIView!
  @IBOutlet private weak var messageViewContainer: UIView!
  @IBOutlet private var messageViewVerticalCenter: NSLayoutConstraint!
  @IBOutlet private var labelVerticalCenter: NSLayoutConstraint!

  enum State {
    case visible
    case binOpenned
    case hidden
  }

  var state = State.hidden {
    didSet {
      guard state != oldValue else { return }
      switch state {
      case .visible:
        isVisible = true
        image.image = #imageLiteral(resourceName: "ic_route_manager_trash")
      case .binOpenned:
        isVisible = true
        image.image = #imageLiteral(resourceName: "ic_route_manager_trash_open")
      case .hidden:
        isVisible = false
      }
    }
  }

  var binDropPoint: CGPoint {
    return convert(image.isHidden ? label.center : image.center, from: messageView)
  }

  private var isVisible = false {
    didSet {
      guard isVisible != oldValue else { return }
      let componentsAlpha: CGFloat = 0.5
      setStyle(.global(.blackStatusBarBackground))
      alpha = isVisible ? 0 : 1
      image.alpha = isVisible ? 0 : componentsAlpha
      label.alpha = isVisible ? 0 : componentsAlpha
      UIView.animate(withDuration: kDefaultAnimationDuration,
                     animations: {
                       self.alpha = self.isVisible ? 1 : 0
                       self.image.alpha = self.isVisible ? componentsAlpha : 0
                       self.label.alpha = self.isVisible ? componentsAlpha : 0
                     },
                     completion: { _ in
                       self.alpha = 1
                       if !self.isVisible {
                         self.backgroundColor = UIColor.clear
                       }
      })
      setNeedsLayout()
    }
  }

  func setViews(container: UIView, controller: UIView, manager: UIView) {
    alpha = 0

    alternative(iPhone: {
      controller.insertSubview(self, at: 0)

      NSLayoutConstraint(item: self, attribute: .left, relatedBy: .equal, toItem: self.messageViewContainer, attribute: .left, multiplier: 1, constant: 0).isActive = true
      NSLayoutConstraint(item: manager, attribute: .top, relatedBy: .equal, toItem: self.messageViewContainer, attribute: .bottom, multiplier: 1, constant: 0).isActive = true
    },
    iPad: {
      container.insertSubview(self, at: 0)

      NSLayoutConstraint(item: self, attribute: .bottom, relatedBy: .equal, toItem: self.messageViewContainer, attribute: .bottom, multiplier: 1, constant: 0).isActive = true
      NSLayoutConstraint(item: controller, attribute: .right, relatedBy: .equal, toItem: self.messageViewContainer, attribute: .left, multiplier: 1, constant: 0).isActive = true
    })()

    NSLayoutConstraint(item: container, attribute: .top, relatedBy: .equal, toItem: self, attribute: .top, multiplier: 1, constant: 0).isActive = true
    NSLayoutConstraint(item: container, attribute: .bottom, relatedBy: .equal, toItem: self, attribute: .bottom, multiplier: 1, constant: 0).isActive = true
    NSLayoutConstraint(item: container, attribute: .left, relatedBy: .equal, toItem: self, attribute: .left, multiplier: 1, constant: 0).isActive = true
    NSLayoutConstraint(item: container, attribute: .right, relatedBy: .equal, toItem: self, attribute: .right, multiplier: 1, constant: 0).isActive = true
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    let isImageHidden = messageView.height > messageViewContainer.height
    image.isHidden = isImageHidden
    messageViewVerticalCenter.isActive = !isImageHidden
    labelVerticalCenter.isActive = isImageHidden
  }
}
