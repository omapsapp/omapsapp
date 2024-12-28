import WidgetKit
import SwiftUI

struct TrackRecordingLiveActivityConfiguration: Widget {
  @Environment(\.colorScheme) private var colorScheme

  var body: some WidgetConfiguration {
    ActivityConfiguration(for: TrackRecordingLiveActivityAttributes.self) { context in
      TrackRecordingLiveActivityView(state: context.state)
    } dynamicIsland: { context in
      DynamicIsland {
        DynamicIslandExpandedRegion(.center) {
          // TODO: Implement the expanded view
        }
      } compactLeading: {
        AppLogo()
      } compactTrailing: {
        StatisticValueView(context.state.duration.value)
      } minimal: {
        // TODO: Implement the minimal view
      }
    }
  }
}


