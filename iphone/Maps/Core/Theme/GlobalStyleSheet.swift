import Foundation

enum StyleSheet {
  case global(Global)

  var styleName: String {
    switch self {
    case .global(let style): return style.rawValue
    }
  }
}

extension StyleSheet {
  enum Global: String, CaseIterable {
    case tableView = "TableView"
    case tableCell = "TableCell"
    case tableViewCell = "MWMTableViewCell"
    case tableViewHeaderFooterView = "TableViewHeaderFooterView"
    case searchBar = "SearchBar"
    case navigationBar = "NavigationBar"
    case navigationBarItem = "NavigationBarItem"
    case checkmark = "Checkmark"
    case `switch` = "Switch"
    case pageControl = "PageControl"
    case starRatingView = "StarRatingView"
    case difficultyView = "DifficultyView"
    case divider = "Divider"
    case solidDivider = "SolidDivider"
    case background = "Background"
    case pressBackground = "PressBackground"
    case primaryBackground = "PrimaryBackground"
    case secondaryBackground = "SecondaryBackground"
    case menuBackground = "MenuBackground"
    case bottomTabBarButton = "BottomTabBarButton"
    case trackRecordingWidgetButton = "TrackRecordingWidgetButton"
    case blackOpaqueBackground = "BlackOpaqueBackground"
    case blueBackground = "BlueBackground"
    case toastBackground = "ToastBackground"
    case fadeBackground = "FadeBackground"
    case errorBackground = "ErrorBackground"
    case blackStatusBarBackground = "BlackStatusBarBackground"
    case presentationBackground = "PresentationBackground"
    case clearBackground = "ClearBackground"
    case border = "Border"
    case tabView = "TabView"
    case dialogView = "DialogView"
    case alertView = "AlertView"
    case alertViewTextFieldContainer = "AlertViewTextFieldContainer"
    case alertViewTextField = "AlertViewTextField"
    case searchStatusBarView = "SearchStatusBarView"
    case flatNormalButton = "FlatNormalButton"
    case flatNormalButtonBig = "FlatNormalButtonBig"
    case flatNormalTransButton = "FlatNormalTransButton"
    case flatNormalTransButtonBig = "FlatNormalTransButtonBig"
    case flatGrayTransButton = "FlatGrayTransButton"
    case flatPrimaryTransButton = "FlatPrimaryTransButton"
    case flatRedTransButton = "FlatRedTransButton"
    case flatRedTransButtonBig = "FlatRedTransButtonBig"
    case flatRedButton = "FlatRedButton"
    case moreButton = "MoreButton"
    case editButton = "EditButton"
    case rateAppButton = "RateAppButton"
    case termsOfUseLinkText = "TermsOfUseLinkText"
    case termsOfUseGrayButton = "TermsOfUseGrayButton"
    case badge = "Badge"
    case blue = "MWMBlue"
    case black = "MWMBlack"
    case other = "MWMOther"
    case gray = "MWMGray"
    case separator = "MWMSeparator"
    case white = "MWMWhite"
  }
}

extension StyleSheet.Global: IStyleSheet {
  static func register(theme: Theme, colors: IColors, fonts: IFonts) {
    allCases.forEach { register($0, theme: theme, colors: colors, fonts: fonts) }
  }

