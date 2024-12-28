import Foundation

class GlobalStyleSheet: IStyleSheet {
  static func register(theme: Theme, colors: IColors, fonts: IFonts) {
    //MARK: Defaults
    theme.add(styleName: "TableView") { (s) -> (Void) in
      s.backgroundColor = colors.white
      s.separatorColor = colors.blackDividers
      s.exclusions = [String(describing: UIDatePicker.self)]
    }

    theme.add(styleName: "TableCell") { (s) -> (Void) in
      s.backgroundColor = colors.white
      s.fontColor = colors.blackPrimaryText
      s.tintColor = colors.linkBlue
      s.fontColorDetailed = colors.blackSecondaryText
      s.backgroundColorSelected = colors.pressBackground
      s.exclusions = [String(describing: UIDatePicker.self),
                       "_UIActivityUserDefaultsActivityCell"]
    }

    theme.add(styleName: "MWMTableViewCell", from: "TableCell") { (s) -> (Void) in
    }

    theme.add(styleName: "TableViewHeaderFooterView") { (s) -> (Void) in
      s.font = fonts.medium14
      s.fontColor = colors.blackSecondaryText
    }
    
    theme.add(styleName: "SearchBar") { (s) -> (Void) in
      s.backgroundColor = colors.white
      s.barTintColor = colors.primary
      s.fontColor = colors.blackPrimaryText
      s.fontColorDetailed = UIColor.white
      s.tintColor = colors.blackSecondaryText
    }

    theme.add(styleName: "NavigationBar") { (s) -> (Void) in
      s.barTintColor = colors.primary
      s.tintColor = colors.whitePrimaryText
      s.backgroundImage = UIImage()
      s.shadowImage = UIImage()
      s.font = fonts.header
      s.fontColor = colors.whitePrimaryText
    }

    theme.add(styleName: "NavigationBarItem") { (s) -> (Void) in
      s.font = fonts.regular18
      s.fontColor = colors.whitePrimaryText
      s.fontColorDisabled = UIColor.lightGray
      s.fontColorHighlighted = colors.whitePrimaryTextHighlighted
      s.tintColor = colors.whitePrimaryText
    }

    theme.add(styleName: "Checkmark") { (s) -> (Void) in
      s.onTintColor = colors.linkBlue
      s.offTintColor = colors.blackHintText
    }

    theme.add(styleName: "Switch") { (s) -> (Void) in
      s.onTintColor = colors.linkBlue
    }

    theme.add(styleName: "PageControl") { (s) -> (Void) in
      s.pageIndicatorTintColor = colors.blackHintText
      s.currentPageIndicatorTintColor = colors.blackSecondaryText
      s.backgroundColor = colors.white
    }

    theme.add(styleName: "StarRatingView") { (s) -> (Void) in
      s.onTintColor = colors.ratingYellow
      s.offTintColor = colors.blackDividers
    }

    theme.add(styleName: "DifficultyView") { (s) -> (Void) in
      s.colors = [colors.blackSecondaryText, colors.ratingGreen, colors.ratingYellow, colors.ratingRed]
      s.offTintColor = colors.blackSecondaryText
      s.backgroundColor = colors.clear
    }
    
    //MARK: Global styles
    theme.add(styleName: "Divider") { (s) -> (Void) in
      s.backgroundColor = colors.blackDividers
    }

    theme.add(styleName: "SolidDivider") { (s) -> (Void) in
      s.backgroundColor = colors.solidDividers
    }

    theme.add(styleName: "Background") { (s) -> (Void) in
      s.backgroundColor = colors.white
      s.backgroundColorSelected = colors.pressBackground
    }

    theme.add(styleName: "PressBackground") { (s) -> (Void) in
      s.backgroundColor = colors.pressBackground
    }

    theme.add(styleName: "PrimaryBackground") { (s) -> (Void) in
      s.backgroundColor = colors.primary
    }

    theme.add(styleName: "SecondaryBackground") { (s) -> (Void) in
      s.backgroundColor = colors.secondary
    }

    theme.add(styleName: "MenuBackground") { (s) -> (Void) in
      s.backgroundColor = colors.menuBackground
    }
    
    theme.add(styleName: "BottomTabBarButton") { (s) -> (Void) in
      s.backgroundColor = colors.tabBarButtonBackground
      s.tintColor = colors.blackSecondaryText
      s.coloring = MWMButtonColoring.black
      s.cornerRadius = 8
      s.shadowColor = UIColor(0,0,0,alpha20)
      s.shadowOpacity = 1
      s.shadowOffset = CGSize(width: 0, height: 1)
      s.onTintColor = .red
    }

    theme.add(styleName: "TrackRecordingWidgetButton", from: "BottomTabBarButton") { (s) -> (Void) in
      s.cornerRadius = 23
    }

    theme.add(styleName: "BlackOpaqueBackground") { (s) -> (Void) in
      s.backgroundColor = colors.blackOpaque
    }

    theme.add(styleName: "BlueBackground") { (s) -> (Void) in
      s.backgroundColor = colors.linkBlue
    }

    theme.add(styleName: "ToastBackground") { (s) -> (Void) in
      s.backgroundColor = colors.toastBackground
    }

    theme.add(styleName: "FadeBackground") { (s) -> (Void) in
      s.backgroundColor = colors.fadeBackground
    }

    theme.add(styleName: "ErrorBackground") { (s) -> (Void) in
      s.backgroundColor = colors.errorPink
    }

    theme.add(styleName: "BlackStatusBarBackground") { (s) -> (Void) in
      s.backgroundColor = colors.blackStatusBarBackground
    }

    theme.add(styleName: "PresentationBackground") { (s) -> (Void) in
      s.backgroundColor = UIColor.black.withAlphaComponent(alpha40)
    }

    theme.add(styleName: "ClearBackground") { (s) -> (Void) in
      s.backgroundColor = colors.clear
    }

    theme.add(styleName: "Border") { (s) -> (Void) in
      s.backgroundColor = colors.border
    }

    theme.add(styleName: "TabView") { (s) -> (Void) in
      s.backgroundColor = colors.pressBackground
      s.barTintColor = colors.primary
      s.tintColor = colors.white
      s.fontColor = colors.whitePrimaryText
      s.font = fonts.medium14
    }

    theme.add(styleName: "DialogView") { (s) -> (Void) in
      s.cornerRadius = 8
      s.shadowRadius = 2
      s.shadowColor = UIColor(0,0,0,alpha26)
      s.shadowOpacity = 1
      s.shadowOffset = CGSize(width: 0, height: 1)
      s.backgroundColor = colors.white
      s.clip = true
    }

    theme.add(styleName: "AlertView") { (s) -> (Void) in
      s.cornerRadius = 12
      s.shadowRadius = 6
      s.shadowColor = UIColor(0,0,0,alpha20)
      s.shadowOpacity = 1
      s.shadowOffset = CGSize(width: 0, height: 3)
      s.backgroundColor = colors.alertBackground
      s.clip = true
    }

    theme.add(styleName: "AlertViewTextFieldContainer") { (s) -> (Void) in
      s.borderColor = colors.blackDividers
      s.borderWidth = 0.5
      s.backgroundColor = colors.white
    }

    theme.add(styleName: "AlertViewTextField") { (s) -> Void in
      s.font = fonts.regular14
      s.fontColor = colors.blackPrimaryText
      s.tintColor = colors.blackSecondaryText
    }

    theme.add(styleName: "SearchStatusBarView") { (s) -> (Void) in
      s.backgroundColor = colors.primary
      s.shadowRadius = 2
      s.shadowColor = colors.blackDividers
      s.shadowOpacity = 1
      s.shadowOffset = CGSize(width: 0, height: 0)
    }

    //MARK: Buttons
    theme.add(styleName: "FlatNormalButton") { (s) -> (Void) in
      s.font = fonts.medium14
      s.cornerRadius = 8
      s.clip = true
      s.fontColor = colors.whitePrimaryText
      s.backgroundColor = colors.linkBlue
      s.fontColorHighlighted = colors.whitePrimaryTextHighlighted
      s.fontColorDisabled = colors.whitePrimaryTextHighlighted
      s.backgroundColorHighlighted = colors.linkBlueHighlighted
    }

    theme.add(styleName: "FlatNormalButtonBig", from: "FlatNormalButton") { (s) -> (Void) in
      s.font = fonts.regular17
    }

    theme.add(styleName: "FlatNormalTransButton") { (s) -> (Void) in
      s.font = fonts.medium14
      s.cornerRadius = 8
      s.clip = true
      s.fontColor = colors.linkBlue
      s.backgroundColor = colors.clear
      s.fontColorHighlighted = colors.linkBlueHighlighted
      s.fontColorDisabled = colors.blackHintText
      s.backgroundColorHighlighted = colors.clear
    }

    theme.add(styleName: "FlatNormalTransButtonBig", from: "FlatNormalTransButton") { (s) -> (Void) in
      s.font = fonts.regular17
    }

    theme.add(styleName: "FlatGrayTransButton") { (s) -> (Void) in
      s.font = fonts.medium14
      s.fontColor = colors.blackSecondaryText
      s.backgroundColor = colors.clear
      s.fontColorHighlighted = colors.linkBlueHighlighted
    }

    theme.add(styleName: "FlatPrimaryTransButton") { (s) -> (Void) in
      s.fontColor = colors.blackPrimaryText
      s.backgroundColor = colors.clear
      s.fontColorHighlighted = colors.linkBlueHighlighted
    }

    theme.add(styleName: "FlatRedTransButton") { (s) -> (Void) in
      s.font = fonts.medium14
      s.fontColor = colors.red
      s.backgroundColor = colors.clear
      s.fontColorHighlighted = colors.red
    }

    theme.add(styleName: "FlatRedTransButtonBig") { (s) -> (Void) in
      s.font = fonts.regular17
      s.fontColor = colors.red
      s.backgroundColor = colors.clear
      s.fontColorHighlighted = colors.red
    }

    theme.add(styleName: "FlatRedButton") { (s) -> (Void) in
      s.font = fonts.medium14
      s.cornerRadius = 8
      s.fontColor = colors.whitePrimaryText
      s.backgroundColor = colors.buttonRed
      s.fontColorHighlighted = colors.buttonRedHighlighted
    }

    theme.add(styleName: "MoreButton") { (s) -> (Void) in
      s.fontColor = colors.linkBlue
      s.fontColorHighlighted = colors.linkBlueHighlighted
      s.backgroundColor = colors.clear
      s.font = fonts.regular16
    }

    theme.add(styleName: "EditButton") { (s) -> (Void) in
      s.font = fonts.regular14
      s.fontColor = colors.linkBlue
      s.cornerRadius = 8
      s.borderColor = colors.linkBlue
      s.borderWidth = 1
      s.fontColorHighlighted = colors.linkBlueHighlighted
      s.backgroundColor = colors.clear
    }

    theme.add(styleName: "RateAppButton") { (s) -> (Void) in
      s.font = fonts.medium17
      s.fontColor = colors.linkBlue
      s.fontColorHighlighted = colors.white
      s.borderColor = colors.linkBlue
      s.cornerRadius = 8
      s.borderWidth = 1
      s.backgroundColor = colors.clear
      s.backgroundColorHighlighted = colors.linkBlue
    }

    theme.add(styleName: "TermsOfUseLinkText") { (s) -> (Void) in
      s.font = fonts.regular16
      s.fontColor = colors.blackPrimaryText

      s.linkAttributes = [NSAttributedString.Key.font: fonts.regular16,
                                     NSAttributedString.Key.foregroundColor: colors.linkBlue,
                                     NSAttributedString.Key.underlineColor: UIColor.clear]
      s.textContainerInset = UIEdgeInsets(top: 0, left: 0, bottom: 0, right: 0)
    }

    theme.add(styleName: "TermsOfUseGrayButton") { (s) -> (Void) in
      s.font = fonts.medium10
      s.fontColor = colors.blackSecondaryText
      s.fontColorHighlighted = colors.blackHintText
    }

    theme.add(styleName: "Badge") { (s) -> (Void) in
      s.round = true
      s.backgroundColor = colors.downloadBadgeBackground
    }

    //MARK: coloring
    theme.add(styleName: "MWMBlue") { (s) -> (Void) in
      s.tintColor = colors.linkBlue
      s.coloring = MWMButtonColoring.blue
    }

    theme.add(styleName: "MWMBlack") { (s) -> (Void) in
      s.tintColor = colors.blackSecondaryText
      s.coloring = MWMButtonColoring.black
    }

    theme.add(styleName: "MWMOther") { (s) -> (Void) in
      s.tintColor = colors.white
      s.coloring = MWMButtonColoring.other
    }

    theme.add(styleName: "MWMGray") { (s) -> (Void) in
      s.tintColor = colors.blackHintText
      s.coloring = MWMButtonColoring.gray
    }

    theme.add(styleName: "MWMSeparator") { (s) -> (Void) in
      s.tintColor = colors.blackDividers
      s.coloring = MWMButtonColoring.black
    }
    
    theme.add(styleName: "MWMWhite") { (s) -> (Void) in
      s.tintColor = colors.white
      s.coloring = MWMButtonColoring.white
    }
  }
}
