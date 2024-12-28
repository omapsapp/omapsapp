final class NavigationStreetNameView: SolidTouchView {
  override var visibleAreaAffectDirections: MWMAvailableAreaAffectDirections {
    return alternative(iPhone: [], iPad: .top)
  }

  override var placePageAreaAffectDirections: MWMAvailableAreaAffectDirections {
    return alternative(iPhone: [], iPad: .top)
  }

  override var sideButtonsAreaAffectDirections: MWMAvailableAreaAffectDirections {
    return .top
  }

  override var trackRecordingButtonAreaAffectDirections: MWMAvailableAreaAffectDirections {
    return .top
  }
}
