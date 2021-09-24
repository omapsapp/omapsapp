#import "MWMHelpController.h"
#import <CoreApi/AppInfo.h>
#import <CoreApi/MWMFrameworkHelper.h>
#import "MWMMailViewController.h"
#import "WebViewController.h"


static NSString * const kiOSEmail = @"ios@organicmaps.app";

@interface MWMHelpController ()<MFMailComposeViewControllerDelegate>

@property(nonatomic) WebViewController * aboutViewController;

@property(weak, nonatomic) IBOutlet UIView * separatorView;

@end  // namespace

@implementation MWMHelpController

- (void)viewDidLoad
{
  [super viewDidLoad];

  self.title = L(@"help");

  NSString * html;
  // TODO: Uncomment when online version will be supported.
//  if ([MWMFrameworkHelper isNetworkConnected]) {
//    NSURL *url = [NSURL URLWithString:@"https://support.omaps.app"];
//    self.aboutViewController = [[WebViewController alloc] initWithUrl:url title:nil];
//  } else {
    NSString *path = [NSBundle.mainBundle pathForResource:@"faq" ofType:@"html"];
    html = [NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil];
    self.aboutViewController = [[WebViewController alloc] initWithHtml:html baseUrl:nil title:nil];
//  }

  self.aboutViewController.openInSafari = NO;
  UIView * aboutView = self.aboutViewController.view;
  [self addChildViewController:self.aboutViewController];
  [self.view addSubview:aboutView];

  aboutView.translatesAutoresizingMaskIntoConstraints = NO;
  NSLayoutConstraint * top = [NSLayoutConstraint constraintWithItem:self.view
                                                          attribute:NSLayoutAttributeTop
                                                          relatedBy:NSLayoutRelationEqual
                                                             toItem:aboutView
                                                          attribute:NSLayoutAttributeTop
                                                         multiplier:1.0
                                                           constant:0.0];
  NSLayoutConstraint * bottom = [NSLayoutConstraint constraintWithItem:self.separatorView
                                                             attribute:NSLayoutAttributeTop
                                                             relatedBy:NSLayoutRelationEqual
                                                                toItem:aboutView
                                                             attribute:NSLayoutAttributeBottom
                                                            multiplier:1.0
                                                              constant:0.0];
  NSLayoutConstraint * left = [NSLayoutConstraint constraintWithItem:self.view
                                                           attribute:NSLayoutAttributeLeft
                                                           relatedBy:NSLayoutRelationEqual
                                                              toItem:aboutView
                                                           attribute:NSLayoutAttributeLeft
                                                          multiplier:1.0
                                                            constant:0.0];
  NSLayoutConstraint * right = [NSLayoutConstraint constraintWithItem:self.view
                                                            attribute:NSLayoutAttributeRight
                                                            relatedBy:NSLayoutRelationEqual
                                                               toItem:aboutView
                                                            attribute:NSLayoutAttributeRight
                                                           multiplier:1.0
                                                             constant:0.0];

  [self.view addConstraints:@[ top, bottom, left, right ]];
}

- (IBAction)reportBug
{
  UIAlertController * alert =
      [UIAlertController alertControllerWithTitle:L(@"feedback")
                                          message:nil
                                   preferredStyle:UIAlertControllerStyleAlert];

  UIAlertAction * commonReport = [UIAlertAction actionWithTitle:L(@"feedback_general")
                                                          style:UIAlertActionStyleDefault
                                                        handler:^(UIAlertAction * _Nonnull action) {
                                                          [self commonReportAction];
                                                        }];
  [alert addAction:commonReport];

  UIAlertAction * bugReport = [UIAlertAction actionWithTitle:L(@"report_a_bug")
                                                       style:UIAlertActionStyleDefault
                                                     handler:^(UIAlertAction * _Nonnull action) {
                                                       [self bugReportAction];
                                                     }];
  [alert addAction:bugReport];

  UIAlertAction * cancel =
      [UIAlertAction actionWithTitle:L(@"cancel") style:UIAlertActionStyleCancel handler:nil];
  [alert addAction:cancel];
  alert.preferredAction = cancel;

  [self presentViewController:alert animated:YES completion:nil];
}

#pragma mark - Actions

- (void)commonReportAction
{
  // Do not localize subject. Support team uses it to filter emails.
  [self sendEmailWithSubject:@"Organic Maps Feedback" toRecipient:kiOSEmail];
}

- (void)bugReportAction
{
  // Do not localize subject. Support team uses it to filter emails.
  [self sendEmailWithSubject:@"Organic Maps Bugreport" toRecipient:kiOSEmail];
}

#pragma mark - Email

- (void)sendEmailWithSubject:(NSString *)subject toRecipient:(NSString *)email
{
  if ([MWMMailViewController canSendMail])
  {
    NSString * deviceModel = [AppInfo sharedInfo].deviceModel;
    NSString * languageCode = NSLocale.preferredLanguages.firstObject;
    NSString * language = [[NSLocale localeWithLocaleIdentifier:kLocaleUsedInSupportEmails]
        displayNameForKey:NSLocaleLanguageCode
                    value:languageCode];
    NSString * locale = [NSLocale.currentLocale objectForKey:NSLocaleCountryCode];
    NSString * country = [[NSLocale localeWithLocaleIdentifier:kLocaleUsedInSupportEmails]
        displayNameForKey:NSLocaleCountryCode
                    value:locale];
    NSString * bundleVersion = [AppInfo sharedInfo].bundleVersion;
    NSString * buildNumber = [AppInfo sharedInfo].buildNumber;
    NSString * text = [NSString stringWithFormat:@"\n\n\n\n- %@ (%@)\n- OMaps %@ (%@)\n- %@/%@",
                                                 deviceModel, UIDevice.currentDevice.systemVersion,
                                                 bundleVersion, buildNumber, language, country];

    MWMMailViewController * vc = [[MWMMailViewController alloc] init];
    vc.mailComposeDelegate = self;
    [vc setSubject:[NSString stringWithFormat:@"[%@ iOS] %@", bundleVersion, subject]];
    [vc setMessageBody:text isHTML:NO];
    [vc setToRecipients:@[ email ]];
    [vc.navigationBar setTintColor:[UIColor whitePrimaryText]];
    [self presentViewController:vc animated:YES completion:nil];
  }
  else
  {
    NSString * text = [NSString stringWithFormat:L(@"email_error_body"), email];
    [[[UIAlertView alloc] initWithTitle:L(@"email_error_title")
                                message:text
                               delegate:nil
                      cancelButtonTitle:L(@"ok")
                      otherButtonTitles:nil] show];
  }
}

- (void)mailComposeController:(MFMailComposeViewController *)controller
          didFinishWithResult:(MFMailComposeResult)result
                        error:(NSError *)error
{
  [self dismissViewControllerAnimated:YES completion:nil];
}

@end
