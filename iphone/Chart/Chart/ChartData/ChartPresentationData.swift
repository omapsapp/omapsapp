import UIKit

public class ChartPresentationData {
  private let chartData: ChartData
  private var presentationLines: [ChartPresentationLine]
  private let pathBuilder = DefaultChartPathBuilder()

  let formatter: ChartFormatter
  var linesCount: Int { chartData.lines.count }
  var pointsCount: Int { chartData.xAxisValues.count }
  var xAxisValues: [Double] { chartData.xAxisValues }
  var type: ChartType { chartData.type }
  var labels: [String]
  var lower = CGFloat(Int.max)
  var upper = CGFloat(Int.min)

  public init(_ chartData: ChartData, formatter: ChartFormatter) {
    self.chartData = chartData
    self.formatter = formatter
    self.presentationLines = chartData.lines.map { ChartPresentationLine($0) }
    self.labels = chartData.xAxisValues.map { formatter.xAxisString(from: $0) }
    recalculateBounds()
  }

  func labelAt(_ point: CGFloat) -> String {
    formatter.xAxisString(from: xAxisValueAt(point))
  }

  func xAxisValueAt(_ point: CGFloat) -> Double {
    let distance = chartData.xAxisValues.last!
    let p1 = floor(point)
    let p2 = ceil(point)
    let v1 = p1 / CGFloat(pointsCount) * distance
    let v2 = p2 / CGFloat(pointsCount) * distance
    return v1 + (v2 - v1) * Double(point.truncatingRemainder(dividingBy: 1))
  }

  func lineAt(_ index: Int) -> ChartPresentationLine {
    presentationLines[index]
  }

  private func recalculateBounds() {
    presentationLines.forEach { $0.aggregatedValues = [] }
    pathBuilder.build(presentationLines, type: type)

    var l = CGFloat(Int.max)
    var u = CGFloat(Int.min)
    presentationLines.forEach {
      l = min($0.minY, l)
      u = max($0.maxY, u)
    }
    lower = l
    upper = u
  }
}

