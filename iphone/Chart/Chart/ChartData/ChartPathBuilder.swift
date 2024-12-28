import UIKit

protocol ChartPathBuilder {
  func build(_ lines: [ChartPresentationLine])
  func makeLinePath(line: ChartPresentationLine) -> UIBezierPath
  func makePercentLinePath(line: ChartPresentationLine, bottomLine: ChartPresentationLine?) -> UIBezierPath
}

extension ChartPathBuilder {
  func makeLinePreviewPath(line: ChartPresentationLine) -> UIBezierPath {
    let path = UIBezierPath()
    let values = line.values
    let xScale = CGFloat(values.count) / values.maxDistance
    for i in 0..<values.count {
      let x = values[i].x * xScale
      let y = values[i].y - line.minY
      if i == 0 {
        path.move(to: CGPoint(x: x, y: y))
      } else {
        path.addLine(to: CGPoint(x: x, y: y))
      }
    }
    return path
  }

  func makeLinePath(line: ChartPresentationLine) -> UIBezierPath {
    let path = UIBezierPath()
    let values = line.values
    let xScale = CGFloat(values.count) / values.maxDistance
    for i in 0..<values.count {
      let x = values[i].x * xScale
      let y = values[i].y - line.minY
      if i == 0 {
        path.move(to: CGPoint(x: x, y: y))
      } else {
        path.addLine(to: CGPoint(x: x, y: y))
      }
    }
    return path
  }

  func makePercentLinePreviewPath(line: ChartPresentationLine, bottomLine: ChartPresentationLine?) -> UIBezierPath {
    let path = UIBezierPath()
    path.move(to: CGPoint(x: 0, y: 0))
    let aggregatedValues = line.aggregatedValues
    let xScale = CGFloat(aggregatedValues.count) / aggregatedValues.maxDistance
    for i in 0..<aggregatedValues.count {
      let x = aggregatedValues[i].x * xScale
      let y = aggregatedValues[i].y - CGFloat(line.minY)
      path.addLine(to: CGPoint(x: x, y: y))
    }
    path.addLine(to: CGPoint(x: aggregatedValues.maxDistance, y: 0))
    path.close()
    return path
  }

  func makePercentLinePath(line: ChartPresentationLine, bottomLine: ChartPresentationLine?) -> UIBezierPath {
    let path = UIBezierPath()
    path.move(to: CGPoint(x: 0, y: 0))
    let aggregatedValues = line.aggregatedValues
    let xScale = CGFloat(aggregatedValues.count) / aggregatedValues.maxDistance
    for i in 0..<aggregatedValues.count {
      let x = aggregatedValues[i].x * xScale
      let y = aggregatedValues[i].y - CGFloat(line.minY)
      path.addLine(to: CGPoint(x: x, y: y))
    }
    path.addLine(to: CGPoint(x: aggregatedValues.maxDistance, y: 0))
    path.close()
    return path
  }
}

final class DefaultChartPathBuilder {
  private let builders: [ChartType: ChartPathBuilder] = [
    .regular : LinePathBuilder(),
    .percentage : PercentagePathBuilder()
  ]

  func build(_ lines: [ChartPresentationLine], type: ChartType) {
    builders[type]?.build(lines)
  }
}

final class LinePathBuilder: ChartPathBuilder {
  func build(_ lines: [ChartPresentationLine]) {
    lines.forEach {
      $0.aggregatedValues = $0.values
      if $0.type == .lineArea {
        $0.minY = 0
        for val in $0.values {
          $0.maxY = max(val.y, $0.maxY)
        }
        $0.path = makePercentLinePath(line: $0, bottomLine: nil)
        $0.previewPath = makePercentLinePreviewPath(line: $0, bottomLine: nil)
      } else {
        for val in $0.values {
          $0.minY = min(val.y, $0.minY)
          $0.maxY = max(val.y, $0.maxY)
        }
        $0.path = makeLinePath(line: $0)
        $0.previewPath = makeLinePreviewPath(line: $0)
      }
    }
  }
}

final class PercentagePathBuilder: ChartPathBuilder {
  func build(_ lines: [ChartPresentationLine]) {
    lines.forEach {
      $0.minY = 0
      $0.maxY = CGFloat(Int.min)
    }

    for i in 0..<lines[0].values.count {
      let sum = CGFloat(lines.reduce(0) { (r, l) in r + l.values[i].y })
      var aggrPercentage: CGFloat = 0
      lines.forEach {
        aggrPercentage += CGFloat($0.values[i].y) / sum * 100
        $0.aggregatedValues.append(ChartValue(xValues: lines[0].values[i].x, y: aggrPercentage))
        $0.maxY = max(round(aggrPercentage), CGFloat($0.maxY))
      }
    }

    var prevLine: ChartPresentationLine? = nil
    lines.forEach {
      $0.path = makePercentLinePath(line: $0, bottomLine: prevLine)
      $0.previewPath = makePercentLinePreviewPath(line: $0, bottomLine: prevLine)
      prevLine = $0
    }
  }
}
