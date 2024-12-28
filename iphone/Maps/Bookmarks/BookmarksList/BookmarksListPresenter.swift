protocol BookmarksListDelegate: AnyObject {
  func bookmarksListDidDeleteGroup()
}

final class BookmarksListPresenter {
  private unowned let view: IBookmarksListView
  private let router: IBookmarksListRouter
  private var interactor: IBookmarksListInteractor
  private weak var delegate: BookmarksListDelegate?
  private var bookmarkGroup: BookmarkGroup

  private enum EditableItem {
    case bookmark(MWMMarkID)
    case track(MWMTrackID)
  }
  private var editingItem: EditableItem?

  init(view: IBookmarksListView,
       router: IBookmarksListRouter,
       delegate: BookmarksListDelegate?,
       interactor: IBookmarksListInteractor) {
    self.view = view
    self.router = router
    self.delegate = delegate
    self.interactor = interactor
    self.bookmarkGroup = interactor.getBookmarkGroup()
    self.subscribeOnGroupReloading()
  }

  private func subscribeOnGroupReloading() {
    interactor.onCategoryReload = { [weak self] result in
      guard let self else { return }
      switch result {
      case .notFound:
        self.router.goBack()
      case .success:
        self.bookmarkGroup = self.interactor.getBookmarkGroup()
        self.reload()
      }
    }
  }

  private func reload() {
    guard let sortingType = interactor.lastSortingType() else {
      setDefaultSections()
      return
    }
    sort(sortingType)
  }

  private func setDefaultSections() {
    interactor.resetSort()
    var sections: [IBookmarksListSectionViewModel] = []
    let tracks = bookmarkGroup.tracks.map { track in
      TrackViewModel(track, formattedDistance: formatDistance(Double(track.trackLengthMeters)), colorDidTap: {
        self.view.showColorPicker(with: .defaultColorPicker(track.trackColor)) { color in
          BookmarksManager.shared().updateTrack(track.trackId, setColor: color)
          self.reload()
        }
      })
    }
    if !tracks.isEmpty {
      sections.append(TracksSectionViewModel(tracks: tracks))
    }

    let collections = bookmarkGroup.collections.map { SubgroupViewModel($0) }
    if !collections.isEmpty {
      sections.append(SubgroupsSectionViewModel(title: L("collections"), subgroups: collections, type: .collection))
    }

    let categories = bookmarkGroup.categories.map { SubgroupViewModel($0)}
    if !categories.isEmpty {
      sections.append(SubgroupsSectionViewModel(title: L("categories"), subgroups: categories, type: .category))
    }

    let bookmarks = mapBookmarks(bookmarkGroup.bookmarks)
    if !bookmarks.isEmpty {
      sections.append(BookmarksSectionViewModel(title: L("bookmarks"), bookmarks: bookmarks))
    }
    view.setSections(sections)
  }

  private func mapBookmarks(_ bookmarks: [Bookmark]) -> [BookmarkViewModel] {
    let location = LocationManager.lastLocation()
    return bookmarks.map { bookmark in
      let formattedDistance: String?
      if let location = location {
        let distance = location.distance(from: CLLocation(latitude: bookmark.locationCoordinate.latitude,
                                                          longitude: bookmark.locationCoordinate.longitude))
        formattedDistance = formatDistance(distance)
      } else {
        formattedDistance = nil
      }
      return BookmarkViewModel(bookmark, formattedDistance: formattedDistance, colorDidTap: { [weak self] in
        self?.view.showColorPicker(with: .bookmarkColorPicker(bookmark.bookmarkColor)) { color in
          guard let bookmarkColor = BookmarkColor.bookmarkColor(from: color) else { return }
          BookmarksManager.shared().updateBookmark(bookmark.bookmarkId, setColor: bookmarkColor)
          self?.reload()
        }
      })
    }
  }

  private func formatDistance(_ distance: Double) -> String {
    DistanceFormatter.distanceString(fromMeters: distance)
  }

