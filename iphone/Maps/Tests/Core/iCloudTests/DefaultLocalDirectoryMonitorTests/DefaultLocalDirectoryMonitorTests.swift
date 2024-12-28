import XCTest
@testable import Organic_Maps__Debug_

final class DefaultLocalDirectoryMonitorTests: XCTestCase {

  let fileManager = FileManager.default
  let tempDirectory = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
  var directoryMonitor: FileSystemDispatchSourceMonitor!
  var mockDelegate: LocalDirectoryMonitorDelegateMock!

  override func setUpWithError() throws {
    try super.setUpWithError()
    // Setup with a temporary directory and a mock delegate
    directoryMonitor = try FileSystemDispatchSourceMonitor(fileManager: fileManager, directory: tempDirectory)
    mockDelegate = LocalDirectoryMonitorDelegateMock()
    directoryMonitor.delegate = mockDelegate
  }

  override func tearDownWithError() throws {
    directoryMonitor.stop()
    mockDelegate = nil
    try? fileManager.removeItem(at: tempDirectory)
    try super.tearDownWithError()
  }

  func testInitialization() {
    XCTAssertEqual(directoryMonitor.directory, tempDirectory, "Monitor initialized with incorrect directory.")
    XCTAssertTrue(directoryMonitor.state == .stopped, "Monitor should be stopped initially.")
  }

  func testStartMonitoring() {
    let startExpectation = expectation(description: "Start monitoring")
    directoryMonitor.start { result in
      switch result {
      case .success:
        XCTAssertTrue(self.directoryMonitor.state == .started, "Monitor should be started.")
      case .failure(let error):
        XCTFail("Monitoring failed to start with error: \(error)")
      }
      startExpectation.fulfill()
    }
    wait(for: [startExpectation], timeout: 5.0)
  }

  func testStopMonitoring() {
    directoryMonitor.start()
    directoryMonitor.stop()
    XCTAssertTrue(directoryMonitor.state == .stopped, "Monitor should be stopped.")
  }

  func testPauseAndResumeMonitoring() {
    directoryMonitor.start()
    directoryMonitor.pause()
    XCTAssertTrue(directoryMonitor.state == .paused, "Monitor should be paused.")

    directoryMonitor.resume()
    XCTAssertTrue(directoryMonitor.state == .started, "Monitor should be started.")
  }

  func testDelegateDidFinishGathering() {
    mockDelegate.didFinishGatheringExpectation = expectation(description: "didFinishGathering called")
    directoryMonitor.start()
    wait(for: [mockDelegate.didFinishGatheringExpectation!], timeout: 5.0)
  }

  func testDelegateDidReceiveError() {
    mockDelegate.didReceiveErrorExpectation = expectation(description: "didReceiveLocalMonitorError called")

    let error = NSError(domain: NSCocoaErrorDomain, code: NSFileNoSuchFileError, userInfo: nil)
    directoryMonitor.delegate?.didReceiveLocalMonitorError(error)

    wait(for: [mockDelegate.didReceiveErrorExpectation!], timeout: 1.0)
  }

  func testContentUpdateDetection() {
    let startExpectation = expectation(description: "Start monitoring")
    let didFinishGatheringExpectation = expectation(description: "didFinishGathering called")
    let didUpdateExpectation = expectation(description: "didUpdate called")

    mockDelegate.didFinishGatheringExpectation = didFinishGatheringExpectation
    mockDelegate.didUpdateExpectation = didUpdateExpectation

    directoryMonitor.start { result in
      if case .success = result {
        XCTAssertTrue(self.directoryMonitor.state == .started, "Monitor should be started.")
      }
      startExpectation.fulfill()
    }

    wait(for: [startExpectation], timeout: 5)

    let fileURL = tempDirectory.appendingPathComponent("test.kml")
    
    DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
      self.fileManager.createFile(atPath: fileURL.path, contents: Data(), attributes: nil)
    }

    wait(for: [didFinishGatheringExpectation, didUpdateExpectation], timeout: 20)
  }

  func testFileWithIncorrectExtension() {
    let startExpectation = expectation(description: "Start monitoring")
    let didFinishGatheringExpectation = expectation(description: "didFinishGathering called")
    mockDelegate.didFinishGatheringExpectation = didFinishGatheringExpectation

    let file1URL = tempDirectory.appendingPathComponent("test.kml.tmp")
    let file2URL = tempDirectory.appendingPathComponent("test2.tmp")
    let file3URL = tempDirectory.appendingPathComponent("test3.jpg")
    let correctFileURL = tempDirectory.appendingPathComponent("test.kml")

    var fileData = Data(count: 12)
    try! fileData.write(to: file1URL, options: .atomic)
    try! fileData.write(to: file2URL, options: .atomic)
    try! fileData.write(to: file3URL, options: .atomic)
    try! fileData.write(to: correctFileURL, options: .atomic)

    directoryMonitor.start { result in
      switch result {
        case .failure(let error):
          XCTFail("Monitoring failed to start with error: \(error)")
      case .success:
        XCTAssertTrue(self.directoryMonitor.state == .started, "Monitor should be started.")
        startExpectation.fulfill()
      }
    }
    wait(for: [startExpectation, didFinishGatheringExpectation], timeout: 5)

    let contents = self.mockDelegate.contents.map { $0.fileUrl }
    XCTAssertFalse(contents.contains(file1URL), "File with incorrect extension should not be included")
    XCTAssertFalse(contents.contains(file2URL), "File with incorrect extension should not be included")
    XCTAssertFalse(contents.contains(file3URL), "File with incorrect extension should not be included")
    XCTAssertTrue(contents.contains(correctFileURL), "File with correct extension should be included")
  }
}
