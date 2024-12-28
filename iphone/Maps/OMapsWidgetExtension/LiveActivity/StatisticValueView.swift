import SwiftUI

struct StatisticValueView: View {
  private let value: String

  init(_ value: String) {
    self.value = value
  }

  var body: some View {
    Text(value)
      .contentTransition(.numericText())
      .minimumScaleFactor(0.1)
      .font(.title3.bold().monospacedDigit())
      .foregroundStyle(.white)
  }
}