  private func showSortMenu() {
    var sortItems = interactor.availableSortingTypes(hasMyPosition: LocationManager.lastLocation() != nil)
      .map { sortingType -> BookmarksListMenuItem in
        switch sortingType {
        case .distance:
          return BookmarksListMenuItem(title: L("sort_distance"), action: { [weak self] in
            self?.sort(.distance)
          })
        case .date:
          return BookmarksListMenuItem(title: L("sort_date"), action: { [weak self] in
            self?.sort(.date)
          })
        case .type:
          return BookmarksListMenuItem(title: L("sort_type"), action: { [weak self] in
            self?.sort(.type)
          })
        case .name:
          return BookmarksListMenuItem(title: L("sort_name"), action: { [weak self] in
            self?.sort(.name)
          })
        }
    }
    sortItems.append(BookmarksListMenuItem(title: L("sort_default"), action: { [weak self] in
      self?.setDefaultSections()
    }))
    view.showMenu(sortItems, from: .sort)
  }

  private func showMoreMenu() {
    var moreItems: [BookmarksListMenuItem] = []
    moreItems.append(BookmarksListMenuItem(title: L("search_show_on_map"), action: { [weak self] in
      self?.viewOnMap()
    }))
    moreItems.append(BookmarksListMenuItem(title: L("edit"), action: { [weak self] in
      guard let self = self else { return }
      self.router.listSettings(self.bookmarkGroup, delegate: self)
    }))

    func exportMenuItem(for fileType: KmlFileType) -> BookmarksListMenuItem {
      let title: String
      switch fileType {
      case .text:
        title = L("export_file")
      case .gpx:
        title = L("export_file_gpx")
      default:
        fatalError("Unexpected file type")
      }
      return BookmarksListMenuItem(title: title, action: { [weak self] in
        self?.interactor.exportFile(fileType: fileType) { (status, url) in
          switch status {
          case .success:
            guard let url = url else { fatalError() }
            self?.view.share(url) {
              self?.interactor.finishExportFile()
            }
          case .emptyCategory:
            self?.view.showError(title: L("bookmarks_error_title_share_empty"),
                                 message: L("bookmarks_error_message_share_empty"))
          case .archiveError, .fileError:
            self?.view.showError(title: L("dialog_routing_system_error"),
                                 message: L("bookmarks_error_message_share_general"))
          }
        }
      })
    }
    moreItems.append(exportMenuItem(for: .text))
    moreItems.append(exportMenuItem(for: .gpx))
    moreItems.append(BookmarksListMenuItem(title: L("delete_list"),
                                           destructive: true,
                                           enabled: interactor.canDeleteGroup(),
                                           action: { [weak self] in
                                            self?.interactor.deleteBookmarksGroup()
                                            self?.delegate?.bookmarksListDidDeleteGroup()
                                           }))
    view.showMenu(moreItems, from: .more)
  }

  private func viewOnMap() {
    interactor.viewOnMap()
    router.viewOnMap(bookmarkGroup)
  }

  private func sort(_ sortingType: BookmarksListSortingType) {
    interactor.sort(sortingType, location: LocationManager.lastLocation()) { [weak self] sortedSections in
      let sections = sortedSections.map { (bookmarksSection) -> IBookmarksListSectionViewModel in
        if let bookmarks = bookmarksSection.bookmarks, let self = self {
          return BookmarksSectionViewModel(title: bookmarksSection.sectionName, bookmarks: self.mapBookmarks(bookmarks))
        }
        if let tracks = bookmarksSection.tracks, let self = self {
          return TracksSectionViewModel(tracks: tracks.map { track in
            TrackViewModel(track, formattedDistance: self.formatDistance(Double(track.trackLengthMeters)), colorDidTap: {
              self.view.showColorPicker(with: .defaultColorPicker(track.trackColor)) { color in
                BookmarksManager.shared().updateTrack(track.trackId, setColor: color)
                self.reload()
              }
            })
          })
        }
        fatalError()
      }
      self?.view.setSections(sections)
    }
  }
}

