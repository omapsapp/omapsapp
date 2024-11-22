#include "indexer/feature_impl.hpp"

#include "base/math.hpp"

namespace feature
{

uint8_t PopulationToRank(uint64_t p)
{
  return static_cast<uint8_t>(std::min(0xFF, math::SignedRound(log(double(p)) / log(1.1))));
}

uint64_t RankToPopulation(uint8_t r)
{
  return static_cast<uint64_t>(pow(1.1, r));
}

} // namespace feature
