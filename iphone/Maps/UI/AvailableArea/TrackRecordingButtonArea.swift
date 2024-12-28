final class TrackRecordingButtonArea: AvailableArea {
  override func isAreaAffectingView(_ other: UIView) -> Bool {
    return !other.trackRecordingButtonAreaAffectDirections.isEmpty
  }

  override func addAffectingView(_ other: UIView) {
    let ov = other.trackRecordingButtonAreaAffectView
    let directions = ov.trackRecordingButtonAreaAffectDirections
    addConstraints(otherView: ov, directions: directions)
  }

  override func notifyObserver() {
    TrackRecordingViewController.updateAvailableArea(areaFrame)
  }
}

extension UIView {
  @objc var trackRecordingButtonAreaAffectDirections: MWMAvailableAreaAffectDirections { return [] }

  var trackRecordingButtonAreaAffectView: UIView { return self }
}