extension BookmarksListPresenter: IBookmarksListPresenter {
  func viewDidLoad() {
    reload()
    view.setTitle(bookmarkGroup.title)
    view.setMoreItemTitle(L("placepage_more_button"))
    view.enableEditing(true)

    let info = BookmarksListInfo(title: bookmarkGroup.title,
                                 description: bookmarkGroup.detailedAnnotation,
                                 hasDescription: bookmarkGroup.hasDescription,
                                 isHtmlDescription: bookmarkGroup.isHtmlDescription,
                                 imageUrl: bookmarkGroup.imageUrl,
                                 hasLogo: false)
    view.setInfo(info)
  }

  func viewDidAppear() {
    reload()
  }

  func activateSearch() {
    interactor.prepareForSearch()
  }

  func deactivateSearch() {

  }

  func cancelSearch() {
    reload()
  }

  func search(_ text: String) {
    interactor.search(text) { [weak self] in
      guard let self = self else { return }
      let bookmarks = self.mapBookmarks($0)
      self.view.setSections(bookmarks.isEmpty ? [] : [BookmarksSectionViewModel(title: L("bookmarks"),
                                                                                 bookmarks: bookmarks)])
    }
  }

  func more() {
    showMoreMenu()
  }

  func sort() {
    showSortMenu()
  }

  func deleteItem(in section: IBookmarksListSectionViewModel, at index: Int) {
    switch section {
    case let bookmarksSection as IBookmarksSectionViewModel:
      guard let bookmark = bookmarksSection.bookmarks[index] as? BookmarkViewModel else { fatalError() }
      interactor.deleteBookmark(bookmark.bookmarkId)
      reload()
    case let tracksSection as ITracksSectionViewModel:
      guard let track = tracksSection.tracks[index] as? TrackViewModel else { fatalError() }
      interactor.deleteTrack(track.trackId)
      reload()
    default:
      fatalError("Cannot delete item: unsupported section type: \(section.self)")
    }
  }

  func moveItem(in section: IBookmarksListSectionViewModel, at index: Int) {
    let group = interactor.getBookmarkGroup()
    switch section {
    case let bookmarksSection as IBookmarksSectionViewModel:
      guard let bookmark = bookmarksSection.bookmarks[index] as? BookmarkViewModel else { fatalError() }
      editingItem = .bookmark(bookmark.bookmarkId)
      router.selectGroup(currentGroupName: group.title, currentGroupId: group.categoryId, delegate: self)
    case let tracksSection as ITracksSectionViewModel:
      guard let track = tracksSection.tracks[index] as? TrackViewModel else { fatalError() }
      editingItem = .track(track.trackId)
      router.selectGroup(currentGroupName: group.title, currentGroupId: group.categoryId, delegate: self)
    default:
      fatalError("Cannot move item: unsupported section type: \(section.self)")
    }
  }

  func editItem(in section: IBookmarksListSectionViewModel, at index: Int) {
    switch section {
    case let bookmarksSection as IBookmarksSectionViewModel:
      guard let bookmarkId = (bookmarksSection.bookmarks[index] as? BookmarkViewModel)?.bookmarkId else { fatalError() }
      router.editBookmark(bookmarkId: bookmarkId) { [weak self] wasChanged in
        if wasChanged { self?.reload() }
      }
    case let tracksSection as ITracksSectionViewModel:
      guard let trackId = (tracksSection.tracks[index] as? TrackViewModel)?.trackId else { fatalError() }
      router.editTrack(trackId: trackId) { [weak self] wasChanged in
        if wasChanged { self?.reload() }
      }
    default:
      fatalError("Cannot edit item: unsupported section type: \(section.self)")
    }
  }

  func selectItem(in section: IBookmarksListSectionViewModel, at index: Int) {
    switch section {
    case let bookmarksSection as IBookmarksSectionViewModel:
      let bookmark = bookmarksSection.bookmarks[index] as! BookmarkViewModel
      interactor.viewBookmarkOnMap(bookmark.bookmarkId)
      router.viewOnMap(bookmarkGroup)
    case let tracksSection as ITracksSectionViewModel:
      let track = tracksSection.tracks[index] as! TrackViewModel
      interactor.viewTrackOnMap(track.trackId)
      router.viewOnMap(bookmarkGroup)
    case let subgroupsSection as ISubgroupsSectionViewModel:
      let subgroup = subgroupsSection.subgroups[index] as! SubgroupViewModel
      router.showSubgroup(subgroup.groupId)
      if subgroup.type == .collection {
      } else if subgroup.type == .category {
      } else {
        assertionFailure()
      }
    default:
      fatalError("Wrong section type: \(section.self)")
    }
  }

