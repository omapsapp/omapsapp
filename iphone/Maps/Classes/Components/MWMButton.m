#import "MWMButton.h"
#import "SwiftBridge.h"
static NSString * const kDefaultPattern = @"%@_%@";
static NSString * const kHighlightedPattern = @"%@_highlighted_%@";
static NSString * const kSelectedPattern = @"%@_selected_%@";

@implementation MWMButton

- (void)setImageName:(NSString *)imageName
{
  _imageName = imageName;
  [self setDefaultImages];
}

// This method is overridden by MWMButtonRenderer.swift
//- (void)applyTheme
//{
//  [self changeColoringToOpposite];
//  [super applyTheme];
//}

- (void)setColoring:(MWMButtonColoring)coloring
{
  _coloring = coloring;
  [self setEnabled:self.enabled];
}

- (void)changeColoringToOpposite
{
  if (self.coloring == MWMButtonColoringOther)
  {
    if (self.imageName)
    {
      [self setDefaultImages];
      self.imageView.image = [self imageForState:self.state];
    }
    return;
  }
  if (self.state == UIControlStateNormal)
    [self setDefaultTintColor];
  else if (self.state == UIControlStateHighlighted)
    [self setHighlighted:YES];
  else if (self.state == UIControlStateSelected)
    [self setSelected:YES];
}

- (void)setDefaultImages
{
  NSString * postfix = [UIColor isNightMode] ? @"dark" : @"light";
  [self setImage:[UIImage imageNamed:[NSString stringWithFormat:kDefaultPattern, self.imageName, postfix]] forState:UIControlStateNormal];
  [self setImage:[UIImage imageNamed:[NSString stringWithFormat:kHighlightedPattern, self.imageName, postfix]] forState:UIControlStateHighlighted];
  [self setImage:[UIImage imageNamed:[NSString stringWithFormat:kSelectedPattern, self.imageName, postfix]] forState:UIControlStateSelected];
}

- (void)setHighlighted:(BOOL)highlighted
{
  [super setHighlighted:highlighted];
  if (highlighted)
  {
    switch (self.coloring)
    {
      case MWMButtonColoringBlue:
        self.tintColor = [UIColor linkBlueHighlighted];
        break;
      case MWMButtonColoringBlack:
        self.tintColor = [UIColor blackHintText];
        break;
      case MWMButtonColoringGray:
        self.tintColor = [UIColor blackDividers];
        break;
      case MWMButtonColoringWhiteText:
        self.tintColor = [UIColor whitePrimaryTextHighlighted];
        break;
      case MWMButtonColoringRed:
        self.tintColor = [UIColor buttonRed];
        break;
      case MWMButtonColoringWhite:
      case MWMButtonColoringOther:
        break;
    }
  }
  else
  {
    if (self.selected)
      [self setSelected:YES];
    else
      [self setEnabled:self.enabled];
  }
}

- (void)setSelected:(BOOL)selected
{
  [super setSelected:selected];
  if (selected)
  {
    switch (self.coloring)
    {
      case MWMButtonColoringBlack:
        self.tintColor = [UIColor linkBlue];
        break;
      case MWMButtonColoringWhite:
      case MWMButtonColoringWhiteText:
      case MWMButtonColoringBlue:
      case MWMButtonColoringOther:
      case MWMButtonColoringGray:
      case MWMButtonColoringRed:
        break;
    }
  }
  else
  {
    [self setEnabled:self.enabled];
  }
}

- (void)setEnabled:(BOOL)enabled
{
  [super setEnabled:enabled];
  if (!enabled)
    self.tintColor = [UIColor lightGrayColor];
  else
    [self setDefaultTintColor];
}

- (void)setDefaultTintColor
{
  switch (self.coloring)
  {
    case MWMButtonColoringBlack:
      self.tintColor = [UIColor blackSecondaryText];
      break;
    case MWMButtonColoringWhite:
      self.tintColor = [UIColor white];
      break;
    case MWMButtonColoringWhiteText:
      self.tintColor = [UIColor whitePrimaryText];
      break;
    case MWMButtonColoringBlue:
      self.tintColor = [UIColor linkBlue];
      break;
    case MWMButtonColoringGray:
      self.tintColor = [UIColor blackHintText];
      break;
    case MWMButtonColoringRed:
      self.tintColor = [UIColor red];
      break;
    case MWMButtonColoringOther:
      self.imageView.image = [self imageForState:UIControlStateNormal];
      break;
  }
}

@end
