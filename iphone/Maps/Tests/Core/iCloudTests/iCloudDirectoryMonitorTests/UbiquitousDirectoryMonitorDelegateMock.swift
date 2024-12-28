import XCTest
@testable import Organic_Maps__Debug_

class UbiquitousDirectoryMonitorDelegateMock: CloudDirectoryMonitorDelegate {
  var didFinishGatheringCalled = false
  var didUpdateCalled = false
  var didReceiveErrorCalled = false

  var didFinishGatheringExpectation: XCTestExpectation?
  var didUpdateExpectation: XCTestExpectation?
  var didReceiveErrorExpectation: XCTestExpectation?

  var contents = CloudContents()

  func didFinishGathering(_ contents: CloudContents) {
    didFinishGatheringCalled = true
    didFinishGatheringExpectation?.fulfill()
    self.contents = contents
  }

  func didUpdate(_ contents: CloudContents, _ update: CloudContentsUpdate) {
    didUpdateCalled = true
    didUpdateExpectation?.fulfill()
    self.contents = contents
  }

  func didReceiveCloudMonitorError(_ error: Error) {
    didReceiveErrorCalled = true
    didReceiveErrorExpectation?.fulfill()
  }
}
