protocol BottomMenuPresenterProtocol: UITableViewDelegate, UITableViewDataSource {
  func onClosePressed()
  func cellToHighlightIndexPath() -> IndexPath?
  func setCellHighlighted(_ highlighted: Bool)
}

class BottomMenuPresenter: NSObject {
  enum CellType: Int, CaseIterable {
    case addPlace
    case recordTrack
    case downloadMaps
    case donate
    case settings
    case share
  }

  enum Sections: Int {
    case layers
    case items
  }

  private weak var view: BottomMenuViewProtocol?
  private let interactor: BottomMenuInteractorProtocol
  private let sections: [Sections]
  private let menuCells: [CellType]
  private let trackRecorder = TrackRecordingManager.shared
  private var cellToHighlight: CellType?

  init(view: BottomMenuViewProtocol,
       interactor: BottomMenuInteractorProtocol,
       sections: [Sections]) {
    self.view = view
    self.interactor = interactor
    self.sections = sections
    let disableDonate = Settings.donateUrl() == nil
    self.menuCells = CellType.allCases.filter { disableDonate ? $0 != .donate : true }
    self.cellToHighlight = Self.getCellToHighlight()
    super.init()
  }

  private static func getCellToHighlight() -> CellType? {
    let featureToHighlightData = DeepLinkHandler.shared.getInAppFeatureHighlightData()
    guard let featureToHighlightData, featureToHighlightData.urlType == .menu else { return nil }
    switch featureToHighlightData.feature {
    case .trackRecorder: return .recordTrack
    default: return nil
    }
  }
}

extension BottomMenuPresenter: BottomMenuPresenterProtocol {
  func onClosePressed() {
    interactor.close()
  }

  func cellToHighlightIndexPath() -> IndexPath? {
    // Highlighting is enabled only for the .items section.
    guard let cellToHighlight,
          let sectionIndex = sections.firstIndex(of: .items),
          let cellIndex = menuCells.firstIndex(of: cellToHighlight) else { return nil }
    return IndexPath(row: cellIndex, section: sectionIndex)
  }

  func setCellHighlighted(_ highlighted: Bool) {
    cellToHighlight = nil
  }
}

//MARK: -- UITableViewDataSource

extension BottomMenuPresenter {
  func numberOfSections(in tableView: UITableView) -> Int {
    sections.count
  }

  func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
    switch sections[section] {
    case .layers:
      return 1
    case .items:
      return menuCells.count
    }
  }
  
  func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
    switch sections[indexPath.section] {
    case .layers:
      let cell = tableView.dequeueReusableCell(cell: BottomMenuLayersCell.self)!
      cell.onClose = { [weak self] in self?.onClosePressed() }
      if sections.count > 1 {
        cell.addSeparator(.bottom)
      }
      return cell
    case .items:
      let cell = tableView.dequeueReusableCell(cell: BottomMenuItemCell.self)!
      switch menuCells[indexPath.row] {
      case .addPlace:
        let enabled = MWMNavigationDashboardManager.shared().state == .hidden && FrameworkHelper.canEditMapAtViewportCenter()
        cell.configure(imageName: "ic_add_place",
                       title: L("placepage_add_place_button"),
                       enabled: enabled)
      case .recordTrack:
        switch trackRecorder.recordingState {
        case .inactive:
          cell.configure(imageName: "track_recorder_inactive", title: L("start_track_recording"))
        case .active:
          cell.configure(imageName: "track_recorder_active", title: L("stop_track_recording"))
        }
        return cell
      case .downloadMaps:
        cell.configure(imageName: "ic_menu_download",
                       title: L("download_maps"),
                       badgeCount: MapsAppDelegate.theApp().badgeNumber())
      case .donate:
        cell.configure(imageName: Settings.isNY() ? "ic_christmas_tree" : "ic_menu_donate",
                       title: L("donate"))
      case .settings:
        cell.configure(imageName: "ic_menu_settings",
                       title: L("settings"))
      case .share:
        cell.configure(imageName: "ic_menu_share",
                       title: L("share_my_location"))
      }
      return cell
    }
  }
}

//MARK: -- UITableViewDelegate

extension BottomMenuPresenter {
  func tableView(_ tableView: UITableView, willSelectRowAt indexPath: IndexPath) -> IndexPath? {
    if let cell = tableView.cellForRow(at: indexPath) as? BottomMenuItemCell {
      return cell.isEnabled ? indexPath : nil
    }
    return indexPath
  }

  func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
    tableView.deselectRow(at: indexPath, animated: true)
    switch sections[indexPath.section] {
    case .layers:
      return;
    case .items:
      switch menuCells[indexPath.row] {
      case .addPlace:
        interactor.addPlace()
      case .recordTrack:
        interactor.toggleTrackRecording()
      case .downloadMaps:
        interactor.downloadMaps()
      case .donate:
        interactor.donate()
      case .settings:
        interactor.openSettings()
      case .share:
        if let cell = tableView.cellForRow(at: indexPath) as? BottomMenuItemCell {
          interactor.shareLocation(cell: cell)
        }
      }
    }
  }
}
