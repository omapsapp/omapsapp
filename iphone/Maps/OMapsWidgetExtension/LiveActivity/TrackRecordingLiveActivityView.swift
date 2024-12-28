import SwiftUI
import WidgetKit

#if canImport(ActivityKit)

struct TrackRecordingLiveActivityView: View {
  let state: TrackRecordingLiveActivityAttributes.ContentState

  var body: some View {
    VStack(alignment: .leading, spacing: 12) {
      HStack(alignment: .top, spacing: 24) {
        StatisticValueView(state.duration.value)
        StatisticValueView(state.distance.value)
        Spacer()
        AppLogo()
          .frame(width: 20, height: 20)
      }
      .padding([.top, .leading, .trailing], 16)
      HStack(alignment: .top, spacing: 20) {
        StatisticDetailView(title: state.ascent.value, subtitle: state.ascent.key)
        StatisticDetailView(title: state.descent.value, subtitle: state.descent.key)
        StatisticDetailView(title: state.maxElevation.value, subtitle: state.maxElevation.key)
        StatisticDetailView(title: state.minElevation.value, subtitle: state.minElevation.key)
      }
      .padding([.leading, .trailing, .bottom], 16)
    }
    .activityBackgroundTint(.black.opacity(0.2))
    // Uncomment the line below to simulate the background color in Preview because the `activityBackgroundTint` can only displayed on the device or simulator.
    //.background(.black.opacity(0.85))
  }
}

#if DEBUG
struct TrackRecordingLiveActivityWidget_Previews: PreviewProvider {
  static var previews: some View {
    Group {
      let activityAttributes = TrackRecordingLiveActivityAttributes()
      let activityState = TrackRecordingLiveActivityAttributes.ContentState(
        duration: .init(
          key: "Duration",
          value: "1h 12min"
        ),
        distance: .init(
          key: "Distance",
          value: "12 km"
        ),
        ascent: .init(
          key: "Ascent",
          value: "100 m"
        ),
        descent: .init(
          key: "Descent",
          value: "100 m"
        ),
        maxElevation: .init(
          key: "Max Elevation",
          value: "100 m"
        ),
        minElevation: .init(
          key: "Min Elevation",
          value: "10 m"
        )
      )

      activityAttributes
        .previewContext(activityState, viewKind: .content)
        .previewDisplayName("Notification")
    }
  }
}
#endif

#endif
