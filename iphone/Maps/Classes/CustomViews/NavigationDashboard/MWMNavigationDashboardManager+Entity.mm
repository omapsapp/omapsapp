#import "MWMLocationManager.h"
#import "MWMNavigationDashboardEntity.h"
#import "MWMNavigationDashboardManager+Entity.h"
#import "MWMRouter.h"
#import "MWMRouterTransitStepInfo.h"
#import "SwiftBridge.h"

#import <AudioToolbox/AudioServices.h>
#import <CoreApi/Framework.h>
#import <CoreApi/DurationFormatter.h>

#include "routing/following_info.hpp"
#include "routing/turns.hpp"

#include "map/routing_manager.hpp"

#include "platform/location.hpp"

#include "geometry/distance_on_sphere.hpp"

namespace {
UIImage * image(routing::turns::CarDirection t, bool isNextTurn) {
  if (![MWMLocationManager lastLocation])
    return nil;

  using namespace routing::turns;
  NSString * imageName;
  switch (t) {
    case CarDirection::ExitHighwayToRight: imageName = @"ic_exit_highway_to_right"; break;
    case CarDirection::TurnSlightRight: imageName = @"slight_right"; break;
    case CarDirection::TurnRight: imageName = @"simple_right"; break;
    case CarDirection::TurnSharpRight: imageName = @"sharp_right"; break;
    case CarDirection::ExitHighwayToLeft: imageName = @"ic_exit_highway_to_left"; break;
    case CarDirection::TurnSlightLeft: imageName = @"slight_left"; break;
    case CarDirection::TurnLeft: imageName = @"simple_left"; break;
    case CarDirection::TurnSharpLeft: imageName = @"sharp_left"; break;
    case CarDirection::UTurnLeft: imageName = @"uturn_left"; break;
    case CarDirection::UTurnRight: imageName = @"uturn_right"; break;
    case CarDirection::ReachedYourDestination: imageName = @"finish_point"; break;
    case CarDirection::LeaveRoundAbout:
    case CarDirection::EnterRoundAbout: imageName = @"round"; break;
    case CarDirection::GoStraight: imageName = @"straight"; break;
    case CarDirection::StartAtEndOfStreet:
    case CarDirection::StayOnRoundAbout:
    case CarDirection::Count:
    case CarDirection::None: imageName = isNextTurn ? nil : @"straight"; break;
  }
  if (!imageName)
    return nil;
  return [UIImage imageNamed:isNextTurn ? [imageName stringByAppendingString:@"_then"] : imageName];
}

UIImage * image(routing::turns::PedestrianDirection t) {
  if (![MWMLocationManager lastLocation])
    return nil;

  using namespace routing::turns;
  NSString * imageName;
  switch (t) {
    case PedestrianDirection::TurnRight: imageName = @"simple_right"; break;
    case PedestrianDirection::TurnLeft: imageName = @"simple_left"; break;
    case PedestrianDirection::ReachedYourDestination: imageName = @"finish_point"; break;
    case PedestrianDirection::GoStraight:
    case PedestrianDirection::Count:
    case PedestrianDirection::None: imageName = @"straight"; break;
  }
  if (!imageName)
    return nil;
  return [UIImage imageNamed:imageName];
}

NSArray<MWMRouterTransitStepInfo *> *buildRouteTransitSteps(NSArray<MWMRoutePoint *> *points) {
  // Generate step info in format: (Segment 1 distance) (1) (Segment 2 distance) (2) ... (n-1) (Segment N distance).
  NSMutableArray<MWMRouterTransitStepInfo *> * steps = [NSMutableArray arrayWithCapacity:[points count] * 2 - 1];
  auto const numPoints = [points count];
  for (int i = 0; i < numPoints - 1; i++) {
    MWMRoutePoint* segmentStart = points[i];
    MWMRoutePoint* segmentEnd = points[i + 1];
    auto const distance = platform::Distance::CreateFormatted(
        ms::DistanceOnEarth(segmentStart.latitude, segmentStart.longitude, segmentEnd.latitude, segmentEnd.longitude));

    MWMRouterTransitStepInfo* segmentInfo = [[MWMRouterTransitStepInfo alloc] init];
    segmentInfo.type = MWMRouterTransitTypeRuler;
    segmentInfo.distance = @(distance.GetDistanceString().c_str());
    segmentInfo.distanceUnits = @(distance.GetUnitsString().c_str());
    steps[i * 2] = segmentInfo;

    if (i < numPoints - 2) {
      MWMRouterTransitStepInfo* stopInfo = [[MWMRouterTransitStepInfo alloc] init];
      stopInfo.type = MWMRouterTransitTypeIntermediatePoint;
      stopInfo.intermediateIndex = i;
      steps[i * 2 + 1] = stopInfo;
    }
  }

  return steps;
}
}  // namespace

