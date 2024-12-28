import Chart
import CoreApi

final class ElevationProfileFormatter {

  private enum Constants {
    static let metricToImperialMultiplier: CGFloat = 0.3048
    static var metricAltitudeStep: CGFloat = 50
    static var imperialAltitudeStep: CGFloat = 100
  }

  private let distanceFormatter: DistanceFormatter.Type
  private let altitudeFormatter: AltitudeFormatter.Type
  private let unitSystemMultiplier: CGFloat
  private let altitudeStep: CGFloat
  private let units: Units

  init(units: Units = Settings.measurementUnits()) {
    self.units = units
    self.distanceFormatter = DistanceFormatter.self
    self.altitudeFormatter = AltitudeFormatter.self
    switch units {
    case .metric:
      self.altitudeStep = Constants.metricAltitudeStep
      self.unitSystemMultiplier = 1
    case .imperial:
      self.altitudeStep = Constants.imperialAltitudeStep
      self.unitSystemMultiplier = Constants.metricToImperialMultiplier
    @unknown default:
      fatalError("Unsupported units")
    }
  }
}

extension ElevationProfileFormatter: ChartFormatter {
  func xAxisString(from value: Double) -> String {
    distanceFormatter.distanceString(fromMeters: value)
  }

  func yAxisString(from value: Double) -> String {
    altitudeFormatter.altitudeString(fromMeters: value)
  }

  func yAxisLowerBound(from value: CGFloat) -> CGFloat {
    floor((value / unitSystemMultiplier) / altitudeStep) * altitudeStep * unitSystemMultiplier
  }

  func yAxisUpperBound(from value: CGFloat) -> CGFloat {
    ceil((value / unitSystemMultiplier) / altitudeStep) * altitudeStep * unitSystemMultiplier
  }

  func yAxisSteps(lowerBound: CGFloat, upperBound: CGFloat) -> [CGFloat] {
    let lower = yAxisLowerBound(from: lowerBound)
    let upper = yAxisUpperBound(from: upperBound)
    let range = upper - lower
    var stepSize = altitudeStep
    var stepsCount = Int((range / stepSize).rounded(.up))

    while stepsCount > 6 {
      stepSize *= 2 // Double the step size to reduce the step count
      stepsCount = Int((range / stepSize).rounded(.up))
    }

    let steps = stride(from: lower, through: upper, by: stepSize)
    return Array(steps)
  }
}
