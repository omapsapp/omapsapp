import UIKit

@objc(MWMEditBookmarkController)
final class EditBookmarkViewController: MWMTableViewController {
  private enum Sections: Int {
    case info
    case description
    case delete
    case count
  }

  private enum InfoSectionRows: Int {
    case title
    case color
    case bookmarkGroup
    case count
  }
  
  private var editingCompleted: ((Bool) -> Void)?

  private var placePageData: PlacePageData?

  private var noteCell: MWMNoteCell?
  private var bookmarkTitle: String?
  private var bookmarkDescription: String?
  private var bookmarkGroupTitle: String?
  private var bookmarkId = FrameworkHelper.invalidBookmarkId()
  private var bookmarkGroupId = FrameworkHelper.invalidCategoryId()
  private var newBookmarkGroupId = FrameworkHelper.invalidCategoryId()
  private var bookmarkColor: BookmarkColor!
  private let bookmarksManager = BookmarksManager.shared()

  override func viewDidLoad() {
    super.viewDidLoad()

    guard bookmarkId != FrameworkHelper.invalidBookmarkId() || placePageData != nil else {
      fatalError("controller should be configured with placePageData or bookmarkId first")
    }

    title = L("bookmark").capitalized
    navigationItem.rightBarButtonItem = UIBarButtonItem(barButtonSystemItem: .save,
                                                        target: self,
                                                        action: #selector(onSave))

    tableView.registerNib(cell: BookmarkTitleCell.self)
    tableView.registerNib(cell: MWMButtonCell.self)
    tableView.registerNib(cell: MWMNoteCell.self)

    addToBookmarksManagerObserverList()
  }

  override func viewWillAppear(_ animated: Bool) {
    super.viewWillAppear(animated)
        updateBookmarkIfNeeded()
  }

  deinit {
    removeFromBookmarksManagerObserverList()
  }

  func configure(with bookmarkId: MWMMarkID, editCompletion completion: ((Bool) -> Void)?) {
    self.bookmarkId = bookmarkId

    let bookmark = bookmarksManager.bookmark(withId: bookmarkId)

    bookmarkTitle = bookmark.bookmarkName
    bookmarkColor = bookmark.bookmarkColor

    bookmarkDescription = bookmarksManager.description(forBookmarkId: bookmarkId)

    let bookmarkGroup = bookmarksManager.category(forBookmarkId: bookmarkId)
    bookmarkGroupId = bookmarkGroup.categoryId
    bookmarkGroupTitle = bookmarkGroup.title

    editingCompleted = completion
  }

  @objc(configureWithPlacePageData:)
  func configure(with placePageData: PlacePageData) {
    guard let bookmarkData = placePageData.bookmarkData else { fatalError("placePageData and bookmarkData can't be nil") }
    self.placePageData = placePageData
    
    bookmarkTitle = placePageData.previewData.title
    bookmarkDescription = bookmarkData.bookmarkDescription
    bookmarkGroupTitle = bookmarkData.bookmarkCategory
    bookmarkId = bookmarkData.bookmarkId
    bookmarkGroupId = bookmarkData.bookmarkGroupId
    bookmarkColor = bookmarkData.color
    
    editingCompleted = nil
  }

  // MARK: - Table view data source

  override func numberOfSections(in tableView: UITableView) -> Int {
    Sections.count.rawValue
  }

  override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
    switch Sections(rawValue: section) {
    case .info:
      return InfoSectionRows.count.rawValue
    case .description, .delete:
      return 1
    default:
      fatalError()
    }
  }