@interface MWMNavigationDashboardEntity ()

@property(copy, nonatomic, readwrite) NSArray<MWMRouterTransitStepInfo *> * transitSteps;
@property(copy, nonatomic, readwrite) NSString * distanceToTurn;
@property(copy, nonatomic, readwrite) NSString * streetName;
@property(copy, nonatomic, readwrite) NSString * targetDistance;
@property(copy, nonatomic, readwrite) NSString * targetUnits;
@property(copy, nonatomic, readwrite) NSString * turnUnits;
@property(nonatomic, readwrite) double speedLimitMps;
@property(nonatomic, readwrite) BOOL isValid;
@property(nonatomic, readwrite) CGFloat progress;
@property(nonatomic, readwrite) NSUInteger roundExitNumber;
@property(nonatomic, readwrite) NSUInteger timeToTarget;
@property(nonatomic, readwrite) UIImage * nextTurnImage;
@property(nonatomic, readwrite) UIImage * turnImage;
@property(nonatomic, readwrite) BOOL showEta;
@property(nonatomic, readwrite) BOOL isWalk;

@end

@implementation MWMNavigationDashboardEntity

- (NSString *)arrival
{
  auto arrivalDate = [[NSDate date] dateByAddingTimeInterval:self.timeToTarget];
  return [DateTimeFormatter dateStringFrom:arrivalDate
                                 dateStyle:NSDateFormatterNoStyle
                                 timeStyle:NSDateFormatterShortStyle];
}

+ (NSAttributedString *)estimateDot
{
  auto attributes = @{
    NSForegroundColorAttributeName: [UIColor blackSecondaryText],
    NSFontAttributeName: [UIFont medium17]
  };
  return [[NSAttributedString alloc] initWithString:@" • " attributes:attributes];
}

- (NSAttributedString *)estimate {
  NSDictionary * primaryAttributes = @{NSForegroundColorAttributeName: [UIColor blackPrimaryText], NSFontAttributeName: [UIFont medium17]};
  NSDictionary * secondaryAttributes = @{NSForegroundColorAttributeName: [UIColor blackSecondaryText], NSFontAttributeName: [UIFont medium17]};

  auto result = [[NSMutableAttributedString alloc] initWithString:@""];
  if (self.showEta) {
    NSString * eta = [DurationFormatter durationStringFromTimeInterval:self.timeToTarget];
    [result appendAttributedString:[[NSMutableAttributedString alloc] initWithString:eta attributes:primaryAttributes]];
    [result appendAttributedString:MWMNavigationDashboardEntity.estimateDot];
  }

  if (self.isWalk) {
    UIFont * font = primaryAttributes[NSFontAttributeName];
    auto textAttachment = [[NSTextAttachment alloc] init];
    auto image = [UIImage imageNamed:@"ic_walk"];
    textAttachment.image = image;
    auto const height = font.lineHeight;
    auto const y = height - image.size.height;
    auto const width = image.size.width * height / image.size.height;
    textAttachment.bounds = CGRectIntegral({{0, y}, {width, height}});

    NSMutableAttributedString * attrStringWithImage =
    [NSAttributedString attributedStringWithAttachment:textAttachment].mutableCopy;
    [attrStringWithImage addAttributes:secondaryAttributes range:NSMakeRange(0, attrStringWithImage.length)];
    [result appendAttributedString:attrStringWithImage];
  }

  auto target = [NSString stringWithFormat:@"%@ %@", self.targetDistance, self.targetUnits];
  [result appendAttributedString:[[NSAttributedString alloc] initWithString:target attributes:secondaryAttributes]];

  return result;
}

