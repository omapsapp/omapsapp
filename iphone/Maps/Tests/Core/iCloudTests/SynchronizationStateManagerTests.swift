import XCTest
@testable import Organic_Maps__Debug_

final class SynchronizationtateManagerTests: XCTestCase {

  var syncStateManager: SynchronizationStateResolver!
  var outgoingEvents: [OutgoingSynchronizationEvent] = []

  override func setUp() {
    super.setUp()
    syncStateManager = iCloudSynchronizationStateResolver(isInitialSynchronization: false)
  }

  override func tearDown() {
    syncStateManager = nil
    outgoingEvents.removeAll()
    super.tearDown()
  }
  // MARK: - Test didFinishGathering without errors and on initial synchronization
  func testInitialSynchronization() {
    syncStateManager = iCloudSynchronizationStateResolver(isInitialSynchronization: true)

    let localItem1 = LocalMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let localItem2 = LocalMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(3)) // Local only item

    let cloudItem1 = CloudMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(2)) // Conflicting item
    let cloudItem3 = CloudMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(4)) // Cloud only item

    let localItems: LocalContents = [localItem1, localItem2]
    let cloudItems: CloudContents = [cloudItem1, cloudItem3]

    outgoingEvents.append(contentsOf: syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems)))
    outgoingEvents.append(contentsOf: syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems)))

    XCTAssertTrue(outgoingEvents.contains { event in
      if case .resolveInitialSynchronizationConflict(let item) = event, item == localItem1 {
        return true
      }
      return false
    }, "Expected to resolve initial synchronization conflict for localItem1")

    XCTAssertTrue(outgoingEvents.contains { event in
      if case .createLocalItem(let item) = event, item == cloudItem3 {
        return true
      }
      return false
    }, "Expected to create local item for cloudItem3")

    XCTAssertTrue(outgoingEvents.contains { event in
      if case .createCloudItem(let item) = event, item == localItem2 {
        return true
      }
      return false
    }, "Expected to create cloud item for localItem2")

    XCTAssertTrue(outgoingEvents.contains { event in
      if case .didFinishInitialSynchronization = event {
        return true
      }
      return false
    }, "Expected to finish initial synchronization")
  }

  func testInitialSynchronizationWithNewerCloudItem() {
    syncStateManager = iCloudSynchronizationStateResolver(isInitialSynchronization: true)

    let localItem = LocalMetadataItem.stub(fileName: "file", lastModificationDate: TimeInterval(1))
    let cloudItem = CloudMetadataItem.stub(fileName: "file", lastModificationDate: TimeInterval(2))

    outgoingEvents.append(contentsOf: syncStateManager.resolveEvent(.didFinishGatheringLocalContents([localItem])))
    outgoingEvents.append(contentsOf: syncStateManager.resolveEvent(.didFinishGatheringCloudContents([cloudItem])))

    XCTAssertTrue(outgoingEvents.contains { if case .resolveInitialSynchronizationConflict(_) = $0 { return true } else { return false } }, "Expected conflict resolution for a newer cloud item")
  }

  func testInitialSynchronizationWithNewerLocalItem() {
    syncStateManager = iCloudSynchronizationStateResolver(isInitialSynchronization: true)

    let localItem = LocalMetadataItem.stub(fileName: "file", lastModificationDate: TimeInterval(2))
    let cloudItem = CloudMetadataItem.stub(fileName: "file", lastModificationDate: TimeInterval(1))

    outgoingEvents.append(contentsOf: syncStateManager.resolveEvent(.didFinishGatheringLocalContents([localItem])))
    outgoingEvents.append(contentsOf: syncStateManager.resolveEvent(.didFinishGatheringCloudContents([cloudItem])))

    XCTAssertTrue(outgoingEvents.contains { if case .resolveInitialSynchronizationConflict(_) = $0 { return true } else { return false } }, "Expected conflict resolution for a newer local item")
  }

  func testInitialSynchronizationWithNonConflictingItems() {
    syncStateManager = iCloudSynchronizationStateResolver(isInitialSynchronization: true)

    let localItem = LocalMetadataItem.stub(fileName: "localFile", lastModificationDate: TimeInterval(1))
    let cloudItem = CloudMetadataItem.stub(fileName: "cloudFile", lastModificationDate: TimeInterval(2))

    outgoingEvents.append(contentsOf: syncStateManager.resolveEvent(.didFinishGatheringLocalContents([localItem])))
    outgoingEvents.append(contentsOf: syncStateManager.resolveEvent(.didFinishGatheringCloudContents([cloudItem])))

    XCTAssertTrue(outgoingEvents.contains { if case .createLocalItem(_) = $0 { return true } else { return false } }, "Expected creation of local item for cloudFile")
    XCTAssertTrue(outgoingEvents.contains { if case .createCloudItem(_) = $0 { return true } else { return false } }, "Expected creation of cloud item for localFile")
  }

  // MARK: - Test didFinishGathering without errors and after initial synchronization
  func testDidFinishGatheringWhenCloudAndLocalIsEmpty() {
    let localItems: LocalContents = []
    let cloudItems: CloudContents = []

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))
    XCTAssertEqual(outgoingEvents.count, 0)

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    XCTAssertEqual(outgoingEvents.count, 0)
  }

  func testDidFinishGatheringWhenOnlyCloudIsEmpty() {
    let localItem1 = LocalMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let localItem2 = LocalMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let localItem3 = LocalMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let localItems: LocalContents = LocalContents([localItem1, localItem2, localItem3])
    let cloudItems: CloudContents = []

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))

    XCTAssertEqual(outgoingEvents.count, 0)

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    outgoingEvents.forEach { event in
      switch event {
      case .createCloudItem(let item):
        XCTAssertTrue(localItems.containsByName(item))
      default:
        XCTFail()
      }
    }
  }

  func testDidFinishGatheringWhenOnlyLocalIsEmpty() {
    let cloudItem1 = CloudMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let cloudItem2 = CloudMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let cloudItem3 = CloudMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let localItems = LocalContents()
    let cloudItems = CloudContents([cloudItem1, cloudItem2, cloudItem3])

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))
    XCTAssertEqual(outgoingEvents.count, 0)

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    outgoingEvents.forEach { event in
      switch event {
      case .createLocalItem(let item):
        XCTAssertTrue(cloudItems.containsByName(item))
      default:
        XCTFail()
      }
    }
  }

  func testDidFinishGatheringWhenTreCloudIsEmpty() {
    let localItem1 = LocalMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let localItem2 = LocalMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let localItem3 = LocalMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let localItems = [localItem1, localItem2, localItem3]

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents([]))
    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))

    XCTAssertEqual(outgoingEvents.count, 3)
    outgoingEvents.forEach { event in
      switch event {
      case .createCloudItem(let item):
        XCTAssertTrue(localItems.containsByName(item))
      default:
        XCTFail()
      }
    }
  }

  func testDidFinishGatheringWhenLocalIsEmpty() {
    let cloudItem1 = CloudMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let cloudItem2 = CloudMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let cloudItem3 = CloudMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let localItems = LocalContents()
    let cloudItems = CloudContents([cloudItem1, cloudItem2, cloudItem3])

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))
    XCTAssertEqual(outgoingEvents.count, 0)

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    XCTAssertEqual(outgoingEvents.count, 3)
    outgoingEvents.forEach { event in
      switch event {
      case .createLocalItem(let item):
        XCTAssertTrue(cloudItems.containsByName(item))
      default:
        XCTFail()
      }
    }
  }

  func testDidFinishGatheringWhenLocalAndCloudAreNotEmptyAndAllFilesEqual() {
    let localItem1 = LocalMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let localItem2 = LocalMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let localItem3 = LocalMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let cloudItem1 = CloudMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let cloudItem2 = CloudMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let cloudItem3 = CloudMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let localItems = LocalContents([localItem1, localItem2, localItem3])
    let cloudItems = CloudContents([cloudItem1, cloudItem2, cloudItem3])

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    XCTAssertEqual(outgoingEvents.count, 0)

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))
    XCTAssertEqual(outgoingEvents.count, 0)
  }

  func testDidFinishGatheringWhenLocalAndCloudAreNotEmptyAndSomeLocalItemsAreNewer() {
    let localItem1 = LocalMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let localItem2 = LocalMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(3))
    let localItem3 = LocalMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(4))

    let cloudItem1 = CloudMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let cloudItem2 = CloudMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let cloudItem3 = CloudMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let localItems = LocalContents([localItem1, localItem2, localItem3])
    let cloudItems = CloudContents([cloudItem1, cloudItem2, cloudItem3])

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    XCTAssertEqual(outgoingEvents.count, 0)

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))
    XCTAssertEqual(outgoingEvents.count, 2)
    outgoingEvents.forEach { event in
      switch event {
      case .updateCloudItem(let item):
        XCTAssertTrue([localItem2, localItem3].containsByName(item))
      default:
        XCTFail()
      }
    }
  }

  func testDidFinishGatheringWhenLocalAndCloudAreNotEmptyAndSomeCloudItemsAreNewer() {
    let localItem1 = LocalMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let localItem2 = LocalMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let localItem3 = LocalMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let cloudItem1 = CloudMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(4))
    let cloudItem2 = CloudMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let cloudItem3 = CloudMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(7))

    let localItems = LocalContents([localItem1, localItem2, localItem3])
    let cloudItems = CloudContents([cloudItem1, cloudItem2, cloudItem3])

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    XCTAssertEqual(outgoingEvents.count, 0)

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))
    XCTAssertEqual(outgoingEvents.count, 2)
    outgoingEvents.forEach { event in
      switch event {
      case .updateLocalItem(let item):
        XCTAssertTrue([cloudItem1, cloudItem3].containsByName(item))
      default:
        XCTFail()
      }
    }
  }

  func testDidFinishGatheringWhenCloudFileNewerThanLocal() {
    let localItem = LocalMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))
    let cloudItem = CloudMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(8))

    let localItems = LocalContents([localItem])
    let cloudItems = CloudContents([cloudItem])

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    XCTAssertEqual(outgoingEvents.count, 0)

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))
    XCTAssertEqual(outgoingEvents.count, 1)
    outgoingEvents.forEach { event in
      switch event {
      case .updateLocalItem(let item):
        XCTAssertEqual(item, cloudItem)
      default:
        XCTFail()
      }
    }
  }

  func testDidFinishGatheringWhenCloudHaveSameFileBothInTrashedAndNotAndTrashedBotLocalIsNewer() {
    let localItem3 = LocalMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(9))

    let cloudItem3 = CloudMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(2))
    let cloudItem3Trashed = CloudMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(6))

    let localItems = LocalContents([localItem3])
    let cloudItems = CloudContents([cloudItem3, cloudItem3Trashed])

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    XCTAssertEqual(outgoingEvents.count, 0)

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))
    XCTAssertEqual(outgoingEvents.count, 1)
    outgoingEvents.forEach { event in
      switch event {
      case .updateCloudItem(let item):
        XCTAssertEqual(item, localItem3)
      default:
        XCTFail()
      }
    }
  }

  // MARK: - Test didUpdateLocalContents
  func testDidUpdateLocalContentsWhenContentWasNotChanged() {
    let localItem1 = LocalMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let localItem2 = LocalMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let localItem3 = LocalMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let localItems = LocalContents([localItem1, localItem2, localItem3])
    let cloudItems = CloudContents([])

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))

    XCTAssertEqual(outgoingEvents.count, 3)

    let newLocalItems = LocalContents([localItem1, localItem2, localItem3])
    let update = LocalContentsUpdate(added: [], updated: [], removed: [])
    outgoingEvents = syncStateManager.resolveEvent(.didUpdateLocalContents(contents: newLocalItems, update: update))
    XCTAssertEqual(outgoingEvents.count, 0)
  }

  func testDidUpdateLocalContentsWhenNewLocalItemWasAdded() {
    let localItem1 = LocalMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let localItem2 = LocalMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let localItem3 = LocalMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let cloudItem1 = CloudMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let cloudItem2 = CloudMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let cloudItem3 = CloudMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let localItems = LocalContents([localItem1, localItem2, localItem3])
    let cloudItems = CloudContents([cloudItem1, cloudItem2, cloudItem3])

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))

    XCTAssertEqual(outgoingEvents.count, 0)

    let localItem4 = LocalMetadataItem.stub(fileName: "file4", lastModificationDate: TimeInterval(4))
    let newLocalItems = LocalContents([localItem1, localItem2, localItem3])
    let update = LocalContentsUpdate(added: [localItem4], updated: [], removed: [])
    outgoingEvents = syncStateManager.resolveEvent(.didUpdateLocalContents(contents: newLocalItems, update: update))
    XCTAssertEqual(outgoingEvents.count, 1)

    outgoingEvents.forEach { event in
      switch event {
      case .createCloudItem(let item):
        XCTAssertEqual(item, localItem4)
      default:
        XCTFail()
      }
    }
  }

  func testDidUpdateLocalContentsWhenLocalItemWasUpdated() {
    let localItem1 = LocalMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let localItem2 = LocalMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let localItem3 = LocalMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let cloudItem1 = CloudMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let cloudItem2 = CloudMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let cloudItem3 = CloudMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let localItems = LocalContents([localItem1, localItem2, localItem3])
    let cloudItems = CloudContents([cloudItem1, cloudItem2, cloudItem3])

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))

    XCTAssertEqual(outgoingEvents.count, 0)

    let localItem2Updated = LocalMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(3))
    let localItem3Updated = LocalMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(4))

    let newLocalItems = LocalContents([localItem1, localItem2Updated, localItem3Updated])
    let update = LocalContentsUpdate(added: [], updated: [localItem2Updated, localItem3Updated], removed: [])
    outgoingEvents = syncStateManager.resolveEvent(.didUpdateLocalContents(contents: newLocalItems, update: update))
    XCTAssertEqual(outgoingEvents.count, 2)

    outgoingEvents.forEach { event in
      switch event {
      case .updateCloudItem(let item):
        XCTAssertTrue([localItem2Updated, localItem3Updated].containsByName(item))
      default:
        XCTFail()
      }
    }
  }

  func testDidUpdateLocalContentsWhenLocalItemWasRemoved() {
    let localItem1 = LocalMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let localItem2 = LocalMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let localItem3 = LocalMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let cloudItem1 = CloudMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let cloudItem2 = CloudMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let cloudItem3 = CloudMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let localItems = LocalContents([localItem1, localItem2, localItem3])
    let cloudItems = CloudContents([cloudItem1, cloudItem2, cloudItem3])

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))

    XCTAssertEqual(outgoingEvents.count, 0)

    let newLocalItems = LocalContents([localItem1, localItem2])
    let update = LocalContentsUpdate(added: [], updated: [], removed: [localItem3])
    outgoingEvents = syncStateManager.resolveEvent(.didUpdateLocalContents(contents: newLocalItems, update: update))
    XCTAssertEqual(outgoingEvents.count, 1)

    outgoingEvents.forEach { event in
      switch event {
      case .removeCloudItem(let item):
        XCTAssertEqual(item, cloudItem3)
      default:
        XCTFail()
      }
    }
  }

  // TODO: Test didUpdateCloudContents
  func testDidUpdateCloudContentsWhenContentWasNotChanged() {
    let localItem1 = LocalMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let localItem2 = LocalMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let localItem3 = LocalMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let cloudItem1 = CloudMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let cloudItem2 = CloudMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let cloudItem3 = CloudMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let localItems = LocalContents([localItem1, localItem2, localItem3])
    let cloudItems = CloudContents([cloudItem1, cloudItem2, cloudItem3])

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))

    XCTAssertEqual(outgoingEvents.count, 0)

    let newCloudItems = CloudContents([cloudItem1, cloudItem2, cloudItem3])
    let update = CloudContentsUpdate(added: [], updated: [], removed: [])
    outgoingEvents = syncStateManager.resolveEvent(.didUpdateCloudContents(contents: newCloudItems, update: update))
    XCTAssertEqual(outgoingEvents.count, 0)
  }

  func testDidUpdateCloudContentsWhenContentItemWasAdded() {
    let localItem1 = LocalMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let localItem2 = LocalMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let localItem3 = LocalMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let cloudItem1 = CloudMetadataItem.stub(fileName: "file1", lastModificationDate: TimeInterval(1))
    let cloudItem2 = CloudMetadataItem.stub(fileName: "file2", lastModificationDate: TimeInterval(2))
    let cloudItem3 = CloudMetadataItem.stub(fileName: "file3", lastModificationDate: TimeInterval(3))

    let localItems = LocalContents([localItem1, localItem2, localItem3])
    var cloudItems = CloudContents([cloudItem1, cloudItem2, cloudItem3])

    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringLocalContents(localItems))
    outgoingEvents = syncStateManager.resolveEvent(.didFinishGatheringCloudContents(cloudItems))

    XCTAssertEqual(outgoingEvents.count, 0)

    var cloudItem4 = CloudMetadataItem.stub(fileName: "file4", lastModificationDate: TimeInterval(3), isDownloaded: false)
    cloudItems.append(cloudItem4)
    var update = CloudContentsUpdate(added: [cloudItem4], updated: [], removed: [])
    outgoingEvents = syncStateManager.resolveEvent(.didUpdateCloudContents(contents: cloudItems, update: update))
    XCTAssertEqual(outgoingEvents.count, 1)
    outgoingEvents.forEach { event in
      switch event {
      case .startDownloading(let cloudMetadataItem):
        XCTAssertEqual(cloudMetadataItem, cloudItem4)
      default:
        XCTFail()
      }
    }

    cloudItem4.isDownloaded = true

    // recreate collection
    cloudItems = [cloudItem1, cloudItem2, cloudItem3, cloudItem4]
    update = CloudContentsUpdate(added: [], updated: [cloudItem4], removed: [])
    outgoingEvents = syncStateManager.resolveEvent(.didUpdateCloudContents(contents: cloudItems, update: update))

    XCTAssertEqual(outgoingEvents.count, 1)
    outgoingEvents.forEach { event in
      switch event {
      case .createLocalItem(let cloudMetadataItem):
        XCTAssertEqual(cloudMetadataItem, cloudItem4)
      default:
        XCTFail()
      }
    }
  }
}