  override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
    switch Sections(rawValue: indexPath.section) {
    case .info:
      switch InfoSectionRows(rawValue: indexPath.row) {
      case .title:
        let cell = tableView.dequeueReusableCell(cell: BookmarkTitleCell.self, indexPath: indexPath)
        cell.configure(name: bookmarkTitle ?? "", delegate: self, hint: L("placepage_bookmark_name_hint"))
        return cell
      case .color:
        let cell = tableView.dequeueDefaultCell(for: indexPath)
        cell.accessoryType = .disclosureIndicator
        cell.textLabel?.text = bookmarkColor.title
        cell.imageView?.image = circleImageForColor(bookmarkColor.color, frameSize: 28, diameter: 22, iconName: "ic_bm_none")
        return cell
      case .bookmarkGroup:
        let cell = tableView.dequeueDefaultCell(for: indexPath)
        cell.textLabel?.text = bookmarkGroupTitle
        cell.imageView?.image = UIImage(named: "ic_folder")
        cell.imageView?.styleName = "MWMBlack";
        cell.accessoryType = .disclosureIndicator;
        return cell;
      default:
        fatalError()
      }
    case .description:
      if let noteCell = noteCell {
        return noteCell
      } else {
        let cell = tableView.dequeueReusableCell(cell: MWMNoteCell.self, indexPath: indexPath)
        cell.config(with: self, noteText: bookmarkDescription ?? "", placeholder: L("placepage_personal_notes_hint"))
        noteCell = cell
        return cell
      }
    case .delete:
      let cell = tableView.dequeueReusableCell(cell: MWMButtonCell.self, indexPath: indexPath)
      cell.configure(with: self, title: L("placepage_delete_bookmark_button"), enabled: true)
      return cell
    default:
      fatalError()
    }
  }

  override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
    tableView.deselectRow(at: indexPath, animated: true)
    switch InfoSectionRows(rawValue: indexPath.row) {
    case .color:
      openColorPicker()
    case .bookmarkGroup:
      openGroupPicker()
    default:
      break
    }
  }

  // MARK: - Private

  private func updateBookmarkIfNeeded() {
    // Skip for the regular place page.
    guard bookmarkId != FrameworkHelper.invalidBookmarkId() else { return }
    // TODO: Update the bookmark content on the Edit screen instead of closing it when the bookmark gets updated from cloud.
    if !bookmarksManager.hasBookmark(bookmarkId) {
      goBack()
    }
  }

  private func addToBookmarksManagerObserverList() {
    bookmarksManager.add(self)
  }

  private func removeFromBookmarksManagerObserverList() {
    bookmarksManager.remove(self)
  }

  @objc private func onSave() {
    view.endEditing(true)

    BookmarksManager.shared().updateBookmark(bookmarkId,
                                             setGroupId: bookmarkGroupId,
                                             title: bookmarkTitle ?? "",
                                             color: bookmarkColor,
                                             description: bookmarkDescription ?? "")
    if let placePageData = placePageData {
      FrameworkHelper.updatePlacePageData()
      placePageData.updateBookmarkStatus()
    }
    editingCompleted?(true)
    goBack()
  }

  @objc private func openColorPicker() {
    ColorPicker.shared.present(from: self, pickerType: .bookmarkColorPicker(bookmarkColor), completionHandler: { [weak self] color in
      self?.bookmarkColor = BookmarkColor.bookmarkColor(from: color)
      self?.tableView.reloadRows(at: [IndexPath(row: InfoSectionRows.color.rawValue, section: Sections.info.rawValue)], with: .none)
    })
  }

  private func openGroupPicker() {
    let groupViewController = SelectBookmarkGroupViewController(groupName: bookmarkGroupTitle ?? "", groupId: bookmarkGroupId)
    let navigationController = UINavigationController(rootViewController: groupViewController)
    groupViewController.delegate = self
    present(navigationController, animated: true, completion: nil)
  }
}

extension EditBookmarkViewController: BookmarkTitleCellDelegate {
  func didFinishEditingTitle(_ title: String) {
    bookmarkTitle = title
  }
}

extension EditBookmarkViewController: MWMNoteCellDelegate {
  func cell(_ cell: MWMNoteCell, didChangeSizeAndText text: String) {
    UIView.setAnimationsEnabled(false)
    tableView.refresh()
    UIView.setAnimationsEnabled(true)
  }

  func cell(_ cell: MWMNoteCell, didFinishEditingWithText text: String) {
    bookmarkDescription = text
  }
}

extension EditBookmarkViewController: MWMButtonCellDelegate {
  func cellDidPressButton(_ cell: UITableViewCell) {
    BookmarksManager.shared().deleteBookmark(bookmarkId)
    if let placePageData = placePageData {
      FrameworkHelper.updateAfterDeleteBookmark()
      placePageData.updateBookmarkStatus()
    }
    goBack()
  }
}

extension EditBookmarkViewController: SelectBookmarkGroupViewControllerDelegate {
  func bookmarkGroupViewController(_ viewController: SelectBookmarkGroupViewController,
                                   didSelect groupTitle: String,
                                   groupId: MWMMarkGroupID) {
    viewController.dismiss(animated: true)
    bookmarkGroupTitle = groupTitle
    bookmarkGroupId = groupId
    tableView.reloadRows(at: [IndexPath(row: InfoSectionRows.bookmarkGroup.rawValue, section: Sections.info.rawValue)], with: .none)
  }
}

// MARK: - BookmarksObserver
extension EditBookmarkViewController: BookmarksObserver {
  func onBookmarksLoadFinished() {
    updateBookmarkIfNeeded()
  }

  func onBookmarksCategoryDeleted(_ groupId: MWMMarkGroupID) {
    if bookmarkGroupId == groupId {
      goBack()
    }
  }
}
