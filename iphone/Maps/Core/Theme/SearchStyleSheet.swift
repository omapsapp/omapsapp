class SearchStyleSheet: IStyleSheet {
  static func register(theme: Theme, colors: IColors, fonts: IFonts) {
    theme.add(styleName: "SearchInstallButton") { (s) -> Void in
      s.cornerRadius = 10
      s.clip = true
      s.font = fonts.medium12
      s.fontColor = colors.blackSecondaryText
      s.backgroundColor = colors.searchPromoBackground
    }

    theme.add(styleName: "SearchBanner") { (s) -> Void in
      s.backgroundColor = colors.searchPromoBackground
    }

    theme.add(styleName: "SearchClosedBackground") { (s) -> Void in
      s.cornerRadius = 4
      s.backgroundColor = colors.blackHintText
    }

    theme.add(styleName: "SearchPopularView") { (s) -> Void in
      s.cornerRadius = 10
      s.backgroundColor = colors.linkBlueHighlighted
    }

    theme.add(styleName: "SearchSideAvaliableMarker") { (s) -> Void in
      s.backgroundColor = colors.ratingGreen
    }

    theme.add(styleName: "SearchBarView") { (s) -> Void in
      s.backgroundColor = colors.primary
      s.shadowRadius = 2
      s.shadowColor = UIColor(0, 0, 0, alpha26)
      s.shadowOpacity = 1
      s.shadowOffset = CGSize.zero
    }

    theme.add(styleName: "SearchActionBarView") { (s) -> (Void) in
      s.backgroundColor = colors.linkBlue
      s.cornerRadius = 20
      s.shadowRadius = 1
      s.shadowColor = UIColor(0, 0, 0, 0.24)
      s.shadowOffset = CGSize(width: 0, height: 2)
      s.shadowOpacity = 1
    }

    theme.add(styleName: "SearchActionBarButton") { (s) -> (Void) in
      s.backgroundColor = colors.clear
      s.fontColor = colors.whitePrimaryText
      s.font = fonts.semibold14
      s.coloring = .whiteText
    }

    theme.add(styleName: "SearchFilterAppliedIndicator") { (s) -> Void in
      s.round = true
      s.clip = true
    }

    theme.add(styleName: "SearchSearchTextField") { (s) -> Void in
      s.fontColor = colors.blackSecondaryText
      s.backgroundColor = colors.white
      s.tintColor = colors.blackSecondaryText
    }

    theme.add(styleName: "SearchSearchTextFieldIcon") { (s) -> Void in
      s.tintColor = colors.blackSecondaryText
      s.coloring = MWMButtonColoring.black
      s.color = colors.blackSecondaryText
    }

    theme.add(styleName: "SearchDatePickerField") { (s) -> Void in
      s.backgroundColor = colors.white
      s.cornerRadius = 4
      s.borderColor = colors.solidDividers
      s.borderWidth = 1
    }

    theme.add(styleName: "FilterRatingButton") { (s) -> Void in
      s.cornerRadius = 4
      s.borderWidth = 1
      s.borderColor = colors.blackDividers
    }

    theme.add(styleName: "SearchFilterTypeCell") { (s) -> Void in
      s.cornerRadius = 16
      s.borderColor = colors.blackDividers
      s.borderWidth = 1
    }

    theme.add(styleName: "FilterCheckButton") { (s) -> Void in
      s.fontColor = colors.blackPrimaryText
      s.fontColorDisabled = colors.blackDividers
      s.backgroundColor = colors.white
      s.font = fonts.regular14
      s.cornerRadius = 4
      s.borderWidth = 1
      s.borderColor = colors.blackDividers
      s.textAlignment = .natural
    }

    theme.add(styleName: "SearchCellAvaliable", from: "TableCell") { (s) -> Void in
      s.backgroundColor = colors.transparentGreen
    }

    theme.add(styleName: "DatePickerView") { (s) -> (Void) in
      s.backgroundColor = colors.white
      s.fontColor = colors.blackPrimaryText
      s.fontColorSelected = colors.whitePrimaryText
      s.backgroundColorSelected = colors.linkBlue
      s.backgroundColorHighlighted = colors.linkBlueHighlighted
      s.fontColorDisabled = colors.blackSecondaryText
    }

    theme.add(styleName: "BookingDateField")  { (s) -> (Void) in
      s.backgroundColor = colors.white
      s.cornerRadius = 4
      s.borderColor = colors.blackDividers
      s.borderWidth = 1
    }

    theme.add(styleName: "BookingDatePickerCancel", from: "FlatNormalTransButton") { (s) -> (Void) in
      s.font = fonts.regular16
    }

    theme.add(styleName: "BookingDatePickerDone", from: "FlatNormalTransButton") { (s) -> (Void) in
      s.font = fonts.semibold16
    }

    theme.add(styleName: "ValueStepperView") { (s) -> (Void) in
      s.font = fonts.regular16
      s.fontColor = colors.blackPrimaryText
      s.coloring = MWMButtonColoring.blue
    }
  }
}
