import UIKit

protocol DownloadAllViewDelegate: AnyObject {
  func onDownloadButtonPressed()
  func onRetryButtonPressed()
  func onCancelButtonPressed()
  func onStateChanged(state: DownloadAllView.State)
}

class DownloadAllView: UIView {
  enum State {
    case none
    case ready
    case error
    case dowloading
  }
  enum Style {
    case download
    case update
  }

  @IBOutlet private var iconImageView: UIImageView!
  @IBOutlet private var title: UILabel!
  @IBOutlet private var downloadSizeLabel: UILabel!
  @IBOutlet private var stateWrapper: UIView!
  @IBOutlet private var downloadButton: UIButton!
  @IBOutlet private var titleCenterConstraint: NSLayoutConstraint!
  lazy private var progress: MWMCircularProgress = {
    let view = MWMCircularProgress.downloaderProgress(forParentView: stateWrapper)
    view.delegate = self
    return view
  }()

  var isSizeHidden: Bool = false {
    didSet {
      if oldValue != isSizeHidden {
        updateView()
      }
    }
  }
  var style: Style = .download {
    didSet {
      if oldValue != style {
        updateView()
      }
    }
  }
  var state: State = .ready {
    didSet {
      if oldValue != state {
        updateView()
        delegate?.onStateChanged(state: state)
      }
    }
  }
  var downloadSize: UInt64 = 0 {
    didSet {
      downloadSizeLabel.text = formattedSize(downloadSize)
    }
  }
  var downloadProgress: CGFloat = 0 {
    didSet {
      self.progress.progress = downloadProgress
    }
  }
  weak var delegate: DownloadAllViewDelegate?

  @IBAction func onDownloadButtonPress(_ sender: Any) {
    if state == .error {
      delegate?.onRetryButtonPressed()
    } else {
      delegate?.onDownloadButtonPressed()
    }
  }

  private func updateView() {
    let readyTitle: String
    let downloadingTitle: String
    let readyButtonTitle: String
    let errorTitle = L("country_status_download_failed")
    let errorButtonTitle = L("downloader_retry")

    switch style {
    case .download:
      iconImageView.image = UIImage(named: "ic_download_all")
      readyTitle = L("downloader_download_all_button")
      downloadingTitle = L("downloader_loading_ios")
      readyButtonTitle = L("download_button")
    case .update:
      iconImageView.image = UIImage(named: "ic_update_all")
      readyTitle = L("downloader_update_maps")
      downloadingTitle = L("downloader_updating_ios")
      readyButtonTitle = L("downloader_update_all_button")
    }

    titleCenterConstraint.priority = isSizeHidden ? .defaultHigh : .defaultLow
    downloadSizeLabel.isHidden = isSizeHidden

    switch state {
    case .error:
      iconImageView.image = UIImage(named: "ic_download_error")
      title.text = errorTitle
      title.setStyleNameAndApply("redText")
      downloadButton.setTitle(errorButtonTitle, for: .normal)
      downloadButton.isHidden = false
      stateWrapper.isHidden = true
      progress.state = .spinner
      downloadSizeLabel.isHidden = false
    case .ready:
      title.text = readyTitle
      title.setStyleNameAndApply("blackPrimaryText")
      downloadButton.setTitle(readyButtonTitle, for: .normal)
      downloadButton.isHidden = false
      stateWrapper.isHidden = true
      progress.state = .spinner
      downloadSizeLabel.isHidden = false
    case .dowloading:
      title.text = downloadingTitle
      title.setStyleNameAndApply("blackPrimaryText")
      downloadButton.isHidden = true
      stateWrapper.isHidden = false
      progress.state = .spinner
    case .none:
      self.downloadButton.isHidden = true
      self.stateWrapper.isHidden = true
    }
  }
}

extension DownloadAllView: MWMCircularProgressProtocol {
  func progressButtonPressed(_ progress: MWMCircularProgress) {
    delegate?.onCancelButtonPressed()
  }
}
