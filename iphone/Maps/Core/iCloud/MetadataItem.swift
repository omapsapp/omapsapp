protocol MetadataItem: Equatable, Hashable {
  var fileName: String { get }
  var fileUrl: URL { get }
  var lastModificationDate: TimeInterval { get }
}

struct LocalMetadataItem: MetadataItem {
  let fileName: String
  let fileUrl: URL
  let lastModificationDate: TimeInterval
}

struct CloudMetadataItem: MetadataItem {
  let fileName: String
  let fileUrl: URL
  var isDownloaded: Bool
  var lastModificationDate: TimeInterval
  let downloadingError: NSError?
  let uploadingError: NSError?
  let hasUnresolvedConflicts: Bool
}

extension LocalMetadataItem {
  init(fileUrl: URL) throws {
    let resources = try fileUrl.resourceValues(forKeys: [.contentModificationDateKey])
    guard let lastModificationDate = resources.contentModificationDate?.roundedTime else {
      LOG(.error, "Failed to initialize LocalMetadataItem from URL's resources: \(resources)")
      throw SynchronizationError.failedToCreateMetadataItem
    }
    self.fileName = fileUrl.lastPathComponent
    self.fileUrl = fileUrl.standardizedFileURL
    self.lastModificationDate = lastModificationDate
  }

  func fileData() throws -> Data {
    try Data(contentsOf: fileUrl)
  }
}

extension CloudMetadataItem {
  init(metadataItem: NSMetadataItem) throws {
    guard let fileName = metadataItem.value(forAttribute: NSMetadataItemFSNameKey) as? String,
          let fileUrl = metadataItem.value(forAttribute: NSMetadataItemURLKey) as? URL,
          let downloadStatus = metadataItem.value(forAttribute: NSMetadataUbiquitousItemDownloadingStatusKey) as? String,
          let lastModificationDate = (metadataItem.value(forAttribute: NSMetadataItemFSContentChangeDateKey) as? Date)?.roundedTime,
          let hasUnresolvedConflicts = metadataItem.value(forAttribute: NSMetadataUbiquitousItemHasUnresolvedConflictsKey) as? Bool else {
      let allAttributes = metadataItem.values(forAttributes: metadataItem.attributes)
      LOG(.error, "Failed to initialize CloudMetadataItem from NSMetadataItem: \(allAttributes.debugDescription)")
      throw SynchronizationError.failedToCreateMetadataItem
    }
    self.fileName = fileName
    self.fileUrl = fileUrl.standardizedFileURL
    self.isDownloaded = downloadStatus == NSMetadataUbiquitousItemDownloadingStatusCurrent
    self.lastModificationDate = lastModificationDate
    self.hasUnresolvedConflicts = hasUnresolvedConflicts
    self.downloadingError = metadataItem.value(forAttribute: NSMetadataUbiquitousItemDownloadingErrorKey) as? NSError
    self.uploadingError = metadataItem.value(forAttribute: NSMetadataUbiquitousItemUploadingErrorKey) as? NSError
  }

  init(fileUrl: URL) throws {
    let resources = try fileUrl.resourceValues(forKeys: [.nameKey,
                                                         .contentModificationDateKey,
                                                         .ubiquitousItemDownloadingStatusKey,
                                                         .ubiquitousItemHasUnresolvedConflictsKey,
                                                         .ubiquitousItemDownloadingErrorKey,
                                                         .ubiquitousItemUploadingErrorKey])
    guard let downloadStatus = resources.ubiquitousItemDownloadingStatus,
          let lastModificationDate = resources.contentModificationDate?.roundedTime,
          let hasUnresolvedConflicts = resources.ubiquitousItemHasUnresolvedConflicts else {
      LOG(.error, "Failed to initialize CloudMetadataItem from \(fileUrl) resources: \(resources.allValues)")
      throw SynchronizationError.failedToCreateMetadataItem
    }
    self.fileName = fileUrl.lastPathComponent
    self.fileUrl = fileUrl.standardizedFileURL
    self.isDownloaded = downloadStatus.rawValue == NSMetadataUbiquitousItemDownloadingStatusCurrent
    self.lastModificationDate = lastModificationDate
    self.hasUnresolvedConflicts = hasUnresolvedConflicts
    self.downloadingError = resources.ubiquitousItemDownloadingError
    self.uploadingError = resources.ubiquitousItemUploadingError
  }

  func relatedLocalItemUrl(to localContainer: URL) -> URL {
    localContainer.appendingPathComponent(fileName)
  }
}

extension MetadataItem {
  var shortDebugDescription: String {
    "fileName: \(fileName), lastModified: \(lastModificationDate)"
  }
}

extension LocalMetadataItem {
  func relatedCloudItemUrl(to cloudContainer: URL) -> URL {
    cloudContainer.appendingPathComponent(fileName)
  }
}

extension Array where Element: MetadataItem {
  func containsByName(_ item: any MetadataItem) -> Bool {
    return contains(where: { $0.fileName == item.fileName })
  }
  func firstByName(_ item: any MetadataItem) -> Element? {
    return first(where: { $0.fileName == item.fileName })
  }

  var shortDebugDescription: String {
    map { $0.shortDebugDescription }.joined(separator: "\n")
  }
}

extension Array where Element == CloudMetadataItem {
  var downloaded: Self {
    filter { $0.isDownloaded }
  }

  var notDownloaded: Self {
    filter { !$0.isDownloaded }
  }

  func withUnresolvedConflicts(_ hasUnresolvedConflicts: Bool) -> Self {
    filter { $0.hasUnresolvedConflicts == hasUnresolvedConflicts }
  }
}

fileprivate extension Date {
  var roundedTime: TimeInterval {
    timeIntervalSince1970.rounded(.down)
  }
}
