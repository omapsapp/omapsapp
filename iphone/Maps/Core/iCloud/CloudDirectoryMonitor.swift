protocol CloudDirectoryMonitor: DirectoryMonitor {
  var delegate: CloudDirectoryMonitorDelegate? { get set }

  func fetchUbiquityDirectoryUrl(completion: ((Result<URL, Error>) -> Void)?)
  func isCloudAvailable() -> Bool
}

protocol CloudDirectoryMonitorDelegate : AnyObject {
  func didFinishGathering(_ contents: CloudContents)
  func didUpdate(_ contents: CloudContents, _ update: CloudContentsUpdate)
  func didReceiveCloudMonitorError(_ error: Error)
}

private let kUDCloudIdentityKey = "com.apple.organicmaps.UbiquityIdentityToken"
private let kDocumentsDirectoryName = "Documents"

final class iCloudDocumentsMonitor: NSObject, CloudDirectoryMonitor {

  private static let sharedContainerIdentifier: String = {
    var identifier = "iCloud.app.organicmaps"
    #if DEBUG
    identifier.append(".debug")
    #endif
    return identifier
  }()

  let containerIdentifier: String
  private let fileManager: FileManager
  private let fileType: FileType // TODO: Should be removed when the nested directory support will be implemented
  private var metadataQuery: NSMetadataQuery?
  private var ubiquitousDocumentsDirectory: URL?
  private var previouslyChangedContents = CloudContentsUpdate()

  // MARK: - Public properties
  private(set) var state: DirectoryMonitorState = .stopped
  weak var delegate: CloudDirectoryMonitorDelegate?

  init(fileManager: FileManager = .default, cloudContainerIdentifier: String = iCloudDocumentsMonitor.sharedContainerIdentifier, fileType: FileType) {
    self.fileManager = fileManager
    self.containerIdentifier = cloudContainerIdentifier
    self.fileType = fileType
    super.init()

    fetchUbiquityDirectoryUrl()
    subscribeOnMetadataQueryNotifications()
  }

  // MARK: - Public methods
  func start(completion: ((Result<URL, Error>) -> Void)? = nil) {
    guard isCloudAvailable() else {
      completion?(.failure(SynchronizationError.iCloudIsNotAvailable))
      return
    }
    fetchUbiquityDirectoryUrl { [weak self] result in
      guard let self else { return }
      switch result {
      case .failure(let error):
        completion?(.failure(error))
      case .success(let url):
        LOG(.debug, "Start cloud monitor.")
        self.startQuery()
        self.state = .started
        completion?(.success(url))
      }
    }
  }

  func stop() {
    guard state != .stopped else { return }
    LOG(.debug, "Stop cloud monitor.")
    stopQuery()
    state = .stopped
    previouslyChangedContents = CloudContentsUpdate()
  }

  func resume() {
    guard state != .started else { return }
    LOG(.debug, "Resume cloud monitor.")
    metadataQuery?.enableUpdates()
    state = .started
  }

  func pause() {
    guard state != .paused else { return }
    LOG(.debug, "Pause cloud monitor.")
    metadataQuery?.disableUpdates()
    state = .paused
  }

  func fetchUbiquityDirectoryUrl(completion: ((Result<URL, Error>) -> Void)? = nil) {
    if let ubiquitousDocumentsDirectory {
      completion?(.success(ubiquitousDocumentsDirectory))
      return
    }
    DispatchQueue.global().async {
      guard let containerUrl = self.fileManager.url(forUbiquityContainerIdentifier: self.containerIdentifier) else {
        LOG(.warning, "Failed to retrieve container's URL for:\(self.containerIdentifier)")
        completion?(.failure(SynchronizationError.containerNotFound))
        return
      }
      let documentsContainerUrl = containerUrl.appendingPathComponent(kDocumentsDirectoryName)
      if !self.fileManager.fileExists(atPath: documentsContainerUrl.path) {
        LOG(.debug, "Creating directory at path: \(documentsContainerUrl.path)...")
        do {
          try self.fileManager.createDirectory(at: documentsContainerUrl, withIntermediateDirectories: true)
        } catch {
          completion?(.failure(SynchronizationError.containerNotFound))
        }
      }
      LOG(.debug, "Ubiquity directory URL: \(documentsContainerUrl)")
      self.ubiquitousDocumentsDirectory = documentsContainerUrl
      completion?(.success(documentsContainerUrl))
    }
  }

  func isCloudAvailable() -> Bool {
    let cloudToken = fileManager.ubiquityIdentityToken
    guard let cloudToken else {
      UserDefaults.standard.removeObject(forKey: kUDCloudIdentityKey)
      LOG(.warning, "Cloud is not available. Cloud token is nil.")
      return false
    }
    do {
      let data = try NSKeyedArchiver.archivedData(withRootObject: cloudToken, requiringSecureCoding: true)
      UserDefaults.standard.set(data, forKey: kUDCloudIdentityKey)
      return true
    } catch {
      UserDefaults.standard.removeObject(forKey: kUDCloudIdentityKey)
      LOG(.warning, "Failed to archive cloud token: \(error)")
      return false
    }
  }
}

