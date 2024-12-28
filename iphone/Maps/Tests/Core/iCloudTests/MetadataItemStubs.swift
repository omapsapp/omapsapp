@testable import Organic_Maps__Debug_

extension LocalMetadataItem {
  static func stub(fileName: String, 
                   lastModificationDate: TimeInterval) -> LocalMetadataItem {
    let item = LocalMetadataItem(fileName: fileName,
                                 fileUrl: URL(string: "url")!,
                                 lastModificationDate: lastModificationDate)
    return item

  }
}

extension CloudMetadataItem {
  static func stub(fileName: String, 
                   lastModificationDate: TimeInterval,
                   isDownloaded: Bool = true,
                   hasUnresolvedConflicts: Bool = false) -> CloudMetadataItem {
    let item = CloudMetadataItem(fileName: fileName,
                                 fileUrl: URL(string: "url")!,
                                 isDownloaded: isDownloaded,
                                 lastModificationDate: lastModificationDate,
                                 downloadingError: nil,
                                 uploadingError: nil,
                                 hasUnresolvedConflicts: hasUnresolvedConflicts)
    return item
  }
}
