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

  override func traitCollectionDidChange(_ previousTraitCollection: UITraitCollection?) {
    super.traitCollectionDidChange(previousTraitCollection)
    guard traitCollection.userInterfaceStyle != previousTraitCollection?.userInterfaceStyle else { return }
    updateViews()
  }

  func updateViews() {
    if placePagePreviewData.isMyPosition {
      if let speedAndAltitude = speedAndAltitude {
        subtitleLabel.text = speedAndAltitude
      }
    } else {
      let subtitleString = NSMutableAttributedString()
//      if placePagePreviewData.isPopular {
//        subtitleString.append(NSAttributedString(string: L("popular_place"),
//                                                 attributes: [.foregroundColor : UIColor.linkBlue(),
//                                                              .font : UIFont.regular14()]))
//      }

      if let subtitle = placePagePreviewData.subtitle ?? placePagePreviewData.coordinates {
        subtitleString.append(NSAttributedString(string: !subtitleString.string.isEmpty ? " • " + subtitle : subtitle,
                                                 attributes: [.foregroundColor : UIColor.blackSecondaryText(),
                                                              .font : UIFont.regular14()]))
      }

      subtitleLabel.attributedText = subtitleString
    }

    placePageDirectionView = subtitleDirectionView

    if let address = placePagePreviewData.secondarySubtitle {
      addressLabel.text = address
      placePageDirectionView = addressDirectionView
    } else {
      addressContainerView.isHidden = true
    }
    placePageDirectionView?.imageView.changeColoringToOpposite()
    configSchedule()
  }

  func updateDistance(_ distance: String) {
    self.distance = distance
    placePageDirectionView?.isHidden = false
    placePageDirectionView?.label.text = distance
    fullScreenDirectionView.updateDistance(distance)
  }

  func updateHeading(_ angle: CGFloat) {
    placePageDirectionView?.imageView.isHidden = false
    let duration = heading == nil ? .zero : kDefaultAnimationDuration // skip the initial setup animation
    UIView.animate(withDuration: duration,
                   delay: 0,
                   options: [.beginFromCurrentState, .curveEaseInOut],
                   animations: { [unowned self] in
                    self.placePageDirectionView?.imageView.transform = CGAffineTransform(rotationAngle: CGFloat.pi / 2 - angle)
    })
    fullScreenDirectionView.updateHeading(angle)
    heading = angle
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
    let now = time_t(Date().timeIntervalSince1970)

    func stringFromTime(_ time: Int) -> String {
      DateTimeFormatter.dateString(from: Date(timeIntervalSince1970: TimeInterval(time)),
                                   dateStyle: .none,
                                   timeStyle: .short)
    }

    switch placePagePreviewData.schedule.state {
    case .unknown:
      scheduleContainerView.isHidden = true
    case .allDay:
      setScheduleLabel(state: L("twentyfour_seven"),
                       stateColor: UIColor.systemGreen,
                       details: nil)
      
    case .open:
      let nextTimeClosed = placePagePreviewData.schedule.nextTimeClosed
      let minutesUntilClosed = (nextTimeClosed - now) / 60
      let stringTimeInterval = getTimeIntervalString(minutes: minutesUntilClosed)
      let stringTime = stringFromTime(nextTimeClosed)

      let details: String?
      if (minutesUntilClosed < 3 * 60)  // Less than 3 hours
      {
        details = String(format: L("closes_in"), stringTimeInterval) + " • " + stringTime
      }
      else if (minutesUntilClosed < 24 * 60)  // Less than 24 hours
      {
        details = String(format: L("closes_at"), stringTime)
      }
      else
      {
        details = nil
      }
      
      setScheduleLabel(state: L("editor_time_open"),
                       stateColor: UIColor.systemGreen,
                       details: details)
      
    case .closed:
      let nextTimeOpen = placePagePreviewData.schedule.nextTimeOpen
      let nextTimeOpenDate = Date(timeIntervalSince1970: TimeInterval(nextTimeOpen))
      
      let minutesUntilOpen = (nextTimeOpen - now) / 60
      let stringTimeInterval = getTimeIntervalString(minutes: minutesUntilOpen)
      let stringTime = stringFromTime(nextTimeOpen)

      let details: String?
      if (minutesUntilOpen < 3 * 60)  // Less than 3 hours
      {
        details = String(format: L("opens_in"), stringTimeInterval) + " • " + stringTime
      }
      else if (Calendar.current.isDateInToday(nextTimeOpenDate))   // Today
      {
        details = String(format: L("opens_at"), stringTime)
      }
      else if (minutesUntilOpen < 24 * 60)   // Less than 24 hours
      {
        details = String(format: L("opens_tomorrow_at"), stringTime)
      }
      else if (minutesUntilOpen < 7 * 24 * 60)  // Less than 1 week
      {
        let dayOfWeek = DateTimeFormatter.dateString(from: nextTimeOpenDate, format: "EEEE")
        details = String(format: L("opens_dayoftheweek_at"), dayOfWeek, stringTime)
      }
      else
      {
        details = nil
      }
      
      setScheduleLabel(state: L("closed_now"),
                       stateColor: UIColor.systemRed,
                       details: details)
      
    @unknown default:
      fatalError()
    }
  }
  
  private func getTimeIntervalString(minutes: Int) -> String {
    var str = ""
    if (minutes >= 60)
    {
      str = String(minutes / 60) + " " + L("hour") + " "
    }
    str += String(minutes % 60) + " " + L("minute")
    return str
  }
  
  private func setScheduleLabel(state: String, stateColor: UIColor, details: String?) {
    let attributedString = NSMutableAttributedString()
    let stateString = NSAttributedString(string: state,
                                         attributes: [NSAttributedString.Key.font: UIFont.regular14(),
                                                      NSAttributedString.Key.foregroundColor: stateColor])
    attributedString.append(stateString)
    if (details != nil)
    {
      let detailsString = NSAttributedString(string: " • " + details!,
                                             attributes: [NSAttributedString.Key.font: UIFont.regular14(),
                                                          NSAttributedString.Key.foregroundColor: UIColor.blackSecondaryText()])
      attributedString.append(detailsString)
    }
    scheduleLabel.attributedText = attributedString
  }
}
