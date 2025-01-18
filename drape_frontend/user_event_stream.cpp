#include "drape_frontend/user_event_stream.hpp"
#include "drape_frontend/animation/follow_animation.hpp"
#include "drape_frontend/animation/linear_animation.hpp"
#include "drape_frontend/animation/scale_animation.hpp"
#include "drape_frontend/animation/parallel_animation.hpp"
#include "drape_frontend/animation/sequence_animation.hpp"
#include "drape_frontend/animation_constants.hpp"
#include "drape_frontend/animation_system.hpp"
#include "drape_frontend/animation_utils.hpp"
#include "drape_frontend/screen_animations.hpp"
#include "drape_frontend/screen_operations.hpp"
#include "drape_frontend/visual_params.hpp"

#include "platform/platform.hpp"

#include "base/macros.hpp"

#include <cmath>

#ifdef DEBUG
#define TEST_CALL(action) if (m_testFn) m_testFn(action)
#else
#define TEST_CALL(action)
#endif


namespace df
{
namespace
{
uint64_t constexpr kDoubleTapPauseMs = 250;
uint64_t constexpr kLongTouchMs = 500;
uint64_t constexpr kKineticDelayMs = 500;

float constexpr kForceTapThreshold = 0.75;

size_t GetValidTouchesCount(std::array<Touch, 2> const & touches)
{
  size_t result = 0;
  if (touches[0].m_id != -1)
    ++result;
  if (touches[1].m_id != -1)
    ++result;

  return result;
}
}  // namespace

#ifdef DEBUG
char const * UserEventStream::BEGIN_DRAG = "BeginDrag";
char const * UserEventStream::DRAG = "Drag";
char const * UserEventStream::END_DRAG = "EndDrag";
char const * UserEventStream::BEGIN_SCALE = "BeginScale";
char const * UserEventStream::SCALE = "Scale";
char const * UserEventStream::END_SCALE = "EndScale";
char const * UserEventStream::BEGIN_TAP_DETECTOR = "BeginTap";
char const * UserEventStream::LONG_TAP_DETECTED = "LongTap";
char const * UserEventStream::SHORT_TAP_DETECTED = "ShortTap";
char const * UserEventStream::CANCEL_TAP_DETECTOR = "CancelTap";
char const * UserEventStream::TRY_FILTER = "TryFilter";
char const * UserEventStream::END_FILTER = "EndFilter";
char const * UserEventStream::CANCEL_FILTER = "CancelFilter";
char const * UserEventStream::TWO_FINGERS_TAP = "TwoFingersTap";
char const * UserEventStream::BEGIN_DOUBLE_TAP_AND_HOLD = "BeginDoubleTapAndHold";
char const * UserEventStream::DOUBLE_TAP_AND_HOLD = "DoubleTapAndHold";
char const * UserEventStream::END_DOUBLE_TAP_AND_HOLD = "EndDoubleTapAndHold";
#endif


void TouchEvent::SetFirstTouch(const Touch & touch)
{
  m_touches[0] = touch;
}

void TouchEvent::SetSecondTouch(const Touch & touch)
{
  m_touches[1] = touch;
}

void TouchEvent::PrepareTouches(std::array<Touch, 2> const & previousTouches)
{
  if (GetValidTouchesCount(m_touches) == 2 && GetValidTouchesCount(previousTouches) > 0)
  {
    if (previousTouches[0].m_id == m_touches[1].m_id)
      Swap();
  }
}

void TouchEvent::SetFirstMaskedPointer(uint8_t firstMask)
{
  m_pointersMask = (m_pointersMask & 0xFF00) | static_cast<uint16_t>(firstMask);
}

uint8_t TouchEvent::GetFirstMaskedPointer() const
{
  return static_cast<uint8_t>(m_pointersMask & 0xFF);
}

void TouchEvent::SetSecondMaskedPointer(uint8_t secondMask)
{
  ASSERT(secondMask == INVALID_MASKED_POINTER || GetFirstMaskedPointer() != INVALID_MASKED_POINTER, ());
  m_pointersMask = (static_cast<uint16_t>(secondMask) << 8) | (m_pointersMask & 0xFF);
}

uint8_t TouchEvent::GetSecondMaskedPointer() const
{
  return static_cast<uint8_t>((m_pointersMask & 0xFF00) >> 8);
}

size_t TouchEvent::GetMaskedCount()
{
  return static_cast<size_t>(GetFirstMaskedPointer() != INVALID_MASKED_POINTER) +
         static_cast<size_t>(GetSecondMaskedPointer() != INVALID_MASKED_POINTER);
}

void TouchEvent::Swap()
{
  auto swapIndex = [](uint8_t index) -> uint8_t
  {
    if (index == INVALID_MASKED_POINTER)
      return index;

    return index ^ 0x1;
  };

  std::swap(m_touches[0], m_touches[1]);
  SetFirstMaskedPointer(swapIndex(GetFirstMaskedPointer()));
  SetSecondMaskedPointer(swapIndex(GetSecondMaskedPointer()));
}

std::string DebugPrint(Touch const & t)
{
  return DebugPrint(t.m_location) + "; " + std::to_string(t.m_id) + "; " + std::to_string(t.m_force);
}

std::string DebugPrint(TouchEvent const & e)
{
  return std::to_string(e.m_type) + "; { " + DebugPrint(e.m_touches[0]) + " }";
}


UserEventStream::UserEventStream()
  : m_state(STATE_EMPTY)
  , m_animationSystem(AnimationSystem::Instance())
  , m_startDragOrg(m2::PointD::Zero())
  , m_startDoubleTapAndHold(m2::PointD::Zero())
  , m_dragThreshold(base::Pow2(VisualParams::Instance().GetDragThreshold()))
{
}

void UserEventStream::AddEvent(drape_ptr<UserEvent> && event)
{
  std::lock_guard guard(m_lock);
  m_events.emplace_back(std::move(event));
}

ScreenBase const & UserEventStream::ProcessEvents(bool & modelViewChanged, bool & viewportChanged, 
                                                  bool & activeFrame)
{
  TEventsList events;
  {
    std::lock_guard guard(m_lock);
    std::swap(m_events, events);
  }

  m2::RectD const prevPixelRect = GetCurrentScreen().PixelRect();

  activeFrame = false;
  viewportChanged = false;
  m_modelViewChanged = !events.empty() || m_state == STATE_SCALE || m_state == STATE_DRAG;

  for (auto const & e : events)
  {
    bool breakAnim = false;

    switch (e->GetType())
    {
    case UserEvent::EventType::Scale:
      {
        ref_ptr<ScaleEvent> scaleEvent = make_ref(e);
        breakAnim = OnSetScale(scaleEvent);
        TouchCancel(m_touches);
      }
      break;
    case UserEvent::EventType::Move:
      {
        ref_ptr<MoveEvent> moveEvent = make_ref(e);
        breakAnim = OnMove(moveEvent);
        TouchCancel(m_touches);
      }
      break;
    case UserEvent::EventType::Resize:
      {
        ref_ptr<ResizeEvent> resizeEvent = make_ref(e);
        m_navigator.OnSize(resizeEvent->GetWidth(), resizeEvent->GetHeight());
        if (!m_visibleViewport.IsValid())
          m_visibleViewport = m2::RectD(0, 0, resizeEvent->GetWidth(), resizeEvent->GetHeight());
        viewportChanged = true;
        breakAnim = true;
        TouchCancel(m_touches);
        if (m_state == STATE_DOUBLE_TAP_HOLD)
          EndDoubleTapAndHold(m_touches[0]);
      }
      break;
    case UserEvent::EventType::SetAnyRect:
      {
        m_needTrackCenter = false;
        ref_ptr<SetAnyRectEvent> anyRectEvent = make_ref(e);
        breakAnim = OnSetAnyRect(anyRectEvent);
        TouchCancel(m_touches);
      }
      break;
    case UserEvent::EventType::SetRect:
      {
        m_needTrackCenter = false;
        ref_ptr<SetRectEvent> rectEvent = make_ref(e);
        breakAnim = OnSetRect(rectEvent);
        TouchCancel(m_touches);
      }
      break;
    case UserEvent::EventType::SetCenter:
      {
        m_needTrackCenter = false;
        ref_ptr<SetCenterEvent> centerEvent = make_ref(e);
        breakAnim = OnSetCenter(centerEvent);
        TouchCancel(m_touches);
      }
      break;
    case UserEvent::EventType::Touch:
      {
        m_needTrackCenter = false;
        ref_ptr<TouchEvent> touchEvent = make_ref(e);
        breakAnim = ProcessTouch(*touchEvent.get());
      }
      break;
    case UserEvent::EventType::Rotate:
      {
        m_needTrackCenter = false;
        ref_ptr<RotateEvent> rotateEvent = make_ref(e);
        breakAnim = OnRotate(rotateEvent);
      }
      break;
    case UserEvent::EventType::FollowAndRotate:
      {
        m_needTrackCenter = false;
        ref_ptr<FollowAndRotateEvent> followEvent = make_ref(e);
        breakAnim = SetFollowAndRotate(followEvent->GetUserPos(), followEvent->GetPixelZero(),
                                       followEvent->GetAzimuth(), followEvent->GetPreferredZoomLelel(),
                                       followEvent->GetAutoScale(), followEvent->IsAnim(), followEvent->IsAutoScale(),
                                       followEvent->GetOnFinishAction(), followEvent->GetParallelAnimCreator());
      }
      break;
    case UserEvent::EventType::AutoPerspective:
      {
        m_needTrackCenter = false;
        ref_ptr<SetAutoPerspectiveEvent> perspectiveEvent = make_ref(e);
        SetAutoPerspective(perspectiveEvent->IsAutoPerspective());
      }
      break;
    case UserEvent::EventType::VisibleViewport:
      {
        ref_ptr<SetVisibleViewportEvent> viewportEvent = make_ref(e);
        breakAnim = OnNewVisibleViewport(viewportEvent);
      }
      break;
    case UserEvent::EventType::Scroll:
      {
        ref_ptr<ScrollEvent> scrollEvent = make_ref(e);
        breakAnim = OnScroll(scrollEvent);
        TouchCancel(m_touches);
      }
      break;
    case UserEvent::EventType::ActiveFrame:
      {
        activeFrame = true;
      }
      break;
    default:
      ASSERT(false, ());
      break;
    }

    if (breakAnim)
      m_modelViewChanged = true;
  }

  ApplyAnimations();

  if (GetValidTouchesCount(m_touches) == 1)
  {
    if (m_state == STATE_WAIT_DOUBLE_TAP)
      DetectShortTap(m_touches[0]);
    else if (m_state == STATE_TAP_DETECTION)
      DetectLongTap(m_touches[0]);
  }

  if (m_modelViewChanged)
    m_animationSystem.UpdateLastScreen(GetCurrentScreen());

  modelViewChanged = m_modelViewChanged;
  m_modelViewChanged = false;

  if (!viewportChanged)
  {
    double constexpr kEps = 1e-5;
    viewportChanged = !m2::IsEqualSize(prevPixelRect, GetCurrentScreen().PixelRect(), kEps, kEps);
  }

  return m_navigator.Screen();
}

void UserEventStream::ApplyAnimations()
{
  if (m_animationSystem.AnimationExists(Animation::Object::MapPlane))
  {
    ScreenBase screen;
    if (m_animationSystem.GetScreen(GetCurrentScreen(), screen))
      m_navigator.SetFromScreen(screen);

    m_modelViewChanged = true;
  }
}

ScreenBase const & UserEventStream::GetCurrentScreen() const
{
  return m_navigator.Screen();
}

m2::RectD const & UserEventStream::GetVisibleViewport() const
{
  return m_visibleViewport;
}

bool UserEventStream::OnSetScale(ref_ptr<ScaleEvent> scaleEvent)
{
  double factor = scaleEvent->GetFactor();

  m2::PointD scaleCenter = scaleEvent->GetPxPoint();
  if (m_listener)
    m_listener->CorrectScalePoint(scaleCenter);

  if (scaleEvent->IsAnim())
  {
    auto followAnim = m_animationSystem.FindAnimation<MapFollowAnimation>(Animation::Type::MapFollow);
    if (followAnim == nullptr)
    {
      auto const parallelAnim = m_animationSystem.FindAnimation<ParallelAnimation>(
          Animation::Type::Parallel, kParallelFollowAnim.c_str());
      if (parallelAnim != nullptr)
        followAnim = parallelAnim->FindAnimation<MapFollowAnimation>(Animation::Type::MapFollow);
    }
    if (followAnim != nullptr && followAnim->HasScale())
    {
      // Scaling is not possible if current follow animation does pixel offset.
      if (followAnim->HasPixelOffset() && !followAnim->IsAutoZoom())
        return false;

      // Reset follow animation with scaling if we apply scale explicitly.
      ResetAnimations(Animation::Type::MapFollow);
      ResetAnimations(Animation::Type::Parallel, kParallelFollowAnim);
    }

    m2::PointD glbScaleCenter = m_navigator.PtoG(m_navigator.P3dtoP(scaleCenter));
    if (m_listener)
      m_listener->CorrectGlobalScalePoint(glbScaleCenter);

    ScreenBase const & startScreen = GetCurrentScreen();

    auto anim = GetScaleAnimation(startScreen, scaleCenter, glbScaleCenter, factor);
    anim->SetOnFinishAction([this](ref_ptr<Animation> animation)
    {
      if (m_listener)
        m_listener->OnAnimatedScaleEnded();
    });

    m_animationSystem.CombineAnimation(std::move(anim));
    return false;
  }

  ResetMapPlaneAnimations();

  m_navigator.Scale(scaleCenter, factor);
  if (m_listener)
    m_listener->OnAnimatedScaleEnded();

  return true;
}

bool UserEventStream::OnMove(ref_ptr<MoveEvent> moveEvent)
{
  double const factorX = moveEvent->GetFactorX();
  double const factorY = moveEvent->GetFactorY();

  ScreenBase screen;
  GetTargetScreen(screen);
  auto const & rect = screen.PixelRectIn3d();
  screen.Move(factorX * rect.SizeX(), -factorY * rect.SizeY());

  ShrinkAndScaleInto(screen, df::GetWorldRect());

  return SetScreen(screen, moveEvent->IsAnim());
}

bool UserEventStream::OnSetAnyRect(ref_ptr<SetAnyRectEvent> anyRectEvent)
{
  return SetRect(anyRectEvent->GetRect(), anyRectEvent->IsAnim(), anyRectEvent->FitInViewport(),
                 anyRectEvent->UseVisibleViewport());
}

bool UserEventStream::OnSetRect(ref_ptr<SetRectEvent> rectEvent)
{
  return SetRect(rectEvent->GetRect(), rectEvent->GetZoom(), rectEvent->GetApplyRotation(),
                 rectEvent->IsAnim(), rectEvent->UseVisibleViewport(),
                 rectEvent->GetParallelAnimCreator());
}

bool UserEventStream::OnSetCenter(ref_ptr<SetCenterEvent> centerEvent)
{
  m2::PointD const & center = centerEvent->GetCenter();
  auto const zoom = centerEvent->GetZoom();
  auto const scaleFactor = centerEvent->GetScaleFactor();

  if (centerEvent->TrackVisibleViewport())
  {
    m_needTrackCenter = true;
    m_trackedCenter = center;
  }

  ScreenBase screen = GetCurrentScreen();

  if (zoom != kDoNotChangeZoom)
  {
    screen.SetFromParams(center, screen.GetAngle(), GetScreenScale(zoom));
    screen.MatchGandP3d(center, m_visibleViewport.Center());
  }
  else if (scaleFactor > 0.0)
  {
    screen.SetOrg(center);
    ApplyScale(m_visibleViewport.Center(), scaleFactor, screen);
  }
  else
  {
    GetTargetScreen(screen);
    screen.MatchGandP3d(center, m_visibleViewport.Center());
  }

  ShrinkAndScaleInto(screen, df::GetWorldRect());

  return SetScreen(screen, centerEvent->IsAnim(), centerEvent->GetParallelAnimCreator());
}

bool UserEventStream::OnRotate(ref_ptr<RotateEvent> rotateEvent)
{
  return SetAngle(rotateEvent->GetTargetAzimuth(), rotateEvent->IsAnim(), rotateEvent->GetParallelAnimCreator());
}

bool UserEventStream::OnNewVisibleViewport(ref_ptr<SetVisibleViewportEvent> viewportEvent)
{
  m2::RectD const prevVisibleViewport = m_visibleViewport;
  m_visibleViewport = viewportEvent->GetRect();
  m2::PointD gOffset;
  ScreenBase screen;

  auto const hasOffset = m_listener->OnNewVisibleViewport(prevVisibleViewport, m_visibleViewport,
                                                          !m_needTrackCenter, gOffset);

  if (m_needTrackCenter)
  {
    GetTargetScreen(screen);
    screen.MatchGandP3d(m_trackedCenter, m_visibleViewport.Center());
    ShrinkAndScaleInto(screen, df::GetWorldRect());
    return SetScreen(screen, true /* isAnim */);
  }
  else if (hasOffset)
  {
    screen = GetCurrentScreen();
    screen.MoveG(gOffset);
    return SetScreen(screen, true /* isAnim */);
  }
  return false;
}

bool UserEventStream::OnScroll(ref_ptr<ScrollEvent> scrollEvent)
{
  double const distanceX = scrollEvent->GetDistanceX();
  double const distanceY = scrollEvent->GetDistanceY();

  ScreenBase screen;
  GetTargetScreen(screen);
  screen.Move(-distanceX, -distanceY);

  ShrinkAndScaleInto(screen, df::GetWorldRect());

  if (m_listener)
    m_listener->OnScrolled({-distanceX, -distanceY});

  return SetScreen(screen, false);
}

bool UserEventStream::SetAngle(double azimuth, bool isAnim, TAnimationCreator const & parallelAnimCreator)
{
  ScreenBase screen;
  GetTargetScreen(screen);
  m2::PointD pt = m_visibleViewport.Center();
  m2::PointD gPt = screen.PtoG(screen.P3dtoP(pt));

  if (screen.isPerspective())
  {
    return SetFollowAndRotate(gPt, pt,
                              azimuth, kDoNotChangeZoom, kDoNotAutoZoom,
                              isAnim, false /* isAutoScale */,
                              nullptr /* onFinishAction */,
                              parallelAnimCreator);
  }

  screen.SetAngle(azimuth);
  screen.MatchGandP3d(gPt, pt);
  return SetScreen(screen, isAnim, parallelAnimCreator);
}

bool UserEventStream::SetRect(m2::RectD rect, int zoom, bool applyRotation, bool isAnim,
                              bool useVisibleViewport, TAnimationCreator const & parallelAnimCreator)
{
  CheckMinGlobalRect(rect, kDefault3dScale);
  CheckMinMaxVisibleScale(rect, zoom, kDefault3dScale);
  m2::AnyRectD targetRect = applyRotation ? ToRotated(m_navigator, rect) : m2::AnyRectD(rect);
  return SetRect(targetRect, isAnim, true /* fitInViewport */, useVisibleViewport, parallelAnimCreator);
}

bool UserEventStream::SetRect(m2::AnyRectD const & rect, bool isAnim, bool fitInViewport,
                              bool useVisibleViewport, TAnimationCreator const & parallelAnimCreator)
{
  ScreenBase tmp = GetCurrentScreen();
  if (fitInViewport)
  {
    auto viewportRect = tmp.PixelRectIn3d();
    if (useVisibleViewport && m_visibleViewport.IsValid())
      viewportRect = m_visibleViewport;
    tmp.SetFromRects(rect, viewportRect);
    tmp.MatchGandP3d(rect.GlobalCenter(), viewportRect.Center());
  }
  else
  {
    tmp.SetFromRects(rect, tmp.PixelRect());
  }
  return SetScreen(tmp, isAnim, parallelAnimCreator);
}

bool UserEventStream::SetScreen(ScreenBase const & endScreen, bool isAnim,
                                TAnimationCreator const & parallelAnimCreator)
{
  if (isAnim)
  {
    ScreenBase const & screen = GetCurrentScreen();

    drape_ptr<Animation> anim = GetRectAnimation(screen, endScreen);
    if (!df::IsAnimationAllowed(anim->GetDuration(), screen))
    {
      anim.reset();
      double const moveDuration = PositionInterpolator::GetMoveDuration(screen.GetOrg(),
                                                                        endScreen.GetOrg(), screen);
      if (moveDuration > kMaxAnimationTimeSec)
        anim = GetPrettyMoveAnimation(screen, endScreen);
    }
    else
    {
      anim->SetMaxDuration(kMaxAnimationTimeSec);
    }

    if (anim != nullptr)
    {
      if (parallelAnimCreator != nullptr)
      {
        drape_ptr<ParallelAnimation> parallelAnim = make_unique_dp<ParallelAnimation>();
        parallelAnim->SetCustomType(kParallelLinearAnim);
        parallelAnim->AddAnimation(parallelAnimCreator(nullptr /* syncAnim */));
        parallelAnim->AddAnimation(std::move(anim));
        m_animationSystem.CombineAnimation(std::move(parallelAnim));
      }
      else
      {
        m_animationSystem.CombineAnimation(std::move(anim));
      }
      return false;
    }
  }

  ResetMapPlaneAnimations();
  m_navigator.SetFromScreen(endScreen);
  return true;
}

bool UserEventStream::InterruptFollowAnimations(bool force)
{
  Animation const * followAnim = m_animationSystem.FindAnimation<MapFollowAnimation>(Animation::Type::MapFollow);

  if (followAnim == nullptr)
    followAnim = m_animationSystem.FindAnimation<SequenceAnimation>(Animation::Type::Sequence, kPrettyFollowAnim.c_str());

  if (followAnim == nullptr)
    followAnim = m_animationSystem.FindAnimation<ParallelAnimation>(Animation::Type::Parallel, kParallelFollowAnim.c_str());

  if (followAnim == nullptr)
    followAnim = m_animationSystem.FindAnimation<ParallelAnimation>(Animation::Type::Parallel, kParallelLinearAnim.c_str());

  if (followAnim != nullptr)
  {
    if (force || followAnim->CouldBeInterrupted())
      ResetAnimations(followAnim->GetType(), followAnim->GetCustomType(), !followAnim->CouldBeInterrupted());
    else
      return false;
  }
  return true;
}

bool UserEventStream::SetFollowAndRotate(m2::PointD const & userPos, m2::PointD const & pixelPos,
                                         double azimuth, int preferredZoomLevel, double autoScale,
                                         bool isAnim, bool isAutoScale, Animation::TAction const & onFinishAction,
                                         TAnimationCreator const & parallelAnimCreator)
{
  // Reset current follow-and-rotate animation if possible.
  if (isAnim && !InterruptFollowAnimations(false /* force */))
    return false;

  ScreenBase const & currentScreen = GetCurrentScreen();
  ScreenBase screen = currentScreen;

  if (preferredZoomLevel == kDoNotChangeZoom && !isAutoScale)
  {
    GetTargetScreen(screen);
    screen.SetAngle(-azimuth);
  }
  else
  {
    screen.SetFromParams(userPos, -azimuth, isAutoScale ? autoScale : GetScreenScale(preferredZoomLevel));
  }
  screen.MatchGandP3d(userPos, pixelPos);

  ShrinkAndScaleInto(screen, df::GetWorldRect());

  if (isAnim)
  {
    drape_ptr<Animation> anim;
    double const moveDuration = PositionInterpolator::GetMoveDuration(currentScreen.GetOrg(), screen.GetOrg(),
                                                                      currentScreen.PixelRectIn3d(),
                                                                      (currentScreen.GetScale() + screen.GetScale()) / 2.0);
    if (moveDuration > kMaxAnimationTimeSec)
    {
      // Run pretty move animation if we are far from userPos.
      anim = GetPrettyFollowAnimation(currentScreen, userPos, screen.GetScale(), -azimuth, pixelPos);
    }
    else
    {
      // Run follow-and-rotate animation.
      anim = GetFollowAnimation(currentScreen, userPos, screen.GetScale(), -azimuth, pixelPos, isAutoScale);
    }

    if (preferredZoomLevel != kDoNotChangeZoom)
    {
      anim->SetCouldBeInterrupted(false);
      anim->SetCouldBeBlended(false);
    }

    anim->SetOnFinishAction(onFinishAction);

    if (parallelAnimCreator != nullptr)
    {
      drape_ptr<ParallelAnimation> parallelAnim = make_unique_dp<ParallelAnimation>();
      parallelAnim->SetCustomType(kParallelFollowAnim);
      parallelAnim->AddAnimation(parallelAnimCreator(anim->GetType() == Animation::Type::MapFollow ? make_ref(anim)
                                                                                                   : nullptr));
      parallelAnim->AddAnimation(std::move(anim));
      m_animationSystem.CombineAnimation(std::move(parallelAnim));
    }
    else
    {
      m_animationSystem.CombineAnimation(std::move(anim));
    }
    return false;
  }

  ResetMapPlaneAnimations();
  m_navigator.SetFromScreen(screen);
  return true;
}

void UserEventStream::SetAutoPerspective(bool isAutoPerspective)
{
  if (isAutoPerspective)
    m_navigator.Enable3dMode();
  else
    m_navigator.Disable3dMode();
  m_navigator.SetAutoPerspective(isAutoPerspective);
}

void UserEventStream::ResetAnimations(Animation::Type animType, bool rewind, bool finishAll)
{
  bool const hasAnimations = m_animationSystem.HasAnimations();
  m_animationSystem.FinishAnimations(animType, rewind, finishAll);
  if (hasAnimations)
    ApplyAnimations();
}

void UserEventStream::ResetAnimations(Animation::Type animType, std::string const & customType, bool rewind, bool finishAll)
{
  bool const hasAnimations = m_animationSystem.HasAnimations();
  m_animationSystem.FinishAnimations(animType, customType, rewind, finishAll);
  if (hasAnimations)
    ApplyAnimations();
}

void UserEventStream::ResetMapPlaneAnimations()
{
  bool const hasAnimations = m_animationSystem.HasAnimations();
  m_animationSystem.FinishObjectAnimations(Animation::Object::MapPlane, false /* rewind */, false /* finishAll */);
  if (hasAnimations)
    ApplyAnimations();
}

m2::AnyRectD UserEventStream::GetCurrentRect() const
{
  return m_navigator.Screen().GlobalRect();
}

void UserEventStream::GetTargetScreen(ScreenBase & screen)
{
  m_animationSystem.FinishAnimations(Animation::Type::KineticScroll, false /* rewind */, false /* finishAll */);
  ApplyAnimations();

  m_animationSystem.GetTargetScreen(m_navigator.Screen(), screen);
}

m2::AnyRectD UserEventStream::GetTargetRect()
{
  ScreenBase targetScreen;
  GetTargetScreen(targetScreen);
  return targetScreen.GlobalRect();
}

bool UserEventStream::ProcessTouch(TouchEvent const & touch)
{
  ASSERT(touch.GetFirstTouch().m_id != -1, ());

  TouchEvent touchEvent = touch;
  touchEvent.PrepareTouches(m_touches);
  bool isMapTouch = false;

  switch (touchEvent.GetTouchType())
  {
  case TouchEvent::TOUCH_DOWN:
    isMapTouch = TouchDown(touchEvent.GetTouches());
    break;
  case TouchEvent::TOUCH_MOVE:
    isMapTouch = TouchMove(touchEvent.GetTouches());
    break;
  case TouchEvent::TOUCH_CANCEL:
    isMapTouch = TouchCancel(touchEvent.GetTouches());
    break;
  case TouchEvent::TOUCH_UP:
    isMapTouch = TouchUp(touchEvent.GetTouches());
    break;
  default:
    ASSERT(false, ());
    break;
  }

  if (m_listener)
    m_listener->OnTouchMapAction(touchEvent.GetTouchType(), isMapTouch);

  return isMapTouch;
}

bool UserEventStream::TouchDown(std::array<Touch, 2> const & touches)
{
  size_t touchCount = GetValidTouchesCount(touches);
  bool isMapTouch = true;

  // Interrupt kinetic scroll on touch down.
  m_animationSystem.FinishAnimations(Animation::Type::KineticScroll, false /* rewind */, true /* finishAll */);

  if (touchCount == 1)
  {
    if (!DetectDoubleTap(touches[0]))
    {
      if (m_state == STATE_EMPTY)
      {
        if (!TryBeginFilter(touches[0]))
        {
          BeginTapDetector();
          m_startDragOrg = touches[0].m_location;
        }
        else
        {
          isMapTouch = false;
        }
      }
    }
  }
  else if (touchCount == 2)
  {
    switch (m_state)
    {
    case STATE_EMPTY:
      BeginTwoFingersTap(touches[0], touches[1]);
      break;
    case STATE_FILTER:
      CancelFilter(touches[0]);
      BeginScale(touches[0], touches[1]);
      break;
    case STATE_TAP_DETECTION:
    case STATE_WAIT_DOUBLE_TAP:
    case STATE_WAIT_DOUBLE_TAP_HOLD:
      CancelTapDetector();
      BeginTwoFingersTap(touches[0], touches[1]);
      break;
    case STATE_DOUBLE_TAP_HOLD:
      EndDoubleTapAndHold(touches[0]);
      BeginScale(touches[0], touches[1]);
      break;
    case STATE_DRAG:
      isMapTouch = EndDrag(touches[0], true /* cancelled */);
      BeginScale(touches[0], touches[1]);
      break;
    default:
      break;
    }
  }

  UpdateTouches(touches);
  return isMapTouch;
}

bool UserEventStream::CheckDrag(std::array<Touch, 2> const & touches, double threshold) const
{
  return m_startDragOrg.SquaredLength(m2::PointD(touches[0].m_location)) > threshold;
}

bool UserEventStream::TouchMove(std::array<Touch, 2> const & touches)
{
  size_t const touchCount = GetValidTouchesCount(touches);
  bool isMapTouch = true;

  switch (m_state)
  {
  case STATE_EMPTY:
    if (touchCount == 1)
    {
      if (CheckDrag(touches, m_dragThreshold))
        BeginDrag(touches[0]);
      else
        isMapTouch = false;
    }
    else
    {
      BeginScale(touches[0], touches[1]);
    }
    break;
  case STATE_TAP_TWO_FINGERS:
    if (touchCount == 2)
    {
      float const threshold = static_cast<float>(m_dragThreshold);
      if (m_twoFingersTouches[0].SquaredLength(touches[0].m_location) > threshold ||
          m_twoFingersTouches[1].SquaredLength(touches[1].m_location) > threshold)
      {
        BeginScale(touches[0], touches[1]);
      }
      else
        isMapTouch = false;
    }
    break;
  case STATE_FILTER:
    ASSERT_EQUAL(touchCount, 1, ());
    isMapTouch = false;
    break;
  case STATE_TAP_DETECTION:
  case STATE_WAIT_DOUBLE_TAP:
    if (CheckDrag(touches, m_dragThreshold))
      CancelTapDetector();
    else
      isMapTouch = false;
    break;
  case STATE_WAIT_DOUBLE_TAP_HOLD:
    if (CheckDrag(touches, m_dragThreshold))
      StartDoubleTapAndHold(touches[0]);
    break;
  case STATE_DOUBLE_TAP_HOLD:
    UpdateDoubleTapAndHold(touches[0]);
    break;
  case STATE_DRAG:
    if (touchCount > 1)
    {
      ASSERT_EQUAL(GetValidTouchesCount(m_touches), 1, ());
      EndDrag(m_touches[0], true /* cancelled */);
    }
    else
    {
      Drag(touches[0]);
    }
    break;
  case STATE_SCALE:
    if (touchCount < 2)
    {
      ASSERT_EQUAL(GetValidTouchesCount(m_touches), 2, ());
      EndScale(m_touches[0], m_touches[1]);
    }
    else
    {
      Scale(touches[0], touches[1]);
    }
    break;
  default:
    ASSERT(false, ());
    break;
  }

  UpdateTouches(touches);
  return isMapTouch;
}

bool UserEventStream::TouchCancel(std::array<Touch, 2> const & touches)
{
  size_t touchCount = GetValidTouchesCount(touches);
  UNUSED_VALUE(touchCount);
  bool isMapTouch = true;
  switch (m_state)
  {
  case STATE_EMPTY:
  case STATE_WAIT_DOUBLE_TAP:
  case STATE_TAP_TWO_FINGERS:
    isMapTouch = false;
    break;
  case STATE_WAIT_DOUBLE_TAP_HOLD:
  case STATE_DOUBLE_TAP_HOLD:
    // Do nothing here.
    break;
  case STATE_FILTER:
    ASSERT_EQUAL(touchCount, 1, ());
    CancelFilter(touches[0]);
    break;
  case STATE_TAP_DETECTION:
    ASSERT_EQUAL(touchCount, 1, ());
    CancelTapDetector();
    isMapTouch = false;
    break;
  case STATE_DRAG:
    ASSERT_EQUAL(touchCount, 1, ());
    isMapTouch = EndDrag(touches[0], true /* cancelled */);
    break;
  case STATE_SCALE:
    ASSERT_EQUAL(touchCount, 2, ());
    EndScale(touches[0], touches[1]);
    break;
  default:
    ASSERT(false, ());
    break;
  }
  UpdateTouches(touches);
  return isMapTouch;
}

bool UserEventStream::TouchUp(std::array<Touch, 2> const & touches)
{
  size_t touchCount = GetValidTouchesCount(touches);
  bool isMapTouch = true;
  switch (m_state)
  {
  case STATE_EMPTY:
  case STATE_WAIT_DOUBLE_TAP:
    isMapTouch = false;
    // Can be if long tap or double tap detected.
    break;
  case STATE_FILTER:
    ASSERT_EQUAL(touchCount, 1, ());
    EndFilter(touches[0]);
    isMapTouch = false;
    break;
  case STATE_TAP_DETECTION:
    if (touchCount == 1)
      EndTapDetector(touches[0]);
    else
      CancelTapDetector();
    break;
  case STATE_TAP_TWO_FINGERS:
    if (touchCount == 2)
      EndTwoFingersTap();
    break;
  case STATE_WAIT_DOUBLE_TAP_HOLD:
    ASSERT_EQUAL(touchCount, 1, ());
    PerformDoubleTap(touches[0]);
    break;
  case STATE_DOUBLE_TAP_HOLD:
    EndDoubleTapAndHold(touches[0]);
    break;
  case STATE_DRAG:
    ASSERT_EQUAL(touchCount, 1, ());
    isMapTouch = EndDrag(touches[0], false /* cancelled */);
    break;
  case STATE_SCALE:
    if (touchCount < 2)
    {
      ASSERT_EQUAL(GetValidTouchesCount(m_touches), 2, ());
      EndScale(m_touches[0], m_touches[1]);
    }
    else
    {
      EndScale(touches[0], touches[1]);
    }
    break;
  default:
    ASSERT(false, ());
    break;
  }

  UpdateTouches(touches);
  return isMapTouch;
}

void UserEventStream::UpdateTouches(std::array<Touch, 2> const & touches)
{
  m_touches = touches;
}

void UserEventStream::BeginTwoFingersTap(Touch const & t1, Touch const & t2)
{
  TEST_CALL(TWO_FINGERS_TAP);
  ASSERT_EQUAL(m_state, STATE_EMPTY, ());
  m_state = STATE_TAP_TWO_FINGERS;
  m_twoFingersTouches[0] = t1.m_location;
  m_twoFingersTouches[1] = t2.m_location;
}

void UserEventStream::EndTwoFingersTap()
{
  ASSERT_EQUAL(m_state, STATE_TAP_TWO_FINGERS, ());
  m_state = STATE_EMPTY;

  if (m_listener)
    m_listener->OnTwoFingersTap();
}

void UserEventStream::BeginDrag(Touch const & t)
{
  TEST_CALL(BEGIN_DRAG);
  ASSERT_EQUAL(m_state, STATE_EMPTY, ());
  m_state = STATE_DRAG;
  m_startDragOrg = m_navigator.Screen().GetOrg();
  if (m_listener)
    m_listener->OnDragStarted();
  m_navigator.StartDrag(m2::PointD(t.m_location));

  if (m_kineticScrollEnabled && !m_scroller.IsActive())
  {
    ResetMapPlaneAnimations();
    m_scroller.Init(m_navigator.Screen());
  }
}

void UserEventStream::Drag(Touch const & t)
{
  TEST_CALL(DRAG);
  ASSERT_EQUAL(m_state, STATE_DRAG, ());
  m_navigator.DoDrag(m2::PointD(t.m_location));

  if (m_kineticScrollEnabled && m_scroller.IsActive())
    m_scroller.Update(m_navigator.Screen());
}

bool UserEventStream::EndDrag(Touch const & t, bool cancelled)
{
  TEST_CALL(END_DRAG);
  ASSERT_EQUAL(m_state, STATE_DRAG, ());
  m_state = STATE_EMPTY;
  if (m_listener)
    m_listener->OnDragEnded(m_navigator.GtoP(m_navigator.Screen().GetOrg()) - m_navigator.GtoP(m_startDragOrg));

  m_startDragOrg = m2::PointD::Zero();
  m_navigator.StopDrag(m2::PointD(t.m_location));

  if (!cancelled && m_kineticScrollEnabled && m_scroller.IsActive() &&
      m_kineticTimer.ElapsedMilliseconds() >= kKineticDelayMs)
  {
    drape_ptr<Animation> anim = m_scroller.CreateKineticAnimation(m_navigator.Screen());
    if (anim != nullptr)
      m_animationSystem.CombineAnimation(std::move(anim));
    return false;
  }

  m_scroller.Cancel();
  return true;
}

void UserEventStream::BeginScale(Touch const & t1, Touch const & t2)
{
  TEST_CALL(BEGIN_SCALE);

  if (m_state == STATE_SCALE)
  {
    Scale(t1, t2);
    return;
  }

  ASSERT(m_state == STATE_EMPTY || m_state == STATE_TAP_TWO_FINGERS, ());
  m_state = STATE_SCALE;
  m2::PointD touch1(t1.m_location);
  m2::PointD touch2(t2.m_location);

  if (m_listener)
  {
    m_listener->OnScaleStarted();
    m_listener->CorrectScalePoint(touch1, touch2);
  }

  m_navigator.StartScale(touch1, touch2);
}

void UserEventStream::Scale(Touch const & t1, Touch const & t2)
{
  TEST_CALL(SCALE);
  ASSERT_EQUAL(m_state, STATE_SCALE, ());

  m2::PointD touch1(t1.m_location);
  m2::PointD touch2(t2.m_location);

  if (m_listener)
  {
    if (m_navigator.IsRotatingDuringScale())
      m_listener->OnRotated();

    m_listener->CorrectScalePoint(touch1, touch2);
  }

  m_navigator.DoScale(touch1, touch2);
}

void UserEventStream::EndScale(const Touch & t1, const Touch & t2)
{
  TEST_CALL(END_SCALE);
  ASSERT_EQUAL(m_state, STATE_SCALE, ());
  m_state = STATE_EMPTY;

  m2::PointD touch1(t1.m_location);
  m2::PointD touch2(t2.m_location);

  if (m_listener)
  {
    m_listener->CorrectScalePoint(touch1, touch2);
    m_listener->OnScaleEnded();
  }

  m_navigator.StopScale(touch1, touch2);

  m_kineticTimer.Reset();
}

void UserEventStream::BeginTapDetector()
{
  TEST_CALL(BEGIN_TAP_DETECTOR);
  ASSERT_EQUAL(m_state, STATE_EMPTY, ());
  m_state = STATE_TAP_DETECTION;
  m_touchTimer.Reset();
}

void UserEventStream::DetectShortTap(Touch const & touch)
{
  if (DetectForceTap(touch))
    return;

  if (m_touchTimer.ElapsedMilliseconds() > kDoubleTapPauseMs)
  {
    m_state = STATE_EMPTY;
    if (m_listener)
      m_listener->OnTap(m2::PointD(touch.m_location), false /* isLongTap */);
  }
}

void UserEventStream::DetectLongTap(Touch const & touch)
{
  ASSERT_EQUAL(m_state, STATE_TAP_DETECTION, ());

  if (DetectForceTap(touch))
    return;

  if (m_touchTimer.ElapsedMilliseconds() > kLongTouchMs)
  {
    TEST_CALL(LONG_TAP_DETECTED);
    m_state = STATE_EMPTY;
    if (m_listener)
      m_listener->OnTap(m2::PointD(touch.m_location), true /* isLongTap */);
  }
}

bool UserEventStream::DetectDoubleTap(Touch const & touch)
{
  if (m_state != STATE_WAIT_DOUBLE_TAP || m_touchTimer.ElapsedMilliseconds() > kDoubleTapPauseMs)
    return false;

  m_state = STATE_WAIT_DOUBLE_TAP_HOLD;
  return true;
}

void UserEventStream::PerformDoubleTap(Touch const & touch)
{
  ASSERT_EQUAL(m_state, STATE_WAIT_DOUBLE_TAP_HOLD, ());
  m_state = STATE_EMPTY;
  if (m_listener)
    m_listener->OnDoubleTap(m2::PointD(touch.m_location));
}

bool UserEventStream::DetectForceTap(Touch const & touch)
{
  if (touch.m_force >= kForceTapThreshold)
  {
    m_state = STATE_EMPTY;
    if (m_listener)
      m_listener->OnForceTap(m2::PointD(touch.m_location));
    return true;
  }

  return false;
}

void UserEventStream::EndTapDetector(Touch const & touch)
{
  TEST_CALL(SHORT_TAP_DETECTED);
  ASSERT_EQUAL(m_state, STATE_TAP_DETECTION, ());
  m_state = STATE_WAIT_DOUBLE_TAP;
}

void UserEventStream::CancelTapDetector()
{
  TEST_CALL(CANCEL_TAP_DETECTOR);
  ASSERT(m_state == STATE_TAP_DETECTION || m_state == STATE_WAIT_DOUBLE_TAP ||
    m_state == STATE_WAIT_DOUBLE_TAP_HOLD, ());
  m_state = STATE_EMPTY;
}

bool UserEventStream::TryBeginFilter(Touch const & t)
{
  TEST_CALL(TRY_FILTER);
  ASSERT_EQUAL(m_state, STATE_EMPTY, ());
  if (m_listener && m_listener->OnSingleTouchFiltrate(m2::PointD(t.m_location), TouchEvent::TOUCH_DOWN))
  {
    m_state = STATE_FILTER;
    return true;
  }

  return false;
}

void UserEventStream::EndFilter(const Touch & t)
{
  TEST_CALL(END_FILTER);
  ASSERT_EQUAL(m_state, STATE_FILTER, ());
  m_state = STATE_EMPTY;
  if (m_listener)
    m_listener->OnSingleTouchFiltrate(m2::PointD(t.m_location), TouchEvent::TOUCH_UP);
}

void UserEventStream::CancelFilter(Touch const & t)
{
  TEST_CALL(CANCEL_FILTER);
  ASSERT_EQUAL(m_state, STATE_FILTER, ());
  m_state = STATE_EMPTY;
  if (m_listener)
    m_listener->OnSingleTouchFiltrate(m2::PointD(t.m_location), TouchEvent::TOUCH_CANCEL);
}

void UserEventStream::StartDoubleTapAndHold(Touch const & touch)
{
  TEST_CALL(BEGIN_DOUBLE_TAP_AND_HOLD);
  ASSERT_EQUAL(m_state, STATE_WAIT_DOUBLE_TAP_HOLD, ());
  m_state = STATE_DOUBLE_TAP_HOLD;
  m_startDoubleTapAndHold = m_startDragOrg;
  if (m_listener)
    m_listener->OnScaleStarted();
}

void UserEventStream::UpdateDoubleTapAndHold(Touch const & touch)
{
  TEST_CALL(DOUBLE_TAP_AND_HOLD);
  ASSERT_EQUAL(m_state, STATE_DOUBLE_TAP_HOLD, ());
  float const kPowerModifier = 10.0f;
  double const scaleFactor = exp(kPowerModifier * (touch.m_location.y - m_startDoubleTapAndHold.y) /
                                 GetCurrentScreen().PixelRectIn3d().SizeY());
  m_startDoubleTapAndHold = touch.m_location;

  m2::PointD scaleCenter = m_startDragOrg;
  if (m_listener)
    m_listener->CorrectScalePoint(scaleCenter);
  m_navigator.Scale(scaleCenter, scaleFactor);
}

void UserEventStream::EndDoubleTapAndHold(Touch const & touch)
{
  TEST_CALL(END_DOUBLE_TAP_AND_HOLD);
  ASSERT_EQUAL(m_state, STATE_DOUBLE_TAP_HOLD, ());
  m_state = STATE_EMPTY;
  if (m_listener)
    m_listener->OnScaleEnded();
}

bool UserEventStream::IsInUserAction() const
{
  return m_state == STATE_DRAG || m_state == STATE_SCALE;
}

bool UserEventStream::IsWaitingForActionCompletion() const
{
  return m_state != STATE_EMPTY;
}

void UserEventStream::SetKineticScrollEnabled(bool enabled)
{
  m_kineticScrollEnabled = enabled;
  m_kineticTimer.Reset();
  if (!m_kineticScrollEnabled)
    m_scroller.Cancel();
}
}  // namespace df
