protocol ActionBarViewControllerDelegate: AnyObject {
  func actionBar(_ actionBar: ActionBarViewController, didPressButton type: ActionBarButtonType)
}

final class ActionBarViewController: UIViewController {
  @IBOutlet var stackView: UIStackView!
  private(set) var downloadButton: ActionBarButton? = nil
  private(set) var bookmarkButton: ActionBarButton? = nil
  private var popoverSourceView: UIView? {
    stackView.arrangedSubviews.last
  }

  var placePageData: PlacePageData!
  var isRoutePlanning = false
  var canAddStop = false

  private var visibleButtons: [ActionBarButtonType] = []
  private var additionalButtons: [ActionBarButtonType] = []

  weak var delegate: ActionBarViewControllerDelegate?

  override func viewDidLoad() {
    super.viewDidLoad()
    configureButtons()
  }

  // MARK: - Private methods

  private func configureButtons() {
    if placePageData.isRoutePoint {
      visibleButtons.append(.routeRemoveStop)
    } else if placePageData.roadType != .none {
      switch placePageData.roadType {
      case .toll:
        visibleButtons.append(.avoidToll)
      case .ferry:
        visibleButtons.append(.avoidFerry)
      case .dirty:
        visibleButtons.append(.avoidDirty)
      default:
        fatalError()
      }
    } else {
      configButton1()
      configButton2()
      configButton3()
      configButton4()
    }

    setupButtonsState()
  }

  private func configButton1() {
    if let mapNodeAttributes = placePageData.mapNodeAttributes {
      switch mapNodeAttributes.nodeStatus {
      case .onDiskOutOfDate, .onDisk, .undefined:
        break
      case .downloading, .applying, .inQueue, .error, .notDownloaded, .partly:
        visibleButtons.append(.download)
        return
      @unknown default:
        fatalError()
      }
    }
    var buttons: [ActionBarButtonType] = []
    if isRoutePlanning {
      buttons.append(.routeFrom)
    }
    if placePageData.infoData?.phone != nil, AppInfo.shared().canMakeCalls {
      buttons.append(.call)
    }
    if !isRoutePlanning {
      buttons.append(.routeFrom)
    }

    assert(buttons.count > 0)
    visibleButtons.append(buttons[0])
    if buttons.count > 1 {
      additionalButtons.append(contentsOf: buttons.suffix(from: 1))
    }
  }

  private func configButton2() {
    var buttons: [ActionBarButtonType] = []
    switch placePageData.objectType {
    case .POI, .bookmark:
      if canAddStop {
        buttons.append(.routeAddStop)
      }
      buttons.append(.bookmark)
    case .track:
      buttons.append(.track)
    case .trackRecording:
      // TODO: implement for track recording
      break
    @unknown default:
      fatalError()
    }
    assert(buttons.count > 0)
    visibleButtons.append(buttons[0])
    if buttons.count > 1 {
      additionalButtons.append(contentsOf: buttons.suffix(from: 1))
    }
  }

  private func configButton3() {
    visibleButtons.append(.routeTo)
  }

  private func configButton4() {
    guard !additionalButtons.isEmpty else { return }
    additionalButtons.count == 1 ? visibleButtons.append(additionalButtons[0]) : visibleButtons.append(.more)
  }

  private func setupButtonsState() {
    for buttonType in visibleButtons {
      let (selected, enabled) = buttonState(buttonType)
      let button = ActionBarButton(delegate: self,
                                   buttonType: buttonType,
                                   isSelected: selected,
                                   isEnabled: enabled)
      stackView.addArrangedSubview(button)
      switch buttonType {
      case .download:
        downloadButton = button
        updateDownloadButtonState(placePageData.mapNodeAttributes!.nodeStatus)
      case .bookmark:
        bookmarkButton = button
      default:
        break
      }
    }
  }

  private func buttonState(_ buttonType: ActionBarButtonType) -> (selected: Bool, enabled: Bool) {
    var selected = false
    let enabled = true
    switch buttonType {
      case .bookmark:
      selected = placePageData.bookmarkData != nil
      case .track:
      selected = placePageData.trackData != nil
      default:
      break
    }
    return (selected, enabled)
  }

  private func showMore() {
    let actionSheet = UIAlertController(title: placePageData.previewData.title,
                                        message: placePageData.previewData.subtitle,
                                        preferredStyle: .actionSheet)
    for button in additionalButtons {
      let (selected, enabled) = buttonState(button)
      let action = UIAlertAction(title: titleForButton(button, selected),
                                 style: .default,
                                 handler: { [weak self] _ in
                                  guard let self = self else { return }
                                  self.delegate?.actionBar(self, didPressButton: button)
      })
      action.isEnabled = enabled
      actionSheet.addAction(action)
    }
    actionSheet.addAction(UIAlertAction(title: L("cancel"), style: .cancel))
    if let popover = actionSheet.popoverPresentationController, let sourceView = stackView.arrangedSubviews.last {
      popover.sourceView = sourceView
      popover.sourceRect = sourceView.bounds
    }
    present(actionSheet, animated: true)
  }


  // MARK: - Public methods

  func resetButtons() {
    stackView.arrangedSubviews.forEach {
      stackView.removeArrangedSubview($0)
      $0.removeFromSuperview()
    }
    visibleButtons.removeAll()
    additionalButtons.removeAll()
    downloadButton = nil
    bookmarkButton = nil
    configureButtons()
  }

  func updateDownloadButtonState(_ nodeStatus: MapNodeStatus) {
    guard let downloadButton = downloadButton, let mapNodeAttributes = placePageData.mapNodeAttributes else { return }
    switch mapNodeAttributes.nodeStatus {
    case .downloading:
      downloadButton.mapDownloadProgress?.state = .progress
    case .applying, .inQueue:
      downloadButton.mapDownloadProgress?.state = .spinner
    case .error:
      downloadButton.mapDownloadProgress?.state = .failed
    case .onDisk, .undefined, .onDiskOutOfDate:
      downloadButton.mapDownloadProgress?.state = .completed
    case .notDownloaded, .partly:
      downloadButton.mapDownloadProgress?.state = .normal
    @unknown default:
      fatalError()
    }
  }

  func updateBookmarkButtonState(isSelected: Bool) {
    guard let bookmarkButton else { return }
    if !isSelected && BookmarksManager.shared().hasRecentlyDeletedBookmark() {
      bookmarkButton.setBookmarkButtonState(.recover)
      return
    }
    bookmarkButton.setBookmarkButtonState(isSelected ? .delete : .save)
  }
}

extension ActionBarViewController: ActionBarButtonDelegate {
  func tapOnButton(with type: ActionBarButtonType) {
    switch type {
    case .more:
      showMore()
    default:
      delegate?.actionBar(self, didPressButton: type)
    }
  }
}
