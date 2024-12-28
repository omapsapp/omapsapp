typealias LocalContents = [LocalMetadataItem]
typealias CloudContents = [CloudMetadataItem]
typealias LocalContentsUpdate = ContentsUpdate<LocalMetadataItem>
typealias CloudContentsUpdate = ContentsUpdate<CloudMetadataItem>

struct ContentsUpdate<T: MetadataItem>: Equatable {
  let added: [T]
  let updated: [T]
  let removed: [T]
}

protocol SynchronizationStateResolver {
  func setInitialSynchronization(_ isInitialSynchronization: Bool)
  func resolveEvent(_ event: IncomingSynchronizationEvent) -> [OutgoingSynchronizationEvent]
  func resetState()
}

enum IncomingSynchronizationEvent {
  case didFinishGatheringLocalContents(LocalContents)
  case didFinishGatheringCloudContents(CloudContents)
  case didUpdateLocalContents(contents: LocalContents, update: LocalContentsUpdate)
  case didUpdateCloudContents(contents: CloudContents, update: CloudContentsUpdate)
}

enum OutgoingSynchronizationEvent {
  case startDownloading(CloudMetadataItem)

  case createLocalItem(with: CloudMetadataItem)
  case updateLocalItem(with: CloudMetadataItem)
  case removeLocalItem(LocalMetadataItem)

  case createCloudItem(with: LocalMetadataItem)
  case updateCloudItem(with: LocalMetadataItem)
  case removeCloudItem(CloudMetadataItem)

  case didReceiveError(SynchronizationError)
  case resolveVersionsConflict(CloudMetadataItem)
  case resolveInitialSynchronizationConflict(LocalMetadataItem)
  case didFinishInitialSynchronization
}

final class iCloudSynchronizationStateResolver: SynchronizationStateResolver {

  // MARK: - Public properties
  private var localContentsGatheringIsFinished = false
  private var cloudContentGatheringIsFinished = false
  private var currentLocalContents: LocalContents = []
  private var currentCloudContents: CloudContents = []
  private var isInitialSynchronization: Bool

  init(isInitialSynchronization: Bool) {
    self.isInitialSynchronization = isInitialSynchronization
  }

  // MARK: - Public methods
  @discardableResult
  func resolveEvent(_ event: IncomingSynchronizationEvent) -> [OutgoingSynchronizationEvent] {
    let outgoingEvents: [OutgoingSynchronizationEvent]
    switch event {
    case .didFinishGatheringLocalContents(let contents):
      localContentsGatheringIsFinished = true
      outgoingEvents = resolveDidFinishGathering(localContents: contents, cloudContents: currentCloudContents)
    case .didFinishGatheringCloudContents(let contents):
      cloudContentGatheringIsFinished = true
      outgoingEvents = resolveDidFinishGathering(localContents: currentLocalContents, cloudContents: contents)
    case .didUpdateLocalContents(let contents, let update):
      currentLocalContents = contents
      outgoingEvents = resolveDidUpdateLocalContents(update)
    case .didUpdateCloudContents(let contents, let update):
      currentCloudContents = contents
      outgoingEvents = resolveDidUpdateCloudContents(update)
    }

    LOG(.info, "Events to process (\(outgoingEvents.count)):")
    outgoingEvents.forEach { LOG(.info, $0) }

    return outgoingEvents
  }

  func resetState() {
    LOG(.debug, "Resetting state")
    currentLocalContents.removeAll()
    currentCloudContents.removeAll()
    localContentsGatheringIsFinished = false
    cloudContentGatheringIsFinished = false
  }

  func setInitialSynchronization(_ isInitialSynchronization: Bool) {
    self.isInitialSynchronization = isInitialSynchronization
  }