// MARK: - Private
private extension iCloudDocumentsMonitor {
  // MARK: - MetadataQuery
  func subscribeOnMetadataQueryNotifications() {
    NotificationCenter.default.addObserver(self, selector: #selector(queryDidFinishGathering(_:)), name: NSNotification.Name.NSMetadataQueryDidFinishGathering, object: nil)
    NotificationCenter.default.addObserver(self, selector: #selector(queryDidUpdate(_:)), name: NSNotification.Name.NSMetadataQueryDidUpdate, object: nil)
  }

  func startQuery() {
    metadataQuery = Self.buildMetadataQuery(for: fileType)
    guard let metadataQuery, !metadataQuery.isStarted else { return }
    LOG(.debug, "Start metadata query")
    metadataQuery.start()
  }

  func stopQuery() {
    LOG(.debug, "Stop metadata query")
    metadataQuery?.stop()
    metadataQuery = nil
  }

  @objc func queryDidFinishGathering(_ notification: Notification) {
    guard isCloudAvailable() else { return }
    metadataQuery?.disableUpdates()
    LOG(.debug, "Query did finish gathering")
    do {
      let currentContents = try Self.getCurrentContents(notification)
      LOG(.info, "Cloud contents (\(currentContents.count)):")
      currentContents.forEach { LOG(.info, $0.shortDebugDescription) }
      delegate?.didFinishGathering(currentContents)
    } catch {
      delegate?.didReceiveCloudMonitorError(error)
    }
    metadataQuery?.enableUpdates()
  }

  @objc func queryDidUpdate(_ notification: Notification) {
    guard isCloudAvailable() else { return }
    metadataQuery?.disableUpdates()
    LOG(.debug, "Query did update")
    do {
      let changedContents = try Self.getChangedContents(notification)
      /* The metadataQuery can send the same changes multiple times with only uploading/downloading process updates.
      This unnecessary updated should be skipped. */
      if changedContents != previouslyChangedContents {
        previouslyChangedContents = changedContents
        let currentContents = try Self.getCurrentContents(notification)
        LOG(.info, "Cloud contents (\(currentContents.count)):")
        currentContents.forEach { LOG(.info, $0.shortDebugDescription) }
        LOG(.info, "Added to the cloud content (\(changedContents.added.count)): \n\(changedContents.added.shortDebugDescription)")
        LOG(.info, "Updated in the cloud content (\(changedContents.updated.count)): \n\(changedContents.updated.shortDebugDescription)")
        LOG(.info, "Removed from the cloud content (\(changedContents.removed.count)): \n\(changedContents.removed.shortDebugDescription)")
        delegate?.didUpdate(currentContents, changedContents)
      }
    } catch {
      delegate?.didReceiveCloudMonitorError(error)
    }
    metadataQuery?.enableUpdates()
  }

  static func buildMetadataQuery(for fileType: FileType) -> NSMetadataQuery {
    let metadataQuery = NSMetadataQuery()
    metadataQuery.notificationBatchingInterval = 1
    metadataQuery.searchScopes = [NSMetadataQueryUbiquitousDocumentsScope]
    metadataQuery.predicate = NSPredicate(format: "%K LIKE %@", NSMetadataItemFSNameKey, "*.\(fileType.fileExtension)")
    metadataQuery.sortDescriptors = [NSSortDescriptor(key: NSMetadataItemFSNameKey, ascending: true)]
    return metadataQuery
  }

  static func getCurrentContents(_ notification: Notification) throws -> [CloudMetadataItem] {
    guard let metadataQuery = notification.object as? NSMetadataQuery,
          let metadataItems = metadataQuery.results as? [NSMetadataItem] else {
      throw SynchronizationError.failedToRetrieveMetadataQueryContent
    }
    return try metadataItems.map { try CloudMetadataItem(metadataItem: $0) }
  }

  static func getChangedContents(_ notification: Notification) throws -> CloudContentsUpdate {
    guard let userInfo = notification.userInfo else {
      throw SynchronizationError.failedToRetrieveMetadataQueryContent
    }
    let addedMetadataItems = userInfo[NSMetadataQueryUpdateAddedItemsKey] as? [NSMetadataItem] ?? []
    let updatedMetadataItems = userInfo[NSMetadataQueryUpdateChangedItemsKey] as? [NSMetadataItem] ?? []
    let removedMetadataItems = userInfo[NSMetadataQueryUpdateRemovedItemsKey] as? [NSMetadataItem] ?? []
    let addedContents = try addedMetadataItems.map { try CloudMetadataItem(metadataItem: $0) }
    let updatedContents = try updatedMetadataItems.map { try CloudMetadataItem(metadataItem: $0) }
    let removedContents = try removedMetadataItems.map { try CloudMetadataItem(metadataItem: $0) }
    return CloudContentsUpdate(added: addedContents, updated: updatedContents, removed: removedContents)
  }
}

private extension CloudContentsUpdate {
  init() {
    self.added = []
    self.updated = []
    self.removed = []
  }
}
