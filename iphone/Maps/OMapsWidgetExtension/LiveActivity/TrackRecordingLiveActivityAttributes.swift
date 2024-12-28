import ActivityKit
import AppIntents

struct TrackRecordingLiveActivityAttributes: ActivityAttributes {
  struct ContentState: Codable, Hashable {
    struct StatisticsViewModel: Codable, Hashable {
      let key: String
      let value: String
    }
    let duration: StatisticsViewModel
    let distance: StatisticsViewModel
    let ascent: StatisticsViewModel
    let descent: StatisticsViewModel
    let maxElevation: StatisticsViewModel
    let minElevation: StatisticsViewModel
  }
}
