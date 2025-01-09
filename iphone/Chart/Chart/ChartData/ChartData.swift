import UIKit

public enum ChartType {
  case regular
  case percentage
}

public enum ChartLineType: String {
  case line
  case lineArea
}

public protocol ChartFormatter {
  func xAxisString(from value: Double) -> String
  func yAxisString(from value: Double) -> String
  func yAxisLowerBound(from value: CGFloat) -> CGFloat
  func yAxisUpperBound(from value: CGFloat) -> CGFloat
  func yAxisSteps(lowerBound: CGFloat, upperBound: CGFloat) -> [CGFloat]
}

public protocol ChartData {
  var xAxisValues: [Double] { get }
  var lines: [ChartLine] { get }
  var type: ChartType { get }
}

public protocol ChartLine {
  var values: [ChartValue] { get }
  var color: UIColor { get }
  var type: ChartLineType { get }
}

public struct ChartValue {
  let x: CGFloat
  let y: CGFloat

  public init(xValues: CGFloat, y: CGFloat) {
    self.x = xValues
    self.y = y
  }
}

extension Array where Element == ChartValue {
  var maxDistance: CGFloat { return map { $0.x }.max() ?? 0 }

  func altitude(at relativeDistance: CGFloat) -> CGFloat {
    guard let distance = last?.x else { return 0 }
    return first { $0.x >= distance * relativeDistance }?.y ?? 0
  }
}