  private func resolveDidFinishGathering(localContents: LocalContents, cloudContents: CloudContents) -> [OutgoingSynchronizationEvent] {
    currentLocalContents = localContents
    currentCloudContents = cloudContents
    guard localContentsGatheringIsFinished, cloudContentGatheringIsFinished else { return [] }

    switch (localContents.isEmpty, cloudContents.isEmpty) {
    case (true, true):
      return []
    case (true, false):
      return cloudContents.map { .createLocalItem(with: $0) }
    case (false, true):
      return localContents.map { .createCloudItem(with: $0) }
    case (false, false):
      var events = [OutgoingSynchronizationEvent]()
      if isInitialSynchronization {
        /* During the initial synchronization:
         - all conflicted local and cloud items will be saved to avoid a data loss
         - all items that are in the cloud but not in the local container will be created in the local container
         - all items that are in the local container but not in the cloud container will be created in the cloud container
         */
        localContents.forEach { localItem in
          if let cloudItem = cloudContents.firstByName(localItem), localItem.lastModificationDate != cloudItem.lastModificationDate {
            events.append(.resolveInitialSynchronizationConflict(localItem))
            events.append(.updateLocalItem(with: cloudItem))
          }
        }

        let itemsToCreateInCloudContainer = localContents.filter { !cloudContents.containsByName($0) }
        let itemsToCreateInLocalContainer = cloudContents.filter { !localContents.containsByName($0) }
        events.append(contentsOf: itemsToCreateInCloudContainer.map { .createCloudItem(with: $0) })
        events.append(contentsOf: itemsToCreateInLocalContainer.map { .createLocalItem(with: $0) })

        events.append(.didFinishInitialSynchronization)
        isInitialSynchronization = false
      } else {
        cloudContents.getErrors.forEach { events.append(.didReceiveError($0)) }
        cloudContents.withUnresolvedConflicts.forEach { events.append(.resolveVersionsConflict($0)) }

        /* During the non-initial synchronization:
         - the iCloud container is considered as the source of truth and all items that are changed
         in the cloud container will be updated in the local container
         - itemsToCreateInCloudContainer is not present here because the new files cannot be added locally
         when the app is closed and only the cloud contents can be changed (added/removed) between app launches
         */
        let itemsToRemoveFromLocalContainer = localContents.filter { !cloudContents.containsByName($0) }
        let itemsToCreateInLocalContainer = cloudContents.filter { !localContents.containsByName($0) }
        let itemsToUpdateInLocalContainer = cloudContents.filter { cloudItem in
          guard let localItem = localContents.firstByName(cloudItem) else { return false }
          return cloudItem.lastModificationDate > localItem.lastModificationDate
        }
        let itemsToUpdateInCloudContainer = localContents.filter { localItem in
          guard let cloudItem = cloudContents.firstByName(localItem) else { return false }
          return localItem.lastModificationDate > cloudItem.lastModificationDate
        }

        itemsToCreateInLocalContainer.notDownloaded.forEach { events.append(.startDownloading($0)) }
        itemsToUpdateInLocalContainer.notDownloaded.forEach { events.append(.startDownloading($0)) }

        itemsToRemoveFromLocalContainer.forEach { events.append(.removeLocalItem($0)) }
        itemsToCreateInLocalContainer.downloaded.forEach { events.append(.createLocalItem(with: $0)) }
        itemsToUpdateInLocalContainer.downloaded.forEach { events.append(.updateLocalItem(with: $0)) }
        itemsToUpdateInCloudContainer.forEach { events.append(.updateCloudItem(with: $0)) }
      }
      return events
    }
  }

  private func resolveDidUpdateLocalContents(_ localContents: LocalContentsUpdate) -> [OutgoingSynchronizationEvent] {
    var outgoingEvents = [OutgoingSynchronizationEvent]()
    localContents.removed.forEach { localItem in
      guard let cloudItem = self.currentCloudContents.firstByName(localItem) else { return }
      outgoingEvents.append(.removeCloudItem(cloudItem))
    }
    localContents.added.forEach { localItem in
      guard !self.currentCloudContents.containsByName(localItem) else { return }
      outgoingEvents.append(.createCloudItem(with: localItem))
    }
    localContents.updated.forEach { localItem in
      guard let cloudItem = self.currentCloudContents.firstByName(localItem) else {
        outgoingEvents.append(.createCloudItem(with: localItem))
        return
      }
      guard localItem.lastModificationDate > cloudItem.lastModificationDate else { return }
      outgoingEvents.append(.updateCloudItem(with: localItem))
    }
    return outgoingEvents
  }

  private func resolveDidUpdateCloudContents(_ cloudContents: CloudContentsUpdate) -> [OutgoingSynchronizationEvent] {
    var outgoingEvents = [OutgoingSynchronizationEvent]()
    currentCloudContents.getErrors.forEach { outgoingEvents.append(.didReceiveError($0)) }
    currentCloudContents.withUnresolvedConflicts.forEach { outgoingEvents.append(.resolveVersionsConflict($0)) }

    cloudContents.added.notDownloaded.forEach { outgoingEvents.append(.startDownloading($0)) }
    cloudContents.updated.notDownloaded.forEach { outgoingEvents.append(.startDownloading($0)) }

    cloudContents.removed.forEach { cloudItem in
      guard let localItem = self.currentLocalContents.firstByName(cloudItem) else { return }
      outgoingEvents.append(.removeLocalItem(localItem))
    }
    cloudContents.added.downloaded.forEach { cloudItem in
      guard !self.currentLocalContents.containsByName(cloudItem) else { return }
      outgoingEvents.append(.createLocalItem(with: cloudItem))
    }
    cloudContents.updated.downloaded.forEach { cloudItem in
      guard let localItem = self.currentLocalContents.firstByName(cloudItem) else {
        outgoingEvents.append(.createLocalItem(with: cloudItem))
        return
      }
      guard cloudItem.lastModificationDate > localItem.lastModificationDate else { return }
      outgoingEvents.append(.updateLocalItem(with: cloudItem))
    }
    return outgoingEvents
  }
}

private extension CloudContents {
  var withUnresolvedConflicts: CloudContents {
    filter { $0.hasUnresolvedConflicts }
  }

  var getErrors: [SynchronizationError] {
     reduce(into: [SynchronizationError](), { partialResult, cloudItem in
       if let downloadingError = cloudItem.downloadingError, let synchronizationError = downloadingError.ubiquitousError {
        partialResult.append(synchronizationError)
      }
       if let uploadingError = cloudItem.uploadingError, let synchronizationError = uploadingError.ubiquitousError {
        partialResult.append(synchronizationError)
      }
    })
  }
}
