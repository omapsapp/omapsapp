import UIKit

final class ChartPointInfoView: UIView {
  enum Alignment {
    case left
    case right
  }

  private let distanceLabel = UILabel()
  private let altitudeLabel = UILabel()
  private let stackView = UIStackView()

  private let maskLayer = CAShapeLayer()
  private var maskPath: UIBezierPath?

  private let isInterfaceRightToLeft = UIApplication.shared.userInterfaceLayoutDirection == .rightToLeft

  var arrowY: CGFloat? {
    didSet {
      setNeedsLayout()
    }
  }

  var alignment = Alignment.left {
    didSet {
      updateMask()
    }
  }

  var font: UIFont = UIFont.systemFont(ofSize: 12, weight: .regular) {
    didSet {
      distanceLabel.font = font
      altitudeLabel.font = font
    }
  }

  var textColor: UIColor = .lightGray {
    didSet {
      distanceLabel.textColor = textColor
      altitudeLabel.textColor = textColor
    }
  }

  override var backgroundColor: UIColor? {
    didSet {
      maskLayer.fillColor = backgroundColor?.cgColor
    }
  }

  override init(frame: CGRect) {
    super.init(frame: frame)

    layer.cornerRadius = 5
    backgroundColor = .clear
    layer.shadowColor = UIColor(white: 0, alpha: 1).cgColor
    layer.shadowOpacity = 0.25
    layer.shadowRadius = 2
    layer.shadowOffset = CGSize(width: 0, height: 2)
    maskLayer.fillColor = backgroundColor?.cgColor
    layer.addSublayer(maskLayer)

    stackView.alignment = .leading
    stackView.axis = .vertical
    stackView.translatesAutoresizingMaskIntoConstraints = false
    addSubview(stackView)

    NSLayoutConstraint.activate([
      stackView.leftAnchor.constraint(equalTo: leftAnchor, constant: 6),
      stackView.topAnchor.constraint(equalTo: topAnchor, constant: 6),
      stackView.rightAnchor.constraint(equalTo: rightAnchor, constant: -6),
      stackView.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -6)
    ])

    stackView.addArrangedSubview(distanceLabel)
    stackView.addArrangedSubview(altitudeLabel)
    stackView.setCustomSpacing(6, after: distanceLabel)

    distanceLabel.font = font
    altitudeLabel.font = font

    distanceLabel.textColor = textColor
    altitudeLabel.textColor = textColor
  }

  required init?(coder aDecoder: NSCoder) {
    fatalError()
  }

  func set(x: CGFloat, label: String, points: [ChartLineInfo]) {
    distanceLabel.text = label
    altitudeLabel.text = altitudeText(points[0])
  }

  func update(x: CGFloat, label: String, points: [ChartLineInfo]) {
    distanceLabel.text = label
    altitudeLabel.text = altitudeText(points[0])
    layoutIfNeeded()
  }

  private func altitudeText(_ point: ChartLineInfo) -> String {
    return String(isInterfaceRightToLeft ? "\(point.formattedValue) ▲" : "▲ \(point.formattedValue)")
  }

  override func layoutSubviews() {
    super.layoutSubviews()

    let y = arrowY ?? bounds.midY
    let path = UIBezierPath(roundedRect: bounds, cornerRadius: 3)
    let trianglePath = UIBezierPath()
    trianglePath.move(to: CGPoint(x: bounds.maxX, y: y - 3))
    trianglePath.addLine(to: CGPoint(x: bounds.maxX + 5, y: y))
    trianglePath.addLine(to: CGPoint(x: bounds.maxX, y: y + 3))
    trianglePath.close()
    path.append(trianglePath)
    maskPath = path
    updateMask()
  }

  private func updateMask() {
    guard let path = maskPath?.copy() as? UIBezierPath else { return }
    if alignment == .right {
      path.apply(CGAffineTransform.identity.scaledBy(x: -1, y: 1).translatedBy(x: -bounds.width, y: 0))
    }
    maskLayer.path = path.cgPath
    layer.shadowPath = path.cgPath
  }
}
