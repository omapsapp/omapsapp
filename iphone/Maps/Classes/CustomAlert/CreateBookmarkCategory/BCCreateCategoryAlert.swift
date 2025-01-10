@objc(MWMBCCreateCategoryAlert)
final class BCCreateCategoryAlert: MWMAlert {
  private enum State {
    case valid
    case tooFewSymbols
    case tooManySymbols
    case nameAlreadyExists
  }

  @IBOutlet private(set) weak var textField: UITextField!
  @IBOutlet private weak var titleLabel: UILabel!
  @IBOutlet private weak var textFieldContainer: UIView!
  @IBOutlet private weak var centerHorizontaly: NSLayoutConstraint!
  @IBOutlet private weak var errorLabel: UILabel!
  @IBOutlet private weak var charactersCountLabel: UILabel!
  @IBOutlet private weak var rightButton: UIButton!

  private var maxCharactersNum: UInt?
  private var minCharactersNum: UInt?
  private var callback: MWMCheckStringBlock?

  @objc static func alert(maxCharachersNum: UInt,
                    minCharactersNum: UInt,
                    callback: @escaping MWMCheckStringBlock) -> BCCreateCategoryAlert? {
    guard let alert = Bundle.main.loadNibNamed(className(), owner: nil, options: nil)?.first
          as? BCCreateCategoryAlert else {
      assertionFailure()
      return nil
    }

    alert.titleLabel.text = L("bookmarks_create_new_group")
    let text = L("create").capitalized
    for s in [.normal, .highlighted, .disabled] as [UIControl.State] {
      alert.rightButton.setTitle(text, for: s)
    }

    alert.maxCharactersNum = maxCharachersNum
    alert.minCharactersNum = minCharactersNum
    alert.callback = callback
    alert.process(state: .tooFewSymbols)
    alert.formatCharactersCountText()
    MWMKeyboard.add(alert)
    return alert
  }

  @IBAction private func leftButtonTap() {
    MWMKeyboard.remove(self)
    close(nil)
  }

  @IBAction private func rightButtonTap() {
    guard let callback = callback, let text = textField.text else {
      assertionFailure()
      return
    }

    if callback(text) {
      MWMKeyboard.remove(self)
      close(nil)
    } else {
      process(state: .nameAlreadyExists)
    }
  }
  
  @IBAction private func editingChanged(sender: UITextField) {
    formatCharactersCountText()
    process(state: checkState())
  }

  private func checkState() -> State {
    let count = textField.text!.count
    if count <= minCharactersNum! {
      return .tooFewSymbols
    }

    if count > maxCharactersNum! {
      return .tooManySymbols
    }

    return .valid
  }

  private func process(state: State) {
    let styleName: String
    switch state {
    case .valid:
      styleName = "blackHintText"
      rightButton.isEnabled = true
      errorLabel.isHidden = true
    case .tooFewSymbols:
      styleName = "blackHintText"
      errorLabel.isHidden = true
      rightButton.isEnabled = false
    case .tooManySymbols:
      styleName = "buttonRedText"
      errorLabel.isHidden = false
      errorLabel.text = L("bookmarks_error_title_list_name_too_long")
      rightButton.isEnabled = false
    case .nameAlreadyExists:
      styleName = "buttonRedText"
      errorLabel.isHidden = false
      errorLabel.text = L("bookmarks_error_title_list_name_already_taken")
      rightButton.isEnabled = false
    }

    charactersCountLabel.setStyleNameAndApply(styleName)
    textFieldContainer.layer.borderColor = charactersCountLabel.textColor.cgColor
  }

  private func formatCharactersCountText() {
    let count = textField.text!.count
    charactersCountLabel.text = String(count) + " / " + String(maxCharactersNum!)
  }
}

extension BCCreateCategoryAlert: UITextFieldDelegate {
  func textFieldShouldReturn(_ textField: UITextField) -> Bool {
    if checkState() == .valid {
      rightButtonTap()
    }

    return true
  }

  func textField(_ textField: UITextField,
                 shouldChangeCharactersIn range: NSRange,
                 replacementString string: String) -> Bool {
    let str = textField.text as NSString?
    let newStr = str?.replacingCharacters(in: range, with: string)
    guard let count = newStr?.count else {
      return true
    }

    if count > maxCharactersNum! + 1 {
      return false
    }

    return true
  }
}

extension BCCreateCategoryAlert: MWMKeyboardObserver {
  func onKeyboardAnimation() {
    centerHorizontaly.constant = -MWMKeyboard.keyboardHeight() / 2
    layoutIfNeeded()
  }

  func onKeyboardWillAnimate() {
    setNeedsLayout()
  }
}
