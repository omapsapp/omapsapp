#import "ElevationProfileData+Core.h"

#include "geometry/mercator.hpp"

static ElevationDifficulty convertDifficulty(uint8_t difficulty) {
  switch (difficulty) {
    case ElevationInfo::Difficulty::Easy:
      return ElevationDifficulty::ElevationDifficultyEasy;
    case ElevationInfo::Difficulty::Medium:
      return ElevationDifficulty::ElevationDifficultyMedium;
    case ElevationInfo::Difficulty::Hard:
      return ElevationDifficulty::ElevationDifficultyHard;
    case ElevationInfo::Difficulty::Unknown:
      return ElevationDifficulty::ElevationDifficultyDisabled;
  }
  return ElevationDifficulty::ElevationDifficultyDisabled;
}

@implementation ElevationProfileData

@end

@implementation ElevationProfileData (Core)

- (instancetype)initWithTrackId:(MWMTrackID)trackId
                  elevationInfo:(ElevationInfo const &)elevationInfo
                    activePoint:(double)activePoint
                     myPosition:(double)myPosition {
  self = [super init];
  if (self) {
    _trackId = trackId;
    _difficulty = convertDifficulty(elevationInfo.GetDifficulty());
    _points = [ElevationProfileData pointsFromElevationInfo:elevationInfo];
    _activePoint = activePoint;
    _myPosition = myPosition;
    _isTrackRecording = false;
  }
  return self;
}

- (instancetype)initWithElevationInfo:(ElevationInfo const &)elevationInfo {
  self = [super init];
  if (self) {
    _difficulty = convertDifficulty(elevationInfo.GetDifficulty());
    _points = [ElevationProfileData pointsFromElevationInfo:elevationInfo];
    _isTrackRecording = true;
  }
  return self;
}

+ (NSArray<ElevationHeightPoint *> *)pointsFromElevationInfo:(ElevationInfo const &)elevationInfo {
  auto const & points = elevationInfo.GetPoints();
  NSMutableArray * pointsArray = [[NSMutableArray alloc] initWithCapacity:points.size()];
  for (auto const & point : points) {
    auto pointLatLon = mercator::ToLatLon(point.m_point.GetPoint());
    CLLocationCoordinate2D coordinates = CLLocationCoordinate2DMake(pointLatLon.m_lat, pointLatLon.m_lon);
    ElevationHeightPoint * elevationPoint = [[ElevationHeightPoint alloc] initWithCoordinates:coordinates
                                                                                     distance:point.m_distance
                                                                                  andAltitude:point.m_point.GetAltitude()];
    [pointsArray addObject:elevationPoint];
  }
  return [pointsArray copy];
}

@end
