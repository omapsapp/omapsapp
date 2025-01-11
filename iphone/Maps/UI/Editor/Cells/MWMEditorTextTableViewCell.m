#import "MWMEditorTextTableViewCell.h"
#import "MWMEditorCommon.h"
#import "UIImageView+Coloring.h"
#import "SwiftBridge.h"
static CGFloat const kErrorLabelDefaultTopSpace = 4.;

@interface MWMEditorTextTableViewCell ()<UITextFieldDelegate>

@property(weak, nonatomic) IBOutlet UIImageView * icon;
@property(weak, nonatomic, readwrite) IBOutlet UITextField * textField;
@property(weak, nonatomic) IBOutlet UILabel * errorLabel;

@property(weak, nonatomic) IBOutlet NSLayoutConstraint * errorLabelTopSpace;
@property(weak, nonatomic) IBOutlet NSLayoutConstraint * labelHeight;

@property(weak, nonatomic) id<MWMEditorCellProtocol> delegate;

@property(nonatomic) BOOL isValid;

@end

@implementation MWMEditorTextTableViewCell

- (void)configWithDelegate:(id<MWMEditorCellProtocol>)delegate
                      icon:(UIImage *)icon
                      text:(NSString *)text
               placeholder:(NSString *)placeholder
              keyboardType:(UIKeyboardType)keyboardType
            capitalization:(UITextAutocapitalizationType)capitalization
{
  [self configWithDelegate:delegate
                      icon:icon
                      text:text
               placeholder:placeholder
              errorMessage:nil
                   isValid:YES
              keyboardType:keyboardType
            capitalization:capitalization];
}

- (void)configWithDelegate:(id<MWMEditorCellProtocol>)delegate
                      icon:(UIImage *)icon
                      text:(NSString *)text
               placeholder:(NSString *)placeholder
              errorMessage:(NSString *)errorMessage
                   isValid:(BOOL)isValid
              keyboardType:(UIKeyboardType)keyboardType
            capitalization:(UITextAutocapitalizationType)capitalization
{
  self.delegate = delegate;
  self.icon.image = icon;
  [self.icon setStyleNameAndApply: @"MWMBlack"];
  self.icon.hidden = (icon == nil);

  self.textField.text = text;
  self.textField.attributedPlaceholder = [[NSAttributedString alloc]
      initWithString:placeholder
          attributes:@{NSForegroundColorAttributeName : [UIColor blackHintText]}];
  self.errorLabel.text = errorMessage;
  self.textField.keyboardType = keyboardType;
  self.textField.backgroundColor = UIColor.clearColor;
  self.isValid = isValid;
  self.textField.autocapitalizationType = capitalization;
  [self processValidation];
}

- (void)processValidation
{
  if (self.isValid)
  {
    self.labelHeight.priority = UILayoutPriorityDefaultHigh;
    self.errorLabelTopSpace.constant = 0.;
    [self.contentView setStyleNameAndApply: @"Background"];
  }
  else
  {
    self.labelHeight.priority = UILayoutPriorityDefaultLow;
    self.errorLabelTopSpace.constant = kErrorLabelDefaultTopSpace;
    [self.contentView setStyleNameAndApply: @"ErrorBackground"];
  }
  [self layoutIfNeeded];
}

- (void)changeInvalidCellState
{
  if (self.isValid)
    return;
  self.isValid = YES;
  [self processValidation];
  [self.delegate tryToChangeInvalidStateForCell:self];
}

#pragma mark - UITextFieldDelegate

- (BOOL)textField:(UITextField *)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString *)string
{
  [self changeInvalidCellState];
  return YES;
}

- (BOOL)textFieldShouldClear:(UITextField *)textField
{
  [self changeInvalidCellState];
  return YES;
}

- (void)textFieldDidEndEditing:(UITextField *)textField
{
  [self.delegate cell:self changedText:textField.text];
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
  [textField resignFirstResponder];
  return YES;
}

@end
