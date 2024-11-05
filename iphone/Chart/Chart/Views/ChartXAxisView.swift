import UIKit

fileprivate class ChartXAxisInnerView: UIView {
  var lowerBound = 0
  var upperBound = 0
  var steps: [String] = []
  var labels: [UILabel] = []

  var font: UIFont = UIFont.systemFont(ofSize: 12, weight: .regular) {
    didSet {
      labels.forEach { $0.font = font }
    }
  }

  var textColor: UIColor = UIColor(white: 0, alpha: 0.3) {
    didSet {
      labels.forEach { $0.textColor = textColor }
    }
  }

  override var frame: CGRect {
    didSet {
      if upperBound > 0 {
        updateLabels()
      }
    }
  }

  private func makeLabel(text: String) -> UILabel {
    let label = UILabel()
    label.font = font
    label.textColor = textColor
    label.text = text
    label.frame = CGRect(x: 0, y: 0, width: 60, height: 15)
    return label
  }

  func setBounds(lower: Int, upper: Int, steps: [String]) {
    lowerBound = lower
    upperBound = upper
    self.steps = steps

    labels.forEach { $0.removeFromSuperview() }
    labels.removeAll()

    for i in 0..<steps.count {
      let step = steps[i]
      let label = makeLabel(text: step)
      if i == 0 {
        label.textAlignment = .left
      } else if i == steps.count - 1 {
        label.textAlignment = .right
      } else {
        label.textAlignment = .center
      }
      labels.append(label)
      addSubview(label)
    }

    updateLabels()
  }

  private func updateLabels() {
    let step = CGFloat(upperBound - lowerBound) / CGFloat(labels.count - 1)
    for i in 0..<labels.count {
      let x = bounds.width * step * CGFloat(i) / CGFloat(upperBound - lowerBound)
      let l = labels[i]
      var f = l.frame
      let adjust = bounds.width > 0 ? x / bounds.width : 0
      f.origin = CGPoint(x: x - f.width * adjust, y: 0)
      l.frame = f
    }
  }
}

class ChartXAxisView: UIView {

  struct Value {
    let index: Int
    let value: Double
    let text: String
  }

  var lowerBound = 0
  var upperBound = 0
  var values: [Value] = []

  var font: UIFont = UIFont.systemFont(ofSize: 12, weight: .regular) {
    didSet {
      labelsView?.font = font
    }
  }

  var textColor: UIColor = UIColor(white: 0, alpha: 0.3) {
    didSet {
      labelsView?.textColor = textColor
    }
  }

  private var labelsView: ChartXAxisInnerView?

  func setBounds(lower: Int, upper: Int) {
    lowerBound = lower
    upperBound = upper

    let begin = values[lower].value
    let end = values[upper].value
    let step = CGFloat(end - begin) / 5
    var labels: [String] = []
    for i in 0..<5 {
      if let x = values.first(where: { $0.value >= (begin + step * CGFloat(i)) }) {
        labels.append(x.text)
      }
    }
    labels.append(values[upper].text)

    let lv = ChartXAxisInnerView()
    lv.frame = bounds
    lv.textColor = textColor
    lv.autoresizingMask = [.flexibleWidth, .flexibleHeight]
    addSubview(lv)

    if let labelsView = labelsView {
      labelsView.removeFromSuperview()
    }

    lv.setBounds(lower: lower, upper: upper, steps: labels)
    labelsView = lv
  }
}
