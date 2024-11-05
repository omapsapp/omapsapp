import UIKit

enum ChartAnimation: TimeInterval {
  case none = 0.0
  case animated = 0.3
  case interactive = 0.1
}

public class ChartView: UIView {
  let chartsContainerView = ExpandedTouchView()
  let chartPreviewView = ChartPreviewView()
  let yAxisView = ChartYAxisView()
  let xAxisView = ChartXAxisView()
  let chartInfoView = ChartInfoView()
  var lineViews: [ChartLineView] = []
  var showPreview: Bool = false // Set true to show the preview

  private var tapGR: UITapGestureRecognizer!
  private var panStartPoint = 0
  private var panGR: UIPanGestureRecognizer!
  private var pinchStartLower = 0
  private var pinchStartUpper = 0
  private var pinchGR: UIPinchGestureRecognizer!

  public var myPosition: Double = -1 {
    didSet {
      setMyPosition(myPosition)
    }
  }

  public var previewSelectorColor: UIColor = UIColor.lightGray.withAlphaComponent(0.9) {
    didSet {
      chartPreviewView.selectorColor = previewSelectorColor
    }
  }

  public var previewTintColor: UIColor = UIColor.lightGray.withAlphaComponent(0.5) {
    didSet {
      chartPreviewView.selectorTintColor = previewTintColor
    }
  }

  public var infoBackgroundColor: UIColor = UIColor.white {
    didSet {
      chartInfoView.infoBackgroundColor = infoBackgroundColor
      yAxisView.textBackgroundColor = infoBackgroundColor.withAlphaComponent(0.7)
    }
  }

  public var infoShadowColor: UIColor = UIColor.black {
    didSet {
      chartInfoView.infoShadowColor = infoShadowColor
    }
  }

  public var infoShadowOpacity: Float = 0.25 {
    didSet {
      chartInfoView.infoShadowOpacity = infoShadowOpacity
    }
  }

  public var font: UIFont = UIFont.systemFont(ofSize: 12, weight: .regular) {
    didSet {
      xAxisView.font = font
      yAxisView.font = font
      chartInfoView.font = font
    }
  }

  public var textColor: UIColor = UIColor(white: 0, alpha: 0.2) {
    didSet {
      xAxisView.textColor = textColor
      yAxisView.textColor = textColor
      chartInfoView.textColor = textColor
    }
  }

  public var gridColor: UIColor = UIColor(white: 0, alpha: 0.2) {
    didSet {
      yAxisView.gridColor = gridColor
    }
  }

  public override var backgroundColor: UIColor? {
    didSet {
      chartInfoView.tooltipBackgroundColor = backgroundColor ?? .white
    }
  }

  public var chartData: ChartPresentationData! {
    didSet {
      lineViews.forEach { $0.removeFromSuperview() }
      lineViews.removeAll()
      for i in (0..<chartData.linesCount).reversed() {
        let line = chartData.lineAt(i)
        let v = ChartLineView()
        v.clipsToBounds = true
        v.chartLine = line
        v.lineWidth = 3
        v.frame = chartsContainerView.bounds
        v.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        chartsContainerView.addSubview(v)
        lineViews.insert(v, at: 0)
      }

      yAxisView.frame = chartsContainerView.bounds
      yAxisView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
      yAxisView.transform = CGAffineTransform.identity.scaledBy(x: 1, y: -1)
      chartsContainerView.addSubview(yAxisView)

      chartInfoView.frame = chartsContainerView.bounds
      chartInfoView.autoresizingMask = [.flexibleWidth, .flexibleHeight]
      chartInfoView.delegate = self
      chartInfoView.textColor = textColor
      chartsContainerView.addSubview(chartInfoView)

      xAxisView.values = chartData.xAxisValues.enumerated().map { ChartXAxisView.Value(index: $0.offset, value: $0.element, text: chartData.labels[$0.offset]) }
      chartPreviewView.chartData = chartData
      xAxisView.setBounds(lower: chartPreviewView.minX, upper: chartPreviewView.maxX)
      updateCharts()
    }
  }

  public typealias OnSelectedPointChangedClosure = (_ px: CGFloat) -> Void
  public var onSelectedPointChanged: OnSelectedPointChangedClosure?

  override init(frame: CGRect) {
    super.init(frame: frame)
    setup()
  }

  required init?(coder aDecoder: NSCoder) {
    super.init(coder: aDecoder)
    setup()
  }