  func showDescription() {
    router.showDescription(bookmarkGroup)
  }

  func checkItem(in section: IBookmarksListSectionViewModel, at index: Int, checked: Bool) {
    switch section {
    case let subgroupsSection as ISubgroupsSectionViewModel:
      let subgroup = subgroupsSection.subgroups[index] as! SubgroupViewModel
      interactor.setGroup(subgroup.groupId, visible: checked)
      reload()
    default:
      fatalError("Wrong section type: \(section.self)")
    }
  }

  func toggleVisibility(in section: IBookmarksListSectionViewModel) {
    switch section {
    case let subgroupsSection as ISubgroupsSectionViewModel:
      let visible: Bool
      switch subgroupsSection.visibilityButtonState {
      case .hidden:
        fatalError("Unexpected visibility button state")
      case .hideAll:
        visible = false
      case .showAll:
        visible = true
      }
      subgroupsSection.subgroups.forEach {
        let subgroup = $0 as! SubgroupViewModel
        interactor.setGroup(subgroup.groupId, visible: visible)
      }
      reload()
    default:
      fatalError("Wrong section type: \(section.self)")
    }
  }
}

extension BookmarksListPresenter: CategorySettingsViewControllerDelegate {
  func categorySettingsController(_ viewController: CategorySettingsViewController, didEndEditing categoryId: MWMMarkGroupID) {
    let info = BookmarksListInfo(title: bookmarkGroup.title,
                                 description: bookmarkGroup.detailedAnnotation,
                                 hasDescription: bookmarkGroup.hasDescription,
                                 isHtmlDescription: bookmarkGroup.isHtmlDescription,
                                 imageUrl: bookmarkGroup.imageUrl,
                                 hasLogo: false)
    view.setInfo(info)
    viewController.goBack()
  }

  func categorySettingsController(_ viewController: CategorySettingsViewController, didDelete categoryId: MWMMarkGroupID) {
    if let delegate = delegate as? UIViewController {
      viewController.navigationController?.popToViewController(delegate, animated: true)
    }
  }
}

extension BookmarksListPresenter: SelectBookmarkGroupViewControllerDelegate {
    func bookmarkGroupViewController(_ viewController: SelectBookmarkGroupViewController,
                                     didSelect groupTitle: String,
                                     groupId: MWMMarkGroupID) {

      defer { viewController.dismiss(animated: true) }

      guard groupId != bookmarkGroup.categoryId else { return }
      
      switch editingItem {
      case .bookmark(let bookmarkId):
        interactor.moveBookmark(bookmarkId, toGroupId: groupId)
      case .track(let trackId):
        interactor.moveTrack(trackId, toGroupId: groupId)
      case .none:
        break
      }
      
      editingItem = nil
      
      if bookmarkGroup.bookmarksCount > 0 || bookmarkGroup.trackCount > 0 {
        reload()
      } else {
        // if there are no bookmarks or tracks in current group no need to show this group
        // e.g. popping view controller 2 times
        if let rootNavigationController = viewController.presentingViewController as? UINavigationController {
          rootNavigationController.popViewController(animated: false)
        }
      }
    }
}

extension IBookmarksSectionViewModel {
  var numberOfItems: Int { bookmarks.count }
  var visibilityButtonState: BookmarksListVisibilityButtonState { .hidden }
  var canEdit: Bool { true }
}

extension ITracksSectionViewModel {
  var numberOfItems: Int { tracks.count }
  var sectionTitle: String { L("tracks_title") }
  var visibilityButtonState: BookmarksListVisibilityButtonState { .hidden }
  var canEdit: Bool { true }
}

extension ISubgroupsSectionViewModel {
  var numberOfItems: Int { subgroups.count }
  var visibilityButtonState: BookmarksListVisibilityButtonState {
    subgroups.reduce(false) { $0 ? $0 : $1.isVisible } ? .hideAll : .showAll
  }
  var canEdit: Bool { false }
}

