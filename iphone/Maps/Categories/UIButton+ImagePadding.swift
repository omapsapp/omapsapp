extension UIButton {
  @objc func setImagePadding(_ padding: CGFloat) {
    let isRightToLeft = UIView.userInterfaceLayoutDirection(for: self.semanticContentAttribute) == .rightToLeft
    if isRightToLeft {
      self.contentEdgeInsets = UIEdgeInsets(top: 0, left: 0, bottom: 0, right: padding)
      self.imageEdgeInsets = UIEdgeInsets(top: 0, left: 0, bottom: 0, right: -2 * padding)
    } else {
      self.contentEdgeInsets = UIEdgeInsets(top: 0, left: padding, bottom: 0, right: 0)
      self.imageEdgeInsets = UIEdgeInsets(top: 0, left: -2 * padding, bottom: 0, right: 0)
    }
  }
}