  static func register(_ style: StyleSheet.Global, theme: Theme, colors: IColors, fonts: IFonts) {
    switch style {
    case .tableView:
      add { s in
        s.backgroundColor = colors.white
        s.separatorColor = colors.blackDividers
        s.exclusions = [String(describing: UIDatePicker.self)]
      }
    case .tableCell:
      add { s in
        s.backgroundColor = colors.white
        s.fontColor = colors.blackPrimaryText
        s.tintColor = colors.linkBlue
        s.fontColorDetailed = colors.blackSecondaryText
        s.backgroundColorSelected = colors.pressBackground
        s.exclusions = [String(describing: UIDatePicker.self), "_UIActivityUserDefaultsActivityCell"]
      }
    case .tableViewCell:
      add(from: StyleSheet.Global.tableCell) { s in
      }
    case .tableViewHeaderFooterView:
      add { s in
        s.font = fonts.medium14
        s.fontColor = colors.blackSecondaryText
      }
    case .searchBar:
      add { s in
        s.backgroundColor = colors.white
        s.barTintColor = colors.primary
        s.fontColor = colors.blackPrimaryText
        s.fontColorDetailed = UIColor.white
        s.tintColor = colors.blackSecondaryText
      }
    case .navigationBar:
      add { s in
        s.barTintColor = colors.primary
        s.tintColor = colors.whitePrimaryText
        s.backgroundImage = UIImage()
        s.shadowImage = UIImage()
        s.font = fonts.header
        s.fontColor = colors.whitePrimaryText
      }
    case .navigationBarItem:
      add { s in
        s.font = fonts.regular18
        s.fontColor = colors.whitePrimaryText
        s.fontColorDisabled = UIColor.lightGray
        s.fontColorHighlighted = colors.whitePrimaryTextHighlighted
        s.tintColor = colors.whitePrimaryText
      }
    case .checkmark:
      add { s in
        s.onTintColor = colors.linkBlue
        s.offTintColor = colors.blackHintText
      }
    case .switch:
      add { s in
        s.onTintColor = colors.linkBlue
      }
    case .pageControl:
      add { s in
        s.pageIndicatorTintColor = colors.blackHintText
        s.currentPageIndicatorTintColor = colors.blackSecondaryText
        s.backgroundColor = colors.white
      }
    case .starRatingView:
      add { s in
        s.onTintColor = colors.ratingYellow
        s.offTintColor = colors.blackDividers
      }
    case .difficultyView:
      add { s in
        s.colors = [colors.blackSecondaryText, colors.ratingGreen, colors.ratingYellow, colors.ratingRed]
        s.offTintColor = colors.blackSecondaryText
        s.backgroundColor = colors.clear
      }
    case .divider:
      add { s in
        s.backgroundColor = colors.blackDividers
      }
    case .solidDivider:
      add { s in
        s.backgroundColor = colors.solidDividers
      }
    case .background:
      add { s in
        s.backgroundColor = colors.white
        s.backgroundColorSelected = colors.pressBackground
      }
    case .pressBackground:
      add { s in
        s.backgroundColor = colors.pressBackground
      }
    case .primaryBackground:
      add { s in
        s.backgroundColor = colors.primary
      }
    case .secondaryBackground:
      add { s in
        s.backgroundColor = colors.secondary
      }
    case .menuBackground:
      add { s in
        s.backgroundColor = colors.menuBackground
      }
    case .bottomTabBarButton:
      add { s in
        s.backgroundColor = colors.tabBarButtonBackground
        s.tintColor = colors.blackSecondaryText
        s.coloring = MWMButtonColoring.black
        s.cornerRadius = 8
        s.shadowColor = UIColor(0,0,0,alpha20)
        s.shadowOpacity = 1
        s.shadowOffset = CGSize(width: 0, height: 1)
        s.onTintColor = .red
      }
    case .trackRecordingWidgetButton:
      add(from: StyleSheet.Global.bottomTabBarButton) { s in
        s.cornerRadius = 23
      }
    case .blackOpaqueBackground:
      add { s in
        s.backgroundColor = colors.blackOpaque
      }
    case .blueBackground:
      add { s in
        s.backgroundColor = colors.linkBlue
      }
    case .toastBackground:
      add { s in
        s.backgroundColor = colors.toastBackground
      }
    case .fadeBackground:
      add { s in
        s.backgroundColor = colors.fadeBackground
      }
    case .errorBackground:
      add { s in
        s.backgroundColor = colors.errorPink
      }
    case .blackStatusBarBackground:
      add { s in
        s.backgroundColor = colors.blackStatusBarBackground
      }
    case .presentationBackground:
      add { s in
        s.backgroundColor = UIColor.black.withAlphaComponent(alpha40)
      }
    case .clearBackground:
      add { s in
        s.backgroundColor = colors.clear
      }
    case .border:
      add { s in
        s.backgroundColor = colors.border
      }
    case .tabView:
      add { s in
        s.backgroundColor = colors.pressBackground
        s.barTintColor = colors.primary
        s.tintColor = colors.white
        s.fontColor = colors.whitePrimaryText
        s.font = fonts.medium14
      }
    case .dialogView:
      add { s in
        s.cornerRadius = 8
        s.shadowRadius = 2
        s.shadowColor = UIColor(0,0,0,alpha26)
        s.shadowOpacity = 1
        s.shadowOffset = CGSize(width: 0, height: 1)
        s.backgroundColor = colors.white
        s.clip = true
      }
    case .alertView:
      add { s in
        s.cornerRadius = 12
        s.shadowRadius = 6
        s.shadowColor = UIColor(0,0,0,alpha20)
        s.shadowOpacity = 1
        s.shadowOffset = CGSize(width: 0, height: 3)
        s.backgroundColor = colors.alertBackground
        s.clip = true
      }
    case .alertViewTextFieldContainer:
      add { s in
        s.borderColor = colors.blackDividers
        s.borderWidth = 0.5
        s.backgroundColor = colors.white
      }
    case .alertViewTextField:
      add { s in
        s.font = fonts.regular14
        s.fontColor = colors.blackPrimaryText
        s.tintColor = colors.blackSecondaryText
      }
    case .searchStatusBarView:
      add { s in
        s.backgroundColor = colors.primary
        s.shadowRadius = 2
        s.shadowColor = colors.blackDividers
        s.shadowOpacity = 1
        s.shadowOffset = CGSize(width: 0, height: 0)
      }
    case .flatNormalButton:
      add { s in
        s.font = fonts.medium14
        s.cornerRadius = 8
        s.clip = true
        s.fontColor = colors.whitePrimaryText
        s.backgroundColor = colors.linkBlue
        s.fontColorHighlighted = colors.whitePrimaryTextHighlighted
        s.fontColorDisabled = colors.whitePrimaryTextHighlighted
        s.backgroundColorHighlighted = colors.linkBlueHighlighted
      }
    case .flatNormalButtonBig:
      add(from: StyleSheet.Global.flatNormalButton) { s in
        s.font = fonts.regular17
      }
    case .flatNormalTransButton:
      add { s in
        s.font = fonts.medium14
        s.cornerRadius = 8
        s.clip = true
        s.fontColor = colors.linkBlue
        s.backgroundColor = colors.clear
        s.fontColorHighlighted = colors.linkBlueHighlighted
        s.fontColorDisabled = colors.blackHintText
        s.backgroundColorHighlighted = colors.clear
      }
    case .flatNormalTransButtonBig:
      add(from: StyleSheet.Global.flatNormalTransButton) { s in
        s.font = fonts.regular17
      }
    case .flatGrayTransButton:
      add { s in
        s.font = fonts.medium14
        s.fontColor = colors.blackSecondaryText
        s.backgroundColor = colors.clear
        s.fontColorHighlighted = colors.linkBlueHighlighted
      }
    case .flatPrimaryTransButton:
      add { s in
        s.fontColor = colors.blackPrimaryText
        s.backgroundColor = colors.clear
        s.fontColorHighlighted = colors.linkBlueHighlighted
      }
    case .flatRedTransButton:
      add { s in
        s.font = fonts.medium14
        s.fontColor = colors.red
        s.backgroundColor = colors.clear
        s.fontColorHighlighted = colors.red
      }
    case .flatRedTransButtonBig:
      add { s in
        s.font = fonts.regular17
        s.fontColor = colors.red
        s.backgroundColor = colors.clear
        s.fontColorHighlighted = colors.red
      }
    case .flatRedButton:
      add { s in
        s.font = fonts.medium14
        s.cornerRadius = 8
        s.fontColor = colors.whitePrimaryText
        s.backgroundColor = colors.buttonRed
        s.fontColorHighlighted = colors.buttonRedHighlighted
      }
    case .moreButton:
      add { s in
        s.fontColor = colors.linkBlue
        s.fontColorHighlighted = colors.linkBlueHighlighted
        s.backgroundColor = colors.clear
        s.font = fonts.regular16
      }
    case .editButton:
      add { s in
        s.font = fonts.regular14
        s.fontColor = colors.linkBlue
        s.cornerRadius = 8
        s.borderColor = colors.linkBlue
        s.borderWidth = 1
        s.fontColorHighlighted = colors.linkBlueHighlighted
        s.backgroundColor = colors.clear
      }
    case .rateAppButton:
      add { s in
        s.font = fonts.medium17
        s.fontColor = colors.linkBlue
        s.fontColorHighlighted = colors.white
        s.borderColor = colors.linkBlue
        s.cornerRadius = 8
        s.borderWidth = 1
        s.backgroundColor = colors.clear
        s.backgroundColorHighlighted = colors.linkBlue
      }
    case .termsOfUseLinkText:
      add { s in
        s.font = fonts.regular16
        s.fontColor = colors.blackPrimaryText

        s.linkAttributes = [NSAttributedString.Key.font: fonts.regular16,
                            NSAttributedString.Key.foregroundColor: colors.linkBlue,
                            NSAttributedString.Key.underlineColor: UIColor.clear]
        s.textContainerInset = UIEdgeInsets(top: 0, left: 0, bottom: 0, right: 0)
      }
    case .termsOfUseGrayButton:
      add { s in
        s.font = fonts.medium10
        s.fontColor = colors.blackSecondaryText
        s.fontColorHighlighted = colors.blackHintText
      }
    case .badge:
      add { s in
        s.round = true
        s.backgroundColor = colors.downloadBadgeBackground
      }
    case .blue:
      add { s in
        s.tintColor = colors.linkBlue
        s.coloring = MWMButtonColoring.blue
      }
    case .black:
      add { s in
        s.tintColor = colors.blackSecondaryText
        s.coloring = MWMButtonColoring.black
      }
    case .other:
      add { s in
        s.tintColor = colors.white
        s.coloring = MWMButtonColoring.other
      }
    case .gray:
      add { s in
        s.tintColor = colors.blackHintText
        s.coloring = MWMButtonColoring.gray
      }
    case .separator:
      add { s in
        s.tintColor = colors.blackDividers
        s.coloring = MWMButtonColoring.black
      }
    case .white:
      add { s in
        s.tintColor = colors.white
        s.coloring = MWMButtonColoring.white
      }
    }

    // MARK: - Helpers
    func add(_ resolver: @escaping Theme.Resolver) {
      theme.add(styleName: style.rawValue, resolver)
    }
    func add(from: StyleSheet.Global, _ resolver: @escaping Theme.Resolver) {
      theme.add(styleName: style.rawValue, from: from.rawValue, resolver)
    }
  }
}
