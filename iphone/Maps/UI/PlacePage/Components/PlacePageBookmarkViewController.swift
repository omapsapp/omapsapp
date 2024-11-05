protocol PlacePageBookmarkViewControllerDelegate: AnyObject {
  func bookmarkDidPressEdit()
  func trackDidPressEdit()
}

final class PlacePageBookmarkViewController: UIViewController {
  enum BookmarkData {
    case bookmark(PlacePageBookmarkData)
    case track(PlacePageTrackData)
  }

  @IBOutlet var stackView: UIStackView!
  @IBOutlet var spinner: UIImageView!
  @IBOutlet var editButton: UIButton!
  @IBOutlet var topConstraint: NSLayoutConstraint!
  @IBOutlet var expandableLabel: ExpandableLabel! {
    didSet {
      expandableLabel.font = UIFont.regular14()
      expandableLabel.textColor = UIColor.blackPrimaryText()
      expandableLabel.numberOfLines = 5
      expandableLabel.expandColor = UIColor.linkBlue()
      expandableLabel.expandText = L("placepage_more_button")
    }
  }

  var bookmarkData: BookmarkData? {
    didSet {
      updateViews()
    }
  }
  weak var delegate: PlacePageBookmarkViewControllerDelegate?

  override func viewDidLoad() {
    super.viewDidLoad()
    updateViews()
  }

  func updateViews() {
    guard let bookmarkData else { return }
    editButton.isEnabled = true
    switch bookmarkData {
    case .bookmark(let bookmark):
      editButton.setTitle(L("placepage_edit_bookmark_button"), for: .normal)
      if let description = bookmark.bookmarkDescription {
        if bookmark.isHtmlDescription {
          setHtmlDescription(description)
          topConstraint.constant = 16
        } else {
          expandableLabel.text = description
          topConstraint.constant = description.count > 0 ? 16 : 0
        }
      } else {
        topConstraint.constant = 0
      }
    case .track:
      editButton.setTitle(L("edit_track"), for: .normal)
      expandableLabel.isHidden = true
      topConstraint.constant = 0
    }
  }

  private func setHtmlDescription(_ htmlDescription: String) {
    DispatchQueue.global().async {
      let font = UIFont.regular14()
      let color = UIColor.blackPrimaryText()
      let paragraphStyle = NSMutableParagraphStyle()
      paragraphStyle.lineSpacing = 4

      let attributedString: NSAttributedString
      if let str = NSMutableAttributedString(htmlString: htmlDescription, baseFont: font, paragraphStyle: paragraphStyle) {
        str.addAttribute(NSAttributedString.Key.foregroundColor,
                         value: color,
                         range: NSRange(location: 0, length: str.length))
        attributedString = str;
      } else {
        attributedString = NSAttributedString(string: htmlDescription,
                                              attributes: [NSAttributedString.Key.font : font,
                                                           NSAttributedString.Key.foregroundColor: color,
                                                           NSAttributedString.Key.paragraphStyle: paragraphStyle])
      }

      DispatchQueue.main.async {
        self.expandableLabel.attributedText = attributedString
      }
    }
  }

  private func startSpinner() {
    editButton.isHidden = true
    let postfix = UIColor.isNightMode() ? "dark" : "light"
    spinner.image = UIImage(named: "Spinner_" + postfix)
    spinner.isHidden = false
    spinner.startRotation()
  }

  private func stopSpinner() {
    editButton.isHidden = false
    spinner.isHidden = true
    spinner.stopRotation()
  }

  @IBAction func onEdit(_ sender: UIButton) {
    guard let bookmarkData else { return }
    switch bookmarkData {
    case .bookmark:
      delegate?.bookmarkDidPressEdit()
    case .track:
      delegate?.trackDidPressEdit()
    }
  }
  
  override func applyTheme() {
    super.applyTheme()
    updateViews()
  }
}
