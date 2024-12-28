import ActivityKit

#if canImport(ActivityKit)

protocol TrackRecordingActivityManager {
  func start(with info: TrackInfo) throws
  func update(_ info: TrackInfo)
  func stop()
}

@available(iOS 16.2, *)
final class TrackRecordingLiveActivityManager {

  static let shared = TrackRecordingLiveActivityManager()

  private var activity: Activity<TrackRecordingLiveActivityAttributes>?

  private init() {}
}

// MARK: - TrackRecordingActivityManager

@available(iOS 16.2, *)
extension TrackRecordingLiveActivityManager: TrackRecordingActivityManager {
  func start(with info: TrackInfo) throws {
    guard activity == nil else { return }
    let state = TrackRecordingLiveActivityAttributes.ContentState(trackInfo: info)
    let content = ActivityContent<TrackRecordingLiveActivityAttributes.ContentState>(state: state, staleDate: nil)
    let attributes = TrackRecordingLiveActivityAttributes()
    activity = try LiveActivityManager.startActivity(attributes, content: content)
  }

  func update(_ info: TrackInfo) {
    guard let activity else { return }
    let state = TrackRecordingLiveActivityAttributes.ContentState(trackInfo: info)
    let content = ActivityContent<TrackRecordingLiveActivityAttributes.ContentState>(state: state, staleDate: nil)
    LiveActivityManager.update(activity, content: content)
  }

  func stop() {
    guard let activity else { return }
    LiveActivityManager.stop(activity)
    self.activity = nil
  }
}

// MARK: - Wrap TrackRecordingInfo to TrackRecordingLiveActivityAttributes.ContentState

private extension TrackRecordingLiveActivityAttributes.ContentState {
  init(trackInfo: TrackInfo) {
    let distance = DistanceFormatter.distanceString(fromMeters: trackInfo.distance)
    let duration = DurationFormatter.durationString(from: trackInfo.duration)
    let maxElevation = AltitudeFormatter.altitudeString(fromMeters: Double(trackInfo.maxElevation))
    let minElevation = AltitudeFormatter.altitudeString(fromMeters: Double(trackInfo.minElevation))
    let ascent = AltitudeFormatter.altitudeString(fromMeters: Double(trackInfo.ascent))
    let descent = AltitudeFormatter.altitudeString(fromMeters: Double(trackInfo.descent))

    self.distance = StatisticsViewModel(key: "", value: distance)
    self.duration = StatisticsViewModel(key: "", value: duration)
    self.maxElevation = StatisticsViewModel(key: L("elevation_profile_max_elevation"), value: maxElevation)
    self.minElevation = StatisticsViewModel(key: L("elevation_profile_min_elevation"), value: minElevation)
    self.ascent = StatisticsViewModel(key: L("elevation_profile_ascent"), value: ascent)
    self.descent = StatisticsViewModel(key: L("elevation_profile_descent"), value: descent)
  }
}

#endif