  private func setup() {
    xAxisView.font = font
    xAxisView.textColor = textColor
    yAxisView.font = font
    yAxisView.textColor = textColor
    yAxisView.gridColor = textColor
    chartInfoView.font = font
    chartPreviewView.selectorTintColor = previewTintColor
    chartPreviewView.selectorColor = previewSelectorColor
    chartInfoView.tooltipBackgroundColor = backgroundColor ?? .white
    yAxisView.textBackgroundColor = infoBackgroundColor.withAlphaComponent(0.7)

    tapGR = UITapGestureRecognizer(target: self, action: #selector(onTap(_:)))
    chartsContainerView.addGestureRecognizer(tapGR)
    panGR = UIPanGestureRecognizer(target: self, action: #selector(onPan(_:)))
    chartsContainerView.addGestureRecognizer(panGR)
    pinchGR = UIPinchGestureRecognizer(target: self, action: #selector(onPinch(_:)))
    chartsContainerView.addGestureRecognizer(pinchGR)
    addSubview(chartsContainerView)
    if showPreview {
      addSubview(chartPreviewView)
    }
    chartPreviewView.delegate = self
    addSubview(xAxisView)
  }

  public func setSelectedPoint(_ x: Double) {
    let routeLength = chartData.xAxisValueAt(CGFloat(chartData.pointsCount - 1))
    let upper = chartData.xAxisValueAt(CGFloat(chartPreviewView.maxX))
    var lower = chartData.xAxisValueAt(CGFloat(chartPreviewView.minX))
    let rangeLength = upper - lower
    if x < lower || x > upper {
      let current = Double(chartInfoView.infoX) * rangeLength + lower
      let dx = x - current
      let dIdx = Int(dx / routeLength * Double(chartData.pointsCount))
      var lowerIdx = chartPreviewView.minX + dIdx
      var upperIdx = chartPreviewView.maxX + dIdx
      if lowerIdx < 0 {
        upperIdx -= lowerIdx
        lowerIdx = 0
      } else if upperIdx >= chartData.pointsCount {
        lowerIdx -= upperIdx - chartData.pointsCount - 1
        upperIdx = chartData.pointsCount - 1
      }
      chartPreviewView.setX(min: lowerIdx, max: upperIdx)
      lower = chartData.xAxisValueAt(CGFloat(chartPreviewView.minX))
    }
    chartInfoView.infoX = CGFloat((x - lower) / rangeLength)
  }

  fileprivate func setMyPosition(_ x: Double) {
    let upper = chartData.xAxisValueAt(CGFloat(chartPreviewView.maxX))
    let lower = chartData.xAxisValueAt(CGFloat(chartPreviewView.minX))
    let rangeLength = upper - lower
    chartInfoView.myPositionX = CGFloat((x - lower) / rangeLength)
  }

  override public func layoutSubviews() {
    super.layoutSubviews()

    let previewFrame = showPreview ? CGRect(x: bounds.minX, y: bounds.maxY - 30, width: bounds.width, height: 30) : .zero
    chartPreviewView.frame = previewFrame

    let xAxisFrame = CGRect(x: bounds.minX, y: bounds.maxY - previewFrame.height - 26, width: bounds.width, height: 26)
    xAxisView.frame = xAxisFrame

    let chartsFrame = CGRect(x: bounds.minX,
                             y: bounds.minY,
                             width: bounds.width,
                             height: bounds.maxY - previewFrame.height - xAxisFrame.height)
    chartsContainerView.frame = chartsFrame
  }

  override public func point(inside point: CGPoint, with event: UIEvent?) -> Bool {
    let rect = bounds.insetBy(dx: -30, dy: 0)
    return rect.contains(point)
  }

  @objc func onTap(_ sender: UITapGestureRecognizer) {
    guard sender.state == .ended else {
      return
    }
    let point = sender.location(in: chartInfoView)
    updateCharts(animationStyle: .none)
    chartInfoView.update(point.x)
    chartInfoView(chartInfoView, didMoveToPoint: point.x)
  }

  @objc func onPinch(_ sender: UIPinchGestureRecognizer) {
    if sender.state == .began {
      pinchStartLower = xAxisView.lowerBound
      pinchStartUpper = xAxisView.upperBound
    }

    if sender.state != .changed {
      return
    }

    let rangeLength = CGFloat(pinchStartUpper - pinchStartLower)
    let dx = Int(round((rangeLength * sender.scale - rangeLength) / 2))
    let lower = max(pinchStartLower + dx, 0)
    let upper = min(pinchStartUpper - dx, chartData.labels.count - 1)

    if upper - lower < chartData.labels.count / 10 {
      return
    }
    
    chartPreviewView.setX(min: lower, max: upper)
    xAxisView.setBounds(lower: lower, upper: upper)
    updateCharts(animationStyle: .none)
    chartInfoView.update()
  }

  @objc func onPan(_ sender: UIPanGestureRecognizer) {
    let t = sender.translation(in: chartsContainerView)
    if sender.state == .began {
      panStartPoint = xAxisView.lowerBound
    }

    if sender.state != .changed {
      return
    }

    let dx = Int(round(t.x / chartsContainerView.bounds.width * CGFloat(xAxisView.upperBound - xAxisView.lowerBound)))
    let lower = panStartPoint - dx
    let upper = lower + xAxisView.upperBound - xAxisView.lowerBound
    if lower < 0 || upper > chartData.labels.count - 1 {
      return
    }
    
    chartPreviewView.setX(min: lower, max: upper)
    xAxisView.setBounds(lower: lower, upper: upper)
    updateCharts(animationStyle: .none)
    chartInfoView.update()
  }

  func updateCharts(animationStyle: ChartAnimation = .none) {
    var lower = CGFloat(Int.max)
    var upper = CGFloat(Int.min)

    for i in 0..<chartData.linesCount {
      let line = chartData.lineAt(i)
      let subrange = line.aggregatedValues[xAxisView.lowerBound...xAxisView.upperBound]
      subrange.forEach {
        upper = max($0.y, upper)
        if line.type == .line || line.type == .lineArea {
          lower = min($0.y, lower)
        }
      }
    }

    let padding = round((upper - lower) / 10)
    lower = chartData.formatter.yAxisLowerBound(from: max(0, lower - padding))
    upper = chartData.formatter.yAxisUpperBound(from: upper + padding)
    let steps = chartData.formatter.yAxisSteps(lowerBound: lower, upperBound: upper)

    if yAxisView.upperBound != upper || yAxisView.lowerBound != lower {
      yAxisView.setBounds(lower: lower,
                          upper: upper,
                          lowerLabel: chartData.formatter.yAxisString(from: Double(lower)),
                          upperLabel: chartData.formatter.yAxisString(from: Double(upper)),
                          steps: steps,
                          animationStyle: animationStyle)
    }

    lineViews.forEach {
      $0.setViewport(minX: xAxisView.lowerBound,
                     maxX: xAxisView.upperBound,
                     minY: lower,
                     maxY: upper,
                     animationStyle: animationStyle)
    }
  }
}

extension ChartView: ChartPreviewViewDelegate {
  func chartPreviewView(_ view: ChartPreviewView, didChangeMinX minX: Int, maxX: Int) {
    xAxisView.setBounds(lower: minX, upper: maxX)
    updateCharts(animationStyle: .none)
    chartInfoView.update()
    setMyPosition(myPosition)
    let x = chartInfoView.infoX * CGFloat(xAxisView.upperBound - xAxisView.lowerBound) + CGFloat(xAxisView.lowerBound)
    onSelectedPointChanged?(x)
  }
}

extension ChartView: ChartInfoViewDelegate {
  func chartInfoView(_ view: ChartInfoView, didMoveToPoint pointX: CGFloat) {
    let p = convert(CGPoint(x: pointX, y: 0), from: view)
    let x = (p.x / bounds.width) * CGFloat(xAxisView.upperBound - xAxisView.lowerBound) + CGFloat(xAxisView.lowerBound)
    onSelectedPointChanged?(x)
  }

  func chartInfoView(_ view: ChartInfoView, didCaptureInfoView captured: Bool) {
    panGR.isEnabled = !captured
  }

  func chartInfoView(_ view: ChartInfoView, infoAtPointX pointX: CGFloat) -> (String, [ChartLineInfo])? {
    let p = convert(CGPoint(x: pointX, y: 0), from: view)
    let x = (p.x / bounds.width) * CGFloat(xAxisView.upperBound - xAxisView.lowerBound) + CGFloat(xAxisView.lowerBound)
    let x1 = floor(x)
    let x2 = ceil(x)
    guard Int(x1) < chartData.labels.count && x >= 0 else { return nil }
    let label = chartData.labelAt(x)

    var result: [ChartLineInfo] = []
    for i in 0..<chartData.linesCount {
      let line = chartData.lineAt(i)
      guard line.type != .lineArea else { continue }
      let y1 = line.values.altitude(at: x1 / CGFloat(chartData.pointsCount))
      let y2 = line.values.altitude(at: x2 / CGFloat(chartData.pointsCount))

      let dx = x - x1
      let y = dx * (y2 - y1) + y1
      let py = round(chartsContainerView.bounds.height * CGFloat(y - yAxisView.lowerBound) /
        CGFloat(yAxisView.upperBound - yAxisView.lowerBound))

      let v = round(dx * CGFloat(y2 - y1)) + CGFloat(y1)
      result.append(ChartLineInfo(name: line.name,
                                  color: line.color,
                                  point: chartsContainerView.convert(CGPoint(x: p.x, y: py), to: view),
                                  formattedValue: chartData.formatter.yAxisString(from: Double(v))))
    }

    return (label, result)
  }
}
