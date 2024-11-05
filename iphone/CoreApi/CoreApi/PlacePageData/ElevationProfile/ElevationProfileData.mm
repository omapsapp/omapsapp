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
    _ascent = elevationInfo.GetAscent();
    _descent = elevationInfo.GetDescent();
    _maxAttitude = elevationInfo.GetMaxAltitude();
    _minAttitude = elevationInfo.GetMinAltitude();
    _difficulty = convertDifficulty(elevationInfo.GetDifficulty());

    NSMutableArray * pointsArray = [NSMutableArray array];
    for (auto const & point : elevationInfo.GetPoints()) {
      auto pointLatLon = mercator::ToLatLon(point.m_point.GetPoint());
      CLLocationCoordinate2D coordinates = CLLocationCoordinate2DMake(pointLatLon.m_lat, pointLatLon.m_lon);
      ElevationHeightPoint * elevationPoint = [[ElevationHeightPoint alloc] initWithCoordinates:coordinates
                                                                                       distance:point.m_distance
                                                                                    andAltitude:point.m_point.GetAltitude()];
      [pointsArray addObject:elevationPoint];
    }
    _points = [pointsArray copy];
    _activePoint = activePoint;
    _myPosition = myPosition;

    NSMutableArray * segmentDistances = [NSMutableArray array];
    for (auto const & segment : elevationInfo.GetSegmentDistances()) {
      [segmentDistances addObject:[NSNumber numberWithDouble:segment]];
    }
    _segmentDistances = [segmentDistances copy];
  }
  return self;
}


@end