fileprivate struct BookmarkViewModel: IBookmarksListItemViewModel {
  let bookmarkId: MWMMarkID
  let name: String
  let subtitle: String
  var image: UIImage {
    bookmarkColor.image(bookmarkIconName)
  }
  var colorDidTapAction: (() -> Void)?

  private let bookmarkColor: BookmarkColor
  private let bookmarkIconName: String

  init(_ bookmark: Bookmark, formattedDistance: String?, colorDidTap: (() -> Void)?) {
    bookmarkId = bookmark.bookmarkId
    name = bookmark.bookmarkName
    bookmarkColor = bookmark.bookmarkColor
    bookmarkIconName = bookmark.bookmarkIconName
    subtitle = [formattedDistance, bookmark.bookmarkType].compactMap { $0 }.joined(separator: " • ")
    colorDidTapAction = colorDidTap
  }
}

fileprivate struct TrackViewModel: IBookmarksListItemViewModel {
  let trackId: MWMTrackID
  let name: String
  let subtitle: String
  var image: UIImage {
    circleImageForColor(trackColor, frameSize: 22)
  }
  var colorDidTapAction: (() -> Void)?

  private let trackColor: UIColor

  init(_ track: Track, formattedDistance: String, colorDidTap: (() -> Void)?) {
    trackId = track.trackId
    name = track.trackName
    subtitle = "\(L("length")) \(formattedDistance)"
    trackColor = track.trackColor
    colorDidTapAction = colorDidTap
  }
}

fileprivate struct SubgroupViewModel: ISubgroupViewModel {
  let groupId: MWMMarkGroupID
  let subgroupName: String
  let subtitle: String
  let isVisible: Bool
  let type: BookmarkGroupType

  init(_ bookmarkGroup: BookmarkGroup) {
    groupId = bookmarkGroup.categoryId
    subgroupName = bookmarkGroup.title
    subtitle = bookmarkGroup.placesCountTitle()
    isVisible = bookmarkGroup.isVisible
    type = bookmarkGroup.type
  }
}

fileprivate struct BookmarksSectionViewModel: IBookmarksSectionViewModel {
  let sectionTitle: String
  let bookmarks: [IBookmarksListItemViewModel]

  init(title: String, bookmarks: [IBookmarksListItemViewModel]) {
    sectionTitle = title
    self.bookmarks = bookmarks
  }
}

fileprivate struct TracksSectionViewModel: ITracksSectionViewModel {
  let tracks: [IBookmarksListItemViewModel]

  init(tracks: [IBookmarksListItemViewModel]) {
    self.tracks = tracks
  }
}

fileprivate struct SubgroupsSectionViewModel: ISubgroupsSectionViewModel {
  let subgroups: [ISubgroupViewModel]
  let sectionTitle: String
  var type: BookmarkGroupType

  init(title: String, subgroups: [ISubgroupViewModel], type: BookmarkGroupType) {
    sectionTitle = title
    self.type = type
    self.subgroups = subgroups
  }
}

fileprivate struct BookmarksListMenuItem: IBookmarksListMenuItem {
  let title: String
  let destructive: Bool
  let enabled: Bool
  let action: () -> Void

  init(title: String, destructive: Bool = false, enabled: Bool = true, action: @escaping () -> Void) {
    self.title = title
    self.destructive = destructive
    self.enabled = enabled
    self.action = action
  }
}

fileprivate struct BookmarksListInfo: IBookmarksListInfoViewModel {
  let title: String
  let description: String
  let hasDescription: Bool
  let isHtmlDescription: Bool
  let imageUrl: URL?
  let hasLogo: Bool

  init(title: String, description: String, hasDescription: Bool, isHtmlDescription: Bool, imageUrl: URL? = nil, hasLogo: Bool = false) {
    self.title = title
    self.description = description
    self.hasDescription = hasDescription
    self.isHtmlDescription = isHtmlDescription
    self.imageUrl = imageUrl
    self.hasLogo = hasLogo
  }
}
