import UIKit

enum ChartYAxisViewAlignment {
  case left
  case right
}

fileprivate class ChartYAxisInnerView: UIView {
  override class var layerClass: AnyClass { return CAShapeLayer.self }

  private static let font = UIFont.systemFont(ofSize: 12, weight: .regular)
  var lowerBound: CGFloat = 0
  var upperBound: CGFloat = 0
  var steps: [CGFloat] = []
  let lowerLabel: UILabel
  let upperLabel: UILabel
  let lowerLabelBackground = UIView()
  let upperLabelBackground = UIView()
  var alignment: ChartYAxisViewAlignment = .left

  var font: UIFont = UIFont.systemFont(ofSize: 12, weight: .regular) {
    didSet {
      lowerLabel.font = font
      upperLabel.font = font
    }
  }

  var textColor: UIColor = UIColor(white: 0, alpha: 0.3) {
    didSet {
      lowerLabel.textColor = textColor
      upperLabel.textColor = textColor
    }
  }

  var textBackgroundColor: UIColor = UIColor(white: 1, alpha: 0.7) {
    didSet {
      lowerLabelBackground.backgroundColor = textBackgroundColor
      upperLabelBackground.backgroundColor = textBackgroundColor
    }
  }

  var gridColor: UIColor = UIColor.white {
    didSet {
      shapeLayer.strokeColor = gridColor.cgColor
    }
  }

  private var path: UIBezierPath?

  var shapeLayer: CAShapeLayer {
    return layer as! CAShapeLayer
  }

  override init(frame: CGRect) {
    lowerLabel = ChartYAxisInnerView.makeLabel()
    upperLabel = ChartYAxisInnerView.makeLabel()

    super.init(frame: frame)

    lowerLabel.translatesAutoresizingMaskIntoConstraints = false
    upperLabel.translatesAutoresizingMaskIntoConstraints = false
    lowerLabelBackground.translatesAutoresizingMaskIntoConstraints = false
    upperLabelBackground.translatesAutoresizingMaskIntoConstraints = false

    lowerLabelBackground.addSubview(lowerLabel)
    upperLabelBackground.addSubview(upperLabel)
    addSubview(lowerLabelBackground)
    addSubview(upperLabelBackground)

    NSLayoutConstraint.activate([
      lowerLabel.leftAnchor.constraint(equalTo: lowerLabelBackground.leftAnchor, constant: 5),
      lowerLabel.topAnchor.constraint(equalTo: lowerLabelBackground.topAnchor),
      lowerLabel.rightAnchor.constraint(equalTo: lowerLabelBackground.rightAnchor, constant: -5),
      lowerLabel.bottomAnchor.constraint(equalTo: lowerLabelBackground.bottomAnchor),

      upperLabel.leftAnchor.constraint(equalTo: upperLabelBackground.leftAnchor, constant: 5),
      upperLabel.topAnchor.constraint(equalTo: upperLabelBackground.topAnchor),
      upperLabel.rightAnchor.constraint(equalTo: upperLabelBackground.rightAnchor, constant: -5),
      upperLabel.bottomAnchor.constraint(equalTo: upperLabelBackground.bottomAnchor),

      lowerLabelBackground.topAnchor.constraint(equalTo: topAnchor, constant: 5),
      lowerLabelBackground.rightAnchor.constraint(equalTo: rightAnchor, constant: -5),
      upperLabelBackground.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -5),
      upperLabelBackground.rightAnchor.constraint(equalTo: rightAnchor, constant:  -5)
    ])

    lowerLabel.textColor = textColor
    upperLabel.textColor = textColor
    lowerLabelBackground.backgroundColor = textBackgroundColor
    upperLabelBackground.backgroundColor = textBackgroundColor
    shapeLayer.fillColor = UIColor.clear.cgColor
    shapeLayer.strokeColor = gridColor.cgColor
    shapeLayer.lineDashPattern = [2, 3]
    shapeLayer.lineWidth = 1
  }

  required init?(coder: NSCoder) {
    fatalError("init(coder:) has not been implemented")
  }

  override func layoutSubviews() {
    super.layoutSubviews()

    if upperBound > 0 && lowerBound > 0 {
      updateGrid()
    }

    lowerLabelBackground.layer.cornerRadius = lowerLabelBackground.frame.height / 2
    upperLabelBackground.layer.cornerRadius = upperLabelBackground.frame.height / 2
  }

  static func makeLabel() -> UILabel {
    let label = UILabel()
    label.font = ChartYAxisInnerView.font
    label.transform = CGAffineTransform.identity.scaledBy(x: 1, y: -1)
    return label
  }

  func setBounds(lower: CGFloat, upper: CGFloat, lowerLabelText: String, upperLabelText: String, steps: [CGFloat]) {
    lowerBound = lower
    upperBound = upper
    lowerLabel.text = lowerLabelText
    upperLabel.text = upperLabelText
    self.steps = steps

    updateGrid()
  }

  func updateBounds(lower: CGFloat, upper: CGFloat, animationStyle: ChartAnimation = .none) {
    lowerBound = lower
    upperBound = upper
    updateGrid(animationStyle: animationStyle)
  }

  func updateGrid(animationStyle: ChartAnimation = .none) {
    let p = UIBezierPath()
    for step in steps {
      p.move(to: CGPoint(x: 0, y: step))
      p.addLine(to: CGPoint(x: bounds.width, y: step))
    }

    let realPath = p

    let yScale = (bounds.height) / CGFloat(upperBound - lowerBound)
    let yTranslate = (bounds.height) * CGFloat(-lowerBound) / CGFloat(upperBound - lowerBound)
    let scale = CGAffineTransform.identity.scaledBy(x: 1, y: yScale)
    let translate = CGAffineTransform.identity.translatedBy(x: 0, y: yTranslate)
    let transform = scale.concatenating(translate)
    realPath.apply(transform)

    if animationStyle != .none {
      let timingFunction = CAMediaTimingFunction(name: animationStyle == .interactive ? .linear : .easeInEaseOut)
      if shapeLayer.animationKeys()?.contains("path") ?? false,
        let presentation = shapeLayer.presentation(),
        let path = presentation.path {
        shapeLayer.removeAnimation(forKey: "path")
        shapeLayer.path = path
      }

      let animation = CABasicAnimation(keyPath: "path")
      let duration = animationStyle.rawValue
      animation.duration = duration
      animation.fromValue = shapeLayer.path
      animation.timingFunction = timingFunction
      layer.add(animation, forKey: "path")
    }

    shapeLayer.path = realPath.cgPath
  }
}

