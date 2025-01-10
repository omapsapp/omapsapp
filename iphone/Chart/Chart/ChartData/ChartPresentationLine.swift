import UIKit

final class ChartPresentationLine {
  private let chartLine: ChartLine

  var aggregatedValues: [ChartValue] = []
  var minY: CGFloat = CGFloat(Int.max)
  var maxY: CGFloat = CGFloat(Int.min)
  var path = UIBezierPath()
  var previewPath = UIBezierPath()

  var values: [ChartValue] { chartLine.values }
  var color: UIColor { chartLine.color }
  var type: ChartLineType { chartLine.type }

  init(_ chartLine: ChartLine) {
    self.chartLine = chartLine
  }
}
