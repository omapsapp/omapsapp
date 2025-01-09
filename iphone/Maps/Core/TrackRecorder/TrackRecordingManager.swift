@objc
enum TrackRecordingState: Int, Equatable {
  case inactive
  case active
}

enum TrackRecordingAction {
  case start
  case stopAndSave(name: String? = nil)
}

enum TrackRecordingError: Error {
  case locationIsProhibited
  case trackIsEmpty
  case systemError(Error)
}

enum TrackRecordingActionResult {
  case success
  case error(TrackRecordingError)
}

@objc
protocol TrackRecordingObservable: AnyObject {
  var recordingState: TrackRecordingState { get }
  var trackRecordingInfo: TrackInfo { get }

  func addObserver(_ observer: AnyObject, recordingIsActiveDidChangeHandler: @escaping TrackRecordingStateHandler)
  func removeObserver(_ observer: AnyObject)
}

typealias TrackRecordingStateHandler = (TrackRecordingState, TrackInfo) -> Void

@objcMembers
final class TrackRecordingManager: NSObject {

  typealias CompletionHandler = (TrackRecordingActionResult) -> Void

  fileprivate struct Observation {
    weak var observer: AnyObject?
    var recordingStateDidChangeHandler: TrackRecordingStateHandler?
  }

  static let shared: TrackRecordingManager = {
    let trackRecorder = FrameworkHelper.self
    var activityManager: TrackRecordingActivityManager? = nil
    #if canImport(ActivityKit)
    if #available(iOS 16.2, *) {
      activityManager = TrackRecordingLiveActivityManager.shared
    }
    #endif
    return TrackRecordingManager(trackRecorder: trackRecorder, activityManager: activityManager)
  }()

  private let trackRecorder: TrackRecorder.Type
  private var activityManager: TrackRecordingActivityManager?
  private var observations: [Observation] = []
  private(set) var trackRecordingInfo: TrackInfo = .empty()

  var recordingState: TrackRecordingState {
    trackRecorder.isTrackRecordingEnabled() ? .active : .inactive
  }

  private init(trackRecorder: TrackRecorder.Type, activityManager: TrackRecordingActivityManager?) {
    self.trackRecorder = trackRecorder
    self.activityManager = activityManager
    super.init()
    subscribeOnAppLifecycleEvents()
  }

  // MARK: - Public methods

  @objc
  func setup() {
    do {
      try checkIsLocationEnabled()
      switch recordingState {
      case .inactive:
        break
      case .active:
        subscribeOnTrackRecordingProgressUpdates()
      }
    } catch {
      handleError(error)
    }
  }

  @objc
  func isActive() -> Bool {
    recordingState == .active
  }

  func processAction(_ action: TrackRecordingAction, completion: (CompletionHandler)? = nil) {
    do {
      switch action {
      case .start:
        try checkIsLocationEnabled()
        start()
      case .stopAndSave(let name):
        stop()
        try checkIsTrackNotEmpty()
        saveWithName(name)
      }
      DispatchQueue.main.async {
        completion?(.success)
      }
    } catch {
      handleError(error, completion: completion)
    }
  }

  // MARK: - Private methods

  private func checkIsLocationEnabled() throws(TrackRecordingError) {
    if LocationManager.isLocationProhibited() {
      throw TrackRecordingError.locationIsProhibited
    }
  }

  private func checkIsTrackNotEmpty() throws(TrackRecordingError) {
    if trackRecorder.isTrackRecordingEmpty() {
      throw TrackRecordingError.trackIsEmpty
    }
  }

  // MARK: - Handle lifecycle events

  private func subscribeOnAppLifecycleEvents() {
    NotificationCenter.default.addObserver(self, selector: #selector(willResignActive), name: UIApplication.willResignActiveNotification, object: nil)
    NotificationCenter.default.addObserver(self, selector: #selector(willEnterForeground), name: UIApplication.willEnterForegroundNotification, object: nil)
    NotificationCenter.default.addObserver(self, selector: #selector(prepareForTermination), name: UIApplication.willTerminateNotification, object: nil)
  }

  @objc
  private func willResignActive() {
    guard let activityManager, recordingState == .active else { return }
    do {
      try activityManager.start(with: trackRecordingInfo)
    } catch {
      handleError(.systemError(error))
    }
  }

  @objc
  private func willEnterForeground() {
    activityManager?.stop()
  }

  @objc
  private func prepareForTermination() {
    activityManager?.stop()
  }

  // MARK: - Handle track recording process

  private func subscribeOnTrackRecordingProgressUpdates() {
    trackRecorder.setTrackRecordingUpdateHandler { [weak self] info in
      guard let self else { return }
      self.trackRecordingInfo = info
      self.notifyObservers()
      self.activityManager?.update(info)
    }
  }

  private func unsubscribeFromTrackRecordingProgressUpdates() {
    trackRecorder.setTrackRecordingUpdateHandler(nil)
  }

  // MARK: - Handle Start/Stop event and Errors

  private func start() {
    switch recordingState {
    case .inactive:
      trackRecorder.startTrackRecording()
      subscribeOnTrackRecordingProgressUpdates()
      notifyObservers()
    case .active:
      break
    }
  }

  private func stop() {
    unsubscribeFromTrackRecordingProgressUpdates()
    trackRecorder.stopTrackRecording()
    trackRecordingInfo = .empty()
    activityManager?.stop()
    notifyObservers()
  }

  private func saveWithName(_ name: String?) {
    trackRecorder.saveTrackRecording(withName: name)
  }

  private func handleError(_ error: TrackRecordingError, completion: (CompletionHandler)? = nil) {
    switch error {
    case TrackRecordingError.locationIsProhibited:
      // Show alert to enable location
      LocationManager.checkLocationStatus()
    case TrackRecordingError.trackIsEmpty:
      Toast.toast(withText: L("track_recording_toast_nothing_to_save")).show()
    case TrackRecordingError.systemError(let error):
      LOG(.error, error.localizedDescription)
      break
    }
    DispatchQueue.main.async {
      completion?(.error(error))
    }
  }
}

// MARK: - TrackRecordingObserver

extension TrackRecordingManager: TrackRecordingObservable {
  @objc
  func addObserver(_ observer: AnyObject, recordingIsActiveDidChangeHandler: @escaping TrackRecordingStateHandler) {
    let observation = Observation(observer: observer, recordingStateDidChangeHandler: recordingIsActiveDidChangeHandler)
    observations.append(observation)
    recordingIsActiveDidChangeHandler(recordingState, trackRecordingInfo)
  }

  @objc
  func removeObserver(_ observer: AnyObject) {
    observations.removeAll { $0.observer === observer }
  }

  private func notifyObservers() {
    observations = observations.filter { $0.observer != nil }
    observations.forEach { $0.recordingStateDidChangeHandler?(recordingState, trackRecordingInfo) }
  }
}
