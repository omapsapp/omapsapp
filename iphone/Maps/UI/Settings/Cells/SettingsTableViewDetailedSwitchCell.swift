class SettingsTableViewDetailedSwitchCell: SettingsTableViewSwitchCell {

  override func awakeFromNib() {
    super.awakeFromNib()
    styleDetail()
  }

  @objc
  func setDetail(_ text: String?) {
    detailTextLabel?.text = text
    setNeedsLayout()
  }

  private func styleDetail() {
    let detailTextLabelStyle = "regular12:blackSecondaryText"
    detailTextLabel?.setStyleNameAndApply(detailTextLabelStyle)
    detailTextLabel?.numberOfLines = 0
    detailTextLabel?.lineBreakMode = .byWordWrapping
  }
}
