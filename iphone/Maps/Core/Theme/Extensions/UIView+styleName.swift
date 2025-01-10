fileprivate struct AssociatedKeys {
  static var styleName: UInt8 = 0
  static var isStyleApplied: UInt8 = 1
}

@objc extension UIView {
  @objc func sw_didMoveToWindow() {
    guard MapsAppDelegate.theApp().window === window else {
      sw_didMoveToWindow();
      return
    }
    applyTheme()
    isStyleApplied = true
    sw_didMoveToWindow();
  }

  @objc var styleName: String {
    get {
      isStyleApplied = false
      guard let value = objc_getAssociatedObject(self, &AssociatedKeys.styleName) as? String else {
        return ""
      }
      return value
    }
    set(newValue) {
      objc_setAssociatedObject(self, &AssociatedKeys.styleName, newValue, objc_AssociationPolicy.OBJC_ASSOCIATION_RETAIN_NONATOMIC)
    }
  }

  @objc var isStyleApplied: Bool {
    get {
      guard let value = objc_getAssociatedObject(self, &AssociatedKeys.isStyleApplied) as? Bool else {
        return false
      }
      return value
    }
    set(newValue) {
      objc_setAssociatedObject(self, &AssociatedKeys.isStyleApplied, newValue, objc_AssociationPolicy.OBJC_ASSOCIATION_RETAIN_NONATOMIC)
    }
  }

  @objc func setStyleNameAndApply(_ styleName: String) {
    self.styleName = styleName
    applyTheme()
  }
}

extension UIView {
  func setStyle(_ style: StyleSheet) {
    styleName = style.styleName
  }

  func setStyleAndApply(_ style: StyleSheet) {
    setStyleNameAndApply(style.styleName)
  }
}
