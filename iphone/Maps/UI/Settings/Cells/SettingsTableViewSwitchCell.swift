@objc
protocol SettingsTableViewSwitchCellDelegate {
  func switchCell(_ cell: SettingsTableViewSwitchCell, didChangeValue value: Bool)
}

@objc
class SettingsTableViewSwitchCell: MWMTableViewCell {

  let switchButton = UISwitch()

  @IBOutlet weak var delegate: SettingsTableViewSwitchCellDelegate?

  @objc
  var isEnabled = true {
    didSet {
      styleTitle()
      switchButton.isUserInteractionEnabled = isEnabled
    }
  }

  @objc
  var isOn: Bool {
    get { return switchButton.isOn }
    set { switchButton.isOn = newValue }
  }

  @objc
  func setOn(_ isOn: Bool, animated: Bool) {
    switchButton.setOn(isOn, animated: animated)
  }

  override func awakeFromNib() {
    super.awakeFromNib()
    setupCell()
  }

  @objc 
  func config(delegate: SettingsTableViewSwitchCellDelegate, title: String, isOn: Bool) {
    backgroundColor = UIColor.white()

    self.delegate = delegate

    self.textLabel?.text = title
    styleTitle()

    switchButton.isOn = isOn
  }

  private func setupCell() {
    styleName = "Background"
    styleTitle()
    textLabel?.numberOfLines = 0
    textLabel?.lineBreakMode = .byWordWrapping

    switchButton.onTintColor = UIColor.linkBlue()
    switchButton.addTarget(self, action: #selector(switchChanged), for: .valueChanged)
    accessoryView = switchButton
  }

  @objc 
  private func switchChanged() {
    delegate?.switchCell(self, didChangeValue: switchButton.isOn)
  }

  private func styleTitle() {
    let style = "regular17:" + (isEnabled ? "blackPrimaryText" : "blackSecondaryText")
    textLabel?.setStyleNameAndApply(style)
  }
}
