import CoreApi
import map
import kml

@objc
enum TrackRecordingState: Int, Equatable {
  case inactive
  case active
}

enum TrackRecordingAction: String, CaseIterable {
  case start
  case stop
}

enum TrackRecordingError: Error {
  case locationIsProhibited
}

protocol TrackRecordingObservable: AnyObject {
  func addObserver(_ observer: TrackRecordingObserver)
  func removeObserver(_ observer: TrackRecordingObserver)
}

protocol TrackRecordingObserver: AnyObject {
  func trackRecordingStateDidChange(_ state: TrackRecordingState)
  func trackRecordingProgressDidChange(_ trackRecordingInfo: GpsTrackInfo)
}

@objcMembers
final class TrackRecordingManager: NSObject {

  typealias CompletionHandler = () -> Void

  private enum SavingOption {
    case withoutSaving
    case saveWithName(String? = nil)
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
  private let listenerContainer = ListenerContainer<TrackRecordingObserver>()
  private var trackRecordingInfo = GpsTrackInfo()

  var recordingState: TrackRecordingState {
    trackRecorder.isTrackRecordingEnabled() ? .active : .inactive
  }

  private init(trackRecorder: TrackRecorder.Type, activityManager: TrackRecordingActivityManager?) {
    self.trackRecorder = trackRecorder
    self.activityManager = activityManager
    super.init()
    subscribeOnAppLifecycleEvents()

    map.GpsTrackCollection()
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

  func processAction(_ action: TrackRecordingAction, completion: (CompletionHandler)? = nil) {
    switch action {
    case .start:
      start(completion: completion)
    case .stop:
      stop(completion: completion)
    }
  }

  // MARK: - Private methods

  private func checkIsLocationEnabled() throws {
    guard !LocationManager.isLocationProhibited() else {
      throw TrackRecordingError.locationIsProhibited
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
      handleError(error)
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
      self.activityManager?.update(info)
      self.listenerContainer.forEach { $0.trackRecordingProgressDidChange(info) }
    }
  }

  private func unsubscribeFromTrackRecordingProgressUpdates() {
    trackRecorder.setTrackRecordingUpdateHandler(nil)
    trackRecordingInfo = GpsTrackInfo()
  }

  // MARK: - Handle Start/Stop event and Errors

  private func start(completion: (CompletionHandler)? = nil) {
    do {
      try checkIsLocationEnabled()
      switch recordingState {
      case .inactive:
        subscribeOnTrackRecordingProgressUpdates()
        trackRecorder.startTrackRecording()
        listenerContainer.forEach {
          $0.trackRecordingStateDidChange(self.recordingState)
          $0.trackRecordingProgressDidChange(self.trackRecordingInfo)
        }
      case .active:
        break
      }
      completion?()
    } catch {
      handleError(error, completion: completion)
    }
  }

  private func stop(completion: (CompletionHandler)? = nil) {
    guard !trackRecorder.isTrackRecordingEmpty() else {
      Toast.toast(withText: L("track_recording_toast_nothing_to_save")).show()
      stopRecording(.withoutSaving, completion: completion)
      return
    }
    Self.showOnFinishRecordingAlert(onSave: { [weak self] in
      guard let self else { return }
      // TODO: (KK) pass the user provided name from the track saving screen (when it will be implemented)
      self.stopRecording(.saveWithName(), completion: completion)
    },
                                    onStop: { [weak self] in
      guard let self else { return }
      self.stopRecording(.withoutSaving, completion: completion)
    },
                                    onContinue: {
      completion?()
    })
  }

  private func stopRecording(_ savingOption: SavingOption, completion: (CompletionHandler)? = nil) {
    unsubscribeFromTrackRecordingProgressUpdates()
    trackRecorder.stopTrackRecording()
    activityManager?.stop()
    listenerContainer.forEach {
      $0.trackRecordingStateDidChange(self.recordingState)
    }
    switch savingOption {
    case .withoutSaving:
      break
    case .saveWithName(let name):
      trackRecorder.saveTrackRecording(withName: name)
    }
    completion?()
  }

  private func handleError(_ error: Error, completion: (CompletionHandler)? = nil) {
    LOG(.error, error.localizedDescription)
    switch error {
    case TrackRecordingError.locationIsProhibited:
      // Show alert to enable location
      LocationManager.checkLocationStatus()
    default:
      break
    }
    stopRecording(.withoutSaving, completion: completion)
  }

  private static func showOnFinishRecordingAlert(onSave: @escaping CompletionHandler,
                                                 onStop: @escaping CompletionHandler,
                                                 onContinue: @escaping CompletionHandler) {
    let alert = UIAlertController(title: L("track_recording_alert_title"), message: nil, preferredStyle: .alert)
    alert.addAction(UIAlertAction(title: L("continue_recording"), style: .default, handler: { _ in onContinue() }))
    alert.addAction(UIAlertAction(title: L("stop_without_saving"), style: .default, handler: { _ in onStop() }))
    alert.addAction(UIAlertAction(title: L("save"), style: .cancel, handler: { _ in onSave() }))
    UIViewController.topViewController().present(alert, animated: true)
  }
}

// MARK: - TrackRecordingObservable

extension TrackRecordingManager: TrackRecordingObservable {
  func addObserver(_ observer: TrackRecordingObserver) {
    listenerContainer.addListener(observer)
    observer.trackRecordingStateDidChange(recordingState)
    observer.trackRecordingProgressDidChange(trackRecordingInfo)
  }

  func removeObserver(_ observer: TrackRecordingObserver) {
    listenerContainer.removeListener(observer)
  }
}
