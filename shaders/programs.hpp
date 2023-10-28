#pragma once

#include "base/assert.hpp"

#include <string>

namespace gpu
{
// Programs in enum are in the order of rendering priority.
enum class Program
{
  ColoredSymbol = 0,
  Texturing,
  MaskedTexturing,
  Bookmark,
  BookmarkAnim,
  TextOutlined,
  Text,
  TextFixed,
  TextStaticOutlinedGui,
  TextOutlinedGui,
  Area,
  AreaOutline,
  Area3d,
  Area3dOutline,
  Line,
  CapJoin,
  TransitCircle,
  DashedLine,
  PathSymbol,
  HatchingArea,
  TexturingGui,
  GuiCircle,
  Ruler,
  Accuracy,
  MyPosition,
  SelectionLine,
  Transit,
  TransitMarker,
  Route,
  RouteDash,
  RouteArrow,
  RouteMarker,
  CirclePoint,
  BookmarkAboveText,
  BookmarkAnimAboveText,
  DebugRect,
  ScreenQuad,
  Arrow3d,
  Arrow3dTextured,
  Arrow3dShadow,
  Arrow3dOutline,
  ColoredSymbolBillboard,
  TexturingBillboard,
  MaskedTexturingBillboard,
  BookmarkBillboard,
  BookmarkAnimBillboard,
  BookmarkAboveTextBillboard,
  BookmarkAnimAboveTextBillboard,
  TextOutlinedBillboard,
  TextBillboard,
  TextFixedBillboard,
  Traffic,
  TrafficLine,
  TrafficCircle,
  SmaaEdges,
  SmaaBlendingWeight,
  SmaaFinal,

  ProgramsCount
};

inline std::string DebugPrint(Program p)
{
  switch (p)
  {
  case Program::ColoredSymbol: return "ColoredSymbol";
  case Program::Texturing: return "Texturing";
  case Program::MaskedTexturing: return "MaskedTexturing";
  case Program::Bookmark: return "Bookmark";
  case Program::BookmarkAnim: return "BookmarkAnim";
  case Program::TextOutlined: return "TextOutlined";
  case Program::Text: return "Text";
  case Program::TextFixed: return "TextFixed";
  case Program::TextStaticOutlinedGui: return "TextStaticOutlinedGui";
  case Program::TextOutlinedGui: return "TextOutlinedGui";
  case Program::Area: return "Area";
  case Program::AreaOutline: return "AreaOutline";
  case Program::Area3d: return "Area3d";
  case Program::Area3dOutline: return "Area3dOutline";
  case Program::Line: return "Line";
  case Program::CapJoin: return "CapJoin";
  case Program::TransitCircle: return "TransitCircle";
  case Program::DashedLine: return "DashedLine";
  case Program::PathSymbol: return "PathSymbol";
  case Program::HatchingArea: return "HatchingArea";
  case Program::TexturingGui: return "TexturingGui";
  case Program::GuiCircle: return "GuiCircle";
  case Program::Ruler: return "Ruler";
  case Program::Accuracy: return "Accuracy";
  case Program::MyPosition: return "MyPosition";
  case Program::SelectionLine: return "SelectionLine";
  case Program::Transit: return "Transit";
  case Program::TransitMarker: return "TransitMarker";
  case Program::Route: return "Route";
  case Program::RouteDash: return "RouteDash";
  case Program::RouteArrow: return "RouteArrow";
  case Program::RouteMarker: return "RouteMarker";
  case Program::CirclePoint: return "CirclePoint";
  case Program::BookmarkAboveText: return "BookmarkAboveText";
  case Program::BookmarkAnimAboveText: return "BookmarkAnimAboveText";
  case Program::DebugRect: return "DebugRect";
  case Program::ScreenQuad: return "ScreenQuad";
  case Program::Arrow3d: return "Arrow3d";
  case Program::Arrow3dTextured: return "Arrow3dTextured";
  case Program::Arrow3dShadow: return "Arrow3dShadow";
  case Program::Arrow3dOutline: return "Arrow3dOutline";
  case Program::ColoredSymbolBillboard: return "ColoredSymbolBillboard";
  case Program::TexturingBillboard: return "TexturingBillboard";
  case Program::MaskedTexturingBillboard: return "MaskedTexturingBillboard";
  case Program::BookmarkBillboard: return "BookmarkBillboard";
  case Program::BookmarkAnimBillboard: return "BookmarkAnimBillboard";
  case Program::BookmarkAboveTextBillboard: return "BookmarkAboveTextBillboard";
  case Program::BookmarkAnimAboveTextBillboard: return "BookmarkAnimAboveTextBillboard";
  case Program::TextOutlinedBillboard: return "TextOutlinedBillboard";
  case Program::TextBillboard: return "TextBillboard";
  case Program::TextFixedBillboard: return "TextFixedBillboard";
  case Program::Traffic: return "Traffic";
  case Program::TrafficLine: return "TrafficLine";
  case Program::TrafficCircle: return "TrafficCircle";
  case Program::SmaaEdges: return "SmaaEdges";
  case Program::SmaaBlendingWeight: return "SmaaBlendingWeight";
  case Program::SmaaFinal: return "SmaaFinal";

  case Program::ProgramsCount:
    CHECK(false, ("Try to output ProgramsCount"));
  }
  CHECK(false, ("Unknown program"));
  return {};
}
}  // namespace gpu
