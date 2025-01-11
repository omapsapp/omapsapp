#import <Foundation/Foundation.h>

@class PlacePageScheduleData;
@class TrackInfo;

typedef NS_ENUM(NSInteger, PlacePageDataHotelType) {
  PlacePageDataHotelTypeHotel,
  PlacePageDataHotelTypeApartment,
  PlacePageDataHotelTypeCampSite,
  PlacePageDataHotelTypeChalet,
  PlacePageDataHotelTypeGuestHouse,
  PlacePageDataHotelTypeHostel,
  PlacePageDataHotelTypeMotel,
  PlacePageDataHotelTypeResort,
  PlacePageDataHotelTypeNone
};

typedef NS_ENUM(NSInteger, PlacePageDataOpeningHours) {
  PlacePageDataOpeningHoursUnknown,
  PlacePageDataOpeningHoursAllDay,
  PlacePageDataOpeningHoursOpen,
  PlacePageDataOpeningHoursClosed
};

typedef struct {
  PlacePageDataOpeningHours state;
  time_t nextTimeOpen;
  time_t nextTimeClosed;
} PlacePageDataSchedule;

NS_ASSUME_NONNULL_BEGIN

@interface PlacePagePreviewData : NSObject

@property(nonatomic, readonly, nullable) NSString *title;
@property(nonatomic, readonly, nullable) NSString *secondaryTitle;
@property(nonatomic, readonly, nullable) NSString *subtitle;
@property(nonatomic, readonly, nullable) NSString *coordinates;
@property(nonatomic, readonly, nullable) NSString *secondarySubtitle;
@property(nonatomic, readonly) PlacePageDataSchedule schedule;
@property(nonatomic, readonly) BOOL isMyPosition;

- (instancetype)initWithTrackInfo:(TrackInfo * _Nonnull)trackInfo;

@end

NS_ASSUME_NONNULL_END