class ChartYAxisView: UIView {
  var lowerBound: CGFloat = 0
  var upperBound: CGFloat = 0
  var alignment: ChartYAxisViewAlignment = .right

  var font: UIFont = UIFont.systemFont(ofSize: 12, weight: .regular) {
    didSet {
      gridView?.font = font
    }
  }

  var textColor: UIColor = UIColor(white: 0, alpha: 0.3) {
    didSet {
      gridView?.textColor = textColor
    }
  }

  var textBackgroundColor: UIColor = UIColor(white: 0, alpha: 0.3) {
    didSet {
      gridView?.textBackgroundColor = textBackgroundColor
    }
  }

  var gridColor: UIColor = UIColor(white: 0, alpha: 0.3) {
    didSet {
      gridView?.gridColor = gridColor
    }
  }

  override var frame: CGRect {
    didSet {
      gridView?.updateGrid()
    }
  }

  private var gridView: ChartYAxisInnerView?

  func setBounds(lower: CGFloat,
                 upper: CGFloat,
                 lowerLabel: String,
                 upperLabel: String,
                 steps: [CGFloat],
                 animationStyle: ChartAnimation = .none) {
    let gv = ChartYAxisInnerView()
    gv.alignment = alignment
    gv.textColor = textColor
    gv.gridColor = gridColor
    gv.textBackgroundColor = textBackgroundColor
    gv.frame = bounds
    gv.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    addSubview(gv)

    if let gridView = gridView {
      if animationStyle == .animated {
        gv.setBounds(lower: lowerBound,
                     upper: upperBound,
                     lowerLabelText: lowerLabel,
                     upperLabelText: upperLabel,
                     steps: steps)
        gv.alpha = 0
        gv.updateBounds(lower: lower, upper:upper, animationStyle: animationStyle)
        gridView.updateBounds(lower: lower, upper:upper, animationStyle: animationStyle)
        UIView.animate(withDuration: animationStyle.rawValue, animations: {
          gv.alpha = 1
          gridView.alpha = 0
        }) { _ in
          gridView.removeFromSuperview()
        }
      } else {
        gv.setBounds(lower: lower, upper: upper, lowerLabelText: lowerLabel, upperLabelText: upperLabel, steps: steps)
        gridView.removeFromSuperview()
      }
    } else {
      gv.setBounds(lower: lower, upper: upper, lowerLabelText: lowerLabel, upperLabelText: upperLabel, steps: steps)
    }

    gridView = gv
    lowerBound = lower
    upperBound = upper
  }
}
