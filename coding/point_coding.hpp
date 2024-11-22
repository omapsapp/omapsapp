#pragma once

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include <cstdint>
#include <utility>

uint8_t constexpr kPointCoordBits = 30;

uint8_t constexpr kFeatureSorterPointCoordBits = 27;

// The absolute precision of the point encoding in the mwm files.
// If both x and y coordinates of two points lie within |kMwmPointAccuracy| of one
// another we consider the points equal. In other words, |kMwmPointAccuracy| may
// be used as the eps value for both x and y in Point::EqualDxDy, AlmostEqualAbs and such.
//
// The constant is loosely tied to mercator::Bounds::kRangeX / (1 << kPointCoordBits):
//   The range of possible values for point coordinates
//      mercator::Bounds::kRangeX = 360.0
//   The number of distinct values for each coordinate after encoding
//      (1 << kPointCoordBits) = 1073741824 ≈ 1e9
//   Distance between two discernible points in the uniform case
//      360.0 / 1e9 ≈ 4e-7 ≈ 0.04 * |kMwmPointAccuracy|.
//
// On the other hand, this should be enough for most purposes because
// 1e-5 difference in the coordinates of a mercator-projected point corresponds to roughly
// 1 meter difference on the equator and we do not expect most OSM points to be mapped
// with better precision.
//
// todo(@m) By this argument, it seems that 1e-6 is a better choice.
//
// Note. generator/feature_sorter.cpp uses |kFeatureSorterPointCoordBits|,
// effectively overriding |kPointCoordBits|. Presumably it does so to guarantee a maximum of
// 4 bytes in the varint encoding, (27+1 sign(?) bit) / 7 = 4.
// todo(@m) Clarify how kPointCoordBits and kFeatureSorterPointCoordBits are related.
double constexpr kMwmPointAccuracy = 1e-5;

uint32_t DoubleToUint32(double x, double min, double max, uint8_t coordBits);

double Uint32ToDouble(uint32_t x, double min, double max, uint8_t coordBits);

m2::PointU PointDToPointU(double x, double y, uint8_t coordBits);

m2::PointU PointDToPointU(m2::PointD const & pt, uint8_t coordBits);

m2::PointU PointDToPointU(m2::PointD const & pt, uint8_t coordBits, m2::RectD const & limitRect);

m2::PointD PointUToPointD(m2::PointU const & p, uint8_t coordBits);

m2::PointD PointUToPointD(m2::PointU const & pt, uint8_t coordBits, m2::RectD const & limitRect);

// Returns coordBits needed to encode point from given rect with given absolute precision.
// If 32 bits are not enough returns 0. It's caller's responsibility to check it.
uint8_t GetCoordBits(m2::RectD const & limitRect, double accuracy);

// All functions below are deprecated and are left
// only for backward compatibility.
//
// Their intention was to store a point with unsigned 32-bit integer
// coordinates to a signed or to an unsigned 64-bit integer by interleaving the
// bits of the point's coordinates.
//
// A possible reason for interleaving is to lower the number of bytes
// needed by the varint encoding, at least if the coordinates are of the
// same order of magnitude. However, this is hard to justify:
// 1. We have no reason to expect the coordinates to be of the same order.
// 2. If you need to serialize a point, doing it separately
//    for each coordinate is almost always a better option.
// 3. If you need to temporarily store the point as an uint,
//    you do not need the complexity of interleaving.
//
// By VNG: Well, for polys delta encoding WriteVarUint(BitwiseMerge(x, y)) is better than
// WriteVarUint(x) + WriteVarUint(y) by 15%. Check CitiesBoundaries_Compression test with World V0 vs V1.
//
// Another possible reason to interleave bits of x and y arises
// when implementing the Z-order curve but we have this
// written elsewhere (see geometry/cellid.hpp).

int64_t PointToInt64Obsolete(double x, double y, uint8_t coordBits);

int64_t PointToInt64Obsolete(m2::PointD const & pt, uint8_t coordBits);

m2::PointD Int64ToPointObsolete(int64_t v, uint8_t coordBits);

std::pair<int64_t, int64_t> RectToInt64Obsolete(m2::RectD const & r, uint8_t coordBits);

m2::RectD Int64ToRectObsolete(std::pair<int64_t, int64_t> const & p, uint8_t coordBits);

uint64_t PointUToUint64Obsolete(m2::PointU const & pt);

m2::PointU Uint64ToPointUObsolete(int64_t v);