@end

@interface MWMRouterTransitStepInfo ()

- (instancetype)initWithStepInfo:(TransitStepInfo const &)info;

@end

@interface MWMNavigationDashboardManager ()

@property(copy, nonatomic) NSDictionary * etaAttributes;
@property(copy, nonatomic) NSDictionary * etaSecondaryAttributes;
@property(nonatomic) MWMNavigationDashboardEntity * entity;

- (void)onNavigationInfoUpdated;

@end

@implementation MWMNavigationDashboardManager (Entity)

- (void)updateFollowingInfo:(routing::FollowingInfo const &)info routePoints:(NSArray<MWMRoutePoint *> *)points type:(MWMRouterType)type {
  if ([MWMRouter isRouteFinished]) {
    [MWMRouter stopRouting];
    AudioServicesPlaySystemSound(kSystemSoundID_Vibrate);
    return;
  }

  if (auto entity = self.entity) {
    BOOL const showEta = (type != MWMRouterTypeRuler);
    
    entity.isValid = YES;
    entity.timeToTarget = info.m_time;
    entity.targetDistance = @(info.m_distToTarget.GetDistanceString().c_str());
    entity.targetUnits = @(info.m_distToTarget.GetUnitsString().c_str());
    entity.progress = info.m_completionPercent;
    entity.distanceToTurn = @(info.m_distToTurn.GetDistanceString().c_str());
    entity.turnUnits = @(info.m_distToTurn.GetUnitsString().c_str());
    entity.streetName = @(info.m_nextStreetName.c_str());
    entity.speedLimitMps = info.m_speedLimitMps;

    entity.isWalk = NO;
    entity.showEta = showEta;

    if (type == MWMRouterTypeRuler && [points count] > 2)
      entity.transitSteps = buildRouteTransitSteps(points);
    else
      entity.transitSteps = [[NSArray alloc] init];

    if (type == MWMRouterTypePedestrian) {
      entity.turnImage = image(info.m_pedestrianTurn);
    } else {
      using namespace routing::turns;
      CarDirection const turn = info.m_turn;
      entity.turnImage = image(turn, false);
      entity.nextTurnImage = image(info.m_nextTurn, true);
      BOOL const isRound = turn == CarDirection::EnterRoundAbout || turn == CarDirection::StayOnRoundAbout ||
                           turn == CarDirection::LeaveRoundAbout;
      if (isRound)
        entity.roundExitNumber = info.m_exitNum;
      else
        entity.roundExitNumber = 0;
    }
  }

  [self onNavigationInfoUpdated];
}

- (void)updateTransitInfo:(TransitRouteInfo const &)info {
  if (auto entity = self.entity) {
    entity.timeToTarget = info.m_totalTimeInSec;
    entity.targetDistance = @(info.m_totalPedestrianDistanceStr.c_str());
    entity.targetUnits = @(info.m_totalPedestrianUnitsSuffix.c_str());
    entity.isValid = YES;
    entity.isWalk = YES;
    entity.showEta = YES;
    NSMutableArray<MWMRouterTransitStepInfo *> * transitSteps = [NSMutableArray new];
    for (auto const &stepInfo : info.m_steps)
      [transitSteps addObject:[[MWMRouterTransitStepInfo alloc] initWithStepInfo:stepInfo]];
    entity.transitSteps = transitSteps;
  }
  [self onNavigationInfoUpdated];
}

@end
