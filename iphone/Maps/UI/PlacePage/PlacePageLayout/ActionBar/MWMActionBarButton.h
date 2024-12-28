typedef NS_ENUM(NSInteger, MWMActionBarButtonType) {
  MWMActionBarButtonTypeBooking,
  MWMActionBarButtonTypeBookingSearch,
  MWMActionBarButtonTypeBookmark,
  MWMActionBarButtonTypeTrack,
  MWMActionBarButtonTypeCall,
  MWMActionBarButtonTypeDownload,
  MWMActionBarButtonTypeMore,
  MWMActionBarButtonTypeOpentable,
  MWMActionBarButtonTypeRouteAddStop,
  MWMActionBarButtonTypeRouteFrom,
  MWMActionBarButtonTypeRouteRemoveStop,
  MWMActionBarButtonTypeRouteTo,
  MWMActionBarButtonTypeAvoidToll,
  MWMActionBarButtonTypeAvoidDirty,
  MWMActionBarButtonTypeAvoidFerry
} NS_SWIFT_NAME(ActionBarButtonType);

typedef NS_ENUM(NSInteger, MWMBookmarksButtonState) {
  MWMBookmarksButtonStateSave,
  MWMBookmarksButtonStateDelete,
  MWMBookmarksButtonStateRecover,
};

NS_ASSUME_NONNULL_BEGIN

#ifdef __cplusplus
extern "C" {
#endif
NSString * titleForButton(MWMActionBarButtonType type, BOOL isSelected);
#ifdef __cplusplus
}
#endif

@class MWMActionBarButton;
@class MWMCircularProgress;

NS_SWIFT_NAME(ActionBarButtonDelegate)
@protocol MWMActionBarButtonDelegate <NSObject>

- (void)tapOnButtonWithType:(MWMActionBarButtonType)type;

@end

NS_SWIFT_NAME(ActionBarButton)
@interface MWMActionBarButton : UIView

@property(nonatomic, readonly) MWMActionBarButtonType type;
@property(nonatomic, readonly, nullable) MWMCircularProgress *mapDownloadProgress;

+ (MWMActionBarButton *)buttonWithDelegate:(id<MWMActionBarButtonDelegate>)delegate
                                buttonType:(MWMActionBarButtonType)type
                                isSelected:(BOOL)isSelected
                                isEnabled:(BOOL)isEnabled;

- (void)setBookmarkButtonState:(MWMBookmarksButtonState)state;

@end

NS_ASSUME_NONNULL_END
