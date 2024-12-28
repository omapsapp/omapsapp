final class SettingsTableViewiCloudSwitchCell: SettingsTableViewDetailedSwitchCell {

  @objc
  func updateWithSynchronizationState(_ state: SynchronizationManagerState) {
    guard state.isAvailable else {
      accessoryView = nil
      accessoryType = .detailButton
      return
    }
    accessoryView = switchButton
    detailTextLabel?.text = state.error?.localizedDescription
    setOn(state.isOn, animated: true)
    setNeedsLayout()
  }
}
