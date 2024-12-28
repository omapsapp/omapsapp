import UIKit

class BottomTabBarButton: MWMButton {
  @objc override func applyTheme() {
    if styleName.isEmpty {
      styleName = "BottomTabBarButton"
    }

    for style in StyleManager.shared.getStyle(styleName) where !style.isEmpty && !style.hasExclusion(view: self) {
      BottomTabBarButtonRenderer.render(self, style: style)
    }
  }

  override func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
    return bounds.insetBy(dx: kExtendedTabBarTappableMargin, dy: kExtendedTabBarTappableMargin).contains(point)
  }
}

class BottomTabBarButtonRenderer {
  class func render(_ control: BottomTabBarButton, style: Style) {
    UIViewRenderer.renderShadow(control, style: style)
    UIViewRenderer.renderBorder(control, style: style)
    
    if let coloring = style.coloring {
      control.coloring = coloring
    }
    if let imageName = style.mwmImage {
      control.imageName = imageName
    }
    if let backgroundColor = style.backgroundColor {
      control.backgroundColor = backgroundColor
    }
  }
}

