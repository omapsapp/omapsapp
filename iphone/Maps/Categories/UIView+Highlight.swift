extension UIView {
  @objc
  func highlight() {
    let color = UIColor.linkBlueHighlighted().withAlphaComponent(0.2)
    let duration: TimeInterval = kDefaultAnimationDuration
    let overlayView = UIView(frame: bounds)
    overlayView.backgroundColor = color
    overlayView.alpha = 0
    overlayView.clipsToBounds = true
    overlayView.isUserInteractionEnabled = false
    overlayView.layer.cornerRadius = layer.cornerRadius
    overlayView.layer.maskedCorners = layer.maskedCorners
    addSubview(overlayView)

    UIView.animate(withDuration: duration, delay: duration, options: .curveEaseInOut, animations: {
      overlayView.alpha = 1
    }) { _ in
      UIView.animate(withDuration: duration, delay: duration * 3, options: .curveEaseInOut, animations: {
        overlayView.alpha = 0
      }) { _ in
        overlayView.removeFromSuperview()
      }
    }
  }
}
