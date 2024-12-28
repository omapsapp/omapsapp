import XCTest
@testable import Organic_Maps__Debug_

class LocalDirectoryMonitorDelegateMock: LocalDirectoryMonitorDelegate {
  var contents = LocalContents()

  var didFinishGatheringExpectation: XCTestExpectation?
  var didUpdateExpectation: XCTestExpectation?
  var didReceiveErrorExpectation: XCTestExpectation?

  func didFinishGathering(_ contents: LocalContents) {
    self.contents = contents
    didFinishGatheringExpectation?.fulfill()
  }

  func didUpdate(_ contents: LocalContents, _ update: LocalContentsUpdate) {
    self.contents = contents
    didUpdateExpectation?.fulfill()
  }

  func didReceiveLocalMonitorError(_ error: Error) {
    didReceiveErrorExpectation?.fulfill()
  }
}
