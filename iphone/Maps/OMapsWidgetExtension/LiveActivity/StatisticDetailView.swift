import SwiftUI

struct StatisticDetailView: View {
  private let title: String
  private let subtitle: String

  init(title: String, subtitle: String) {
    self.title = title
    self.subtitle = subtitle
  }

  var body: some View {
    VStack(alignment: .leading) {
      Text(title)
        .contentTransition(.numericText())
        .font(.system(size: 14, weight: .bold).monospacedDigit())
        .minimumScaleFactor(0.5)
        .lineLimit(1)
        .foregroundStyle(.white)
      Text(subtitle)
        .font(.system(size: 10))
        .minimumScaleFactor(0.5)
        .lineLimit(2)
        .multilineTextAlignment(.center)
        .foregroundStyle(.white)
    }
  }
}
