final class PlacePageDirectionView: UIView {
  @IBOutlet var imageView: UIImageView!
  @IBOutlet var label: UILabel!
}

final class PlacePagePreviewViewController: UIViewController {
  @IBOutlet var stackView: UIStackView!
  @IBOutlet var popularView: UIView!
  @IBOutlet var subtitleLabel: UILabel! {
    didSet {
      subtitleLabel.textColor = UIColor.blackSecondaryText()
      subtitleLabel.font = UIFont.regular14()
    }
  }
  @IBOutlet var subtitleContainerView: UIStackView!
  @IBOutlet var scheduleLabel: UILabel!
  @IBOutlet var reviewsLabel: UILabel!
  @IBOutlet var addReviewButton: UIButton! {
    didSet {
      addReviewButton.setTitle("+ \(L("leave_a_review"))", for: .normal)
    }
  }
  @IBOutlet var priceLabel: UILabel!
  @IBOutlet var discountView: UIView!
  @IBOutlet var discountLabel: UILabel!
  @IBOutlet var ugcContainerView: UIStackView!
  @IBOutlet var addressLabel: UILabel!
  @IBOutlet var addressContainerView: UIStackView!
  @IBOutlet var scheduleContainerView: UIStackView!

  @IBOutlet var subtitleDirectionView: PlacePageDirectionView!
  @IBOutlet var addressDirectionView: PlacePageDirectionView!

  var placePageDirectionView: PlacePageDirectionView?
  lazy var fullScreenDirectionView: DirectionView = {
    return Bundle.main.load(viewClass: DirectionView.self)!
  }()

  var placePagePreviewData: PlacePagePreviewData! {
    didSet {
      if isViewLoaded {
        updateViews()
      }
    }
  }
  
  private var distance: String? = nil
  private var speedAndAltitude: String? = nil
  private var heading: CGFloat? = nil

  override func viewDidLoad() {
    super.viewDidLoad()

    updateViews()

    if let distance = distance {
      placePageDirectionView?.isHidden = false
      placePageDirectionView?.label.text = distance
    }

    if let heading = heading {
      updateHeading(heading)
    } else {
      placePageDirectionView?.imageView.isHidden = true
    }
  }

  private func updateViews() {
    if placePagePreviewData.isMyPosition {
      if let speedAndAltitude = speedAndAltitude {
        subtitleLabel.text = speedAndAltitude
      }
    } else {
      let subtitleString = NSMutableAttributedString()
      if placePagePreviewData.isPopular {
        subtitleString.append(NSAttributedString(string: L("popular_place"),
                                                 attributes: [.foregroundColor : UIColor.linkBlue(),
                                                              .font : UIFont.regular14()]))
      }

      if let subtitle = placePagePreviewData.subtitle ?? placePagePreviewData.coordinates {
        subtitleString.append(NSAttributedString(string: !subtitleString.string.isEmpty ? " • " + subtitle : subtitle,
                                                 attributes: [.foregroundColor : UIColor.blackSecondaryText(),
                                                              .font : UIFont.regular14()]))
      }

      subtitleLabel.attributedText = subtitleString
    }

    placePageDirectionView = subtitleDirectionView

    if let address = placePagePreviewData.address {
      addressLabel.text = address
      placePageDirectionView = addressDirectionView
    } else {
      addressContainerView.isHidden = true
    }

    configSchedule()
  }

  func updateDistance(_ distance: String) {
    self.distance = distance
    placePageDirectionView?.isHidden = false
    placePageDirectionView?.label.text = distance
    fullScreenDirectionView.updateDistance(distance)
  }

  func updateHeading(_ angle: CGFloat) {
    heading = angle
    placePageDirectionView?.imageView.isHidden = false
    UIView.animate(withDuration: kDefaultAnimationDuration,
                   delay: 0,
                   options: [.beginFromCurrentState, .curveEaseInOut],
                   animations: { [unowned self] in
                    self.placePageDirectionView?.imageView.transform = CGAffineTransform(rotationAngle: CGFloat.pi / 2 - angle)
    })
    fullScreenDirectionView.updateHeading(angle)
  }

  func updateSpeedAndAltitude(_ speedAndAltitude: String) {
    self.speedAndAltitude = speedAndAltitude
    subtitleLabel?.text = speedAndAltitude
  }

  @IBAction func onDirectionPressed(_ sender: Any) {
    guard let heading = heading else {
      return
    }

    fullScreenDirectionView.updateTitle(placePagePreviewData.title,
                                        subtitle: placePagePreviewData.subtitle ?? placePagePreviewData.coordinates)
    fullScreenDirectionView.updateHeading(heading)
    fullScreenDirectionView.updateDistance(distance)
    fullScreenDirectionView.show()
  }
  // MARK: private

  private func configSchedule() {
    switch placePagePreviewData.schedule {
    case .openingHoursAllDay:
      scheduleLabel.text = L("twentyfour_seven")
    case .openingHoursOpen:
      scheduleLabel.text = L("editor_time_open")
    case .openingHoursClosed:
      scheduleLabel.text = L("closed_now")
      scheduleLabel.textColor = UIColor.red
    case .openingHoursUnknown:
      scheduleContainerView.isHidden = true
    @unknown default:
      fatalError()
    }
  }
}
