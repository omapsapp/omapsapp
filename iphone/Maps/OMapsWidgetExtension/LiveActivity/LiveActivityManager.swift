import ActivityKit
import SwiftUI

@available(iOS 16.2, *)
final class LiveActivityManager {
  static func startActivity<T: ActivityAttributes>(_ attributes: T, content: ActivityContent<T.ContentState>) throws -> Activity<T> {
    return try Activity.request(attributes: attributes, content: content, pushType: nil)
  }

  static func update<T: ActivityAttributes>(_ activity: Activity<T>, content: ActivityContent<T.ContentState>) {
    Task {
      await activity.update(content)
    }
  }

  static func stop(_ activity: Activity<some ActivityAttributes>) {
    // semaphore is used for removing the activity during the app termination
    let semaphore = DispatchSemaphore(value: 0)
    Task {
      await activity.end(nil, dismissalPolicy: .immediate)
      semaphore.signal()
    }
    semaphore.wait()
  }
}

