#include "sequence_animation.hpp"

#include "drape_frontend/animation_system.hpp"

#include "base/assert.hpp"

namespace df
{

SequenceAnimation::SequenceAnimation()
  : Animation(true /* couldBeInterrupted */, true /* couldBeBlended */)
{
}

void SequenceAnimation::Init(ScreenBase const & screen, TPropertyCache const & properties)
{
  if (!m_animations.empty())
    m_animations.front()->Init(screen, properties);
}

std::string SequenceAnimation::GetCustomType() const { return m_customType; }

void SequenceAnimation::SetCustomType(std::string const & type) { m_customType = type; }

Animation::TAnimObjects const & SequenceAnimation::GetObjects() const
{
  return m_objects;
}

bool SequenceAnimation::HasObject(Object object) const
{
  ASSERT(!m_animations.empty(), ());
  return m_animations.front()->HasObject(object);
}

Animation::TObjectProperties const & SequenceAnimation::GetProperties(Object object) const
{
  ASSERT(HasObject(object), ());
  return m_properties.find(object)->second;
}

bool SequenceAnimation::HasProperty(Object object, ObjectProperty property) const
{
  if (!HasObject(object))
    return false;
  ASSERT(!m_animations.empty(), ());
  return m_animations.front()->HasProperty(object, property);
}

bool SequenceAnimation::HasTargetProperty(Object object, ObjectProperty property) const
{
  ASSERT(!m_animations.empty(), ());
  for (auto const & anim : m_animations)
  {
    if (anim->HasTargetProperty(object, property))
      return true;
  }
  return false;
}

void SequenceAnimation::SetMaxDuration(double)
{
  ASSERT(false, ("Not implemented"));
}

void SequenceAnimation::SetMinDuration(double)
{
  ASSERT(false, ("Not implemented"));
}

double SequenceAnimation::GetDuration() const
{
  double duration = 0.0;
  for (auto const & anim : m_animations)
    duration += anim->GetDuration();
  return duration;
}

double SequenceAnimation::GetMaxDuration() const
{
  double maxDuration = 0.0;
  double duration;
  for (auto const & anim : m_animations)
  {
    duration = anim->GetMaxDuration();
    if (duration < 0.0)
      return Animation::kInvalidAnimationDuration;
    maxDuration += duration;
  }
  return maxDuration;
}

double SequenceAnimation::GetMinDuration() const
{
  double minDuration = 0.0;
  double duration;
  for (auto const & anim : m_animations)
  {
    duration = anim->GetMinDuration();
    if (duration < 0.0)
      return Animation::kInvalidAnimationDuration;
    minDuration += duration;
  }
  return minDuration;
}

bool SequenceAnimation::IsFinished() const
{
  return m_animations.empty();
}

bool SequenceAnimation::GetProperty(Object object, ObjectProperty property, PropertyValue & value) const
{
  ASSERT(!m_animations.empty(), ());
  return m_animations.front()->GetProperty(object, property, value);
}

bool SequenceAnimation::GetTargetProperty(Object object, ObjectProperty property, PropertyValue & value) const
{
  ASSERT(!m_animations.empty(), ());

  for (auto it = m_animations.rbegin(); it != m_animations.rend(); ++it)
  {
    auto const & anim = *it;
    if (anim->HasTargetProperty(object, property))
      return anim->GetTargetProperty(object, property, value);
  }

  return false;
}

void SequenceAnimation::AddAnimation(drape_ptr<Animation> && animation)
{
  m_animations.emplace_back(std::move(animation));
  if (m_animations.size() == 1)
    ObtainObjectProperties();
}

void SequenceAnimation::OnStart()
{
  if (m_animations.empty())
    return;
  m_animations.front()->OnStart();
  Animation::OnStart();
}

void SequenceAnimation::OnFinish()
{
  Animation::OnFinish();
}

void SequenceAnimation::Advance(double elapsedSeconds)
{
  if (m_animations.empty())
    return;
  m_animations.front()->Advance(elapsedSeconds);
  if (m_animations.front()->IsFinished())
  {
    m_animations.front()->OnFinish();
    AnimationSystem::Instance().SaveAnimationResult(*m_animations.front());
    m_animations.pop_front();
    ObtainObjectProperties();
  }
}

void SequenceAnimation::Finish()
{
  for (auto & anim : m_animations)
  {
    anim->Finish();
    AnimationSystem::Instance().SaveAnimationResult(*anim);
  }
  m_animations.clear();
  ObtainObjectProperties();
  Animation::Finish();
}

void SequenceAnimation::ObtainObjectProperties()
{
  m_objects.clear();
  m_properties.clear();

  if (m_animations.empty())
    return;

  TAnimObjects const & objects = m_animations.front()->GetObjects();
  m_objects.insert(objects.begin(), objects.end());
  for (auto const & object : objects)
  {
    TObjectProperties const & properties = m_animations.front()->GetProperties(object);
    m_properties[object].insert(properties.begin(), properties.end());
  }
}

} // namespace df
