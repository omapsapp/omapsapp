extension UIView {
  enum SeparatorPosition {
    case top
    case bottom
  }

  func addSeparator(_ position: SeparatorPosition = .top,
                    thickness: CGFloat = 1.0,
                    insets: UIEdgeInsets = .zero) {
    let lineView = UIView()
    lineView.setStyleAndApply("Divider")
    lineView.isUserInteractionEnabled = false
    lineView.translatesAutoresizingMaskIntoConstraints = false
    addSubview(lineView)

    NSLayoutConstraint.activate([
      lineView.heightAnchor.constraint(equalToConstant: thickness),
      lineView.leadingAnchor.constraint(equalTo: leadingAnchor, constant: insets.left),
      lineView.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -insets.right),
    ])
    switch position {
    case .top:
      lineView.topAnchor.constraint(equalTo: topAnchor, constant: insets.top).isActive = true
    case .bottom:
      lineView.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -insets.bottom).isActive = true
    }
  }
}
