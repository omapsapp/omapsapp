#include "drape_frontend/animation_utils.hpp"
#include "drape_frontend/animation_constants.hpp"
#include "drape_frontend/visual_params.hpp"

#include "indexer/scales.hpp"

namespace df
{

bool IsAnimationAllowed(double duration, ScreenBase const &)
{
  return duration > 0.0 && duration <= kMaxAnimationTimeSec;
}

} // namespace df
