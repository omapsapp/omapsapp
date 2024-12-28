#include "generator/mwm_diff/diff.hpp"

#include "base/cancellable.hpp"

#include <iostream>
#include <cstring>

int main(int argc, char ** argv)
{
  auto const ShowUsage = [argv]()
  {
    std::cout <<
        "Usage: " << argv[0] << " make|apply olderMWMPath newerMWMPath diffPath\n"
        "make\n"
        "  Creates the diff between newer and older MWMs at `diffPath`\n"
        "apply\n"
        "  Applies the diff at `diffPath` to the mwm at `olderMWMPath` and stores result at `newerMWMPath`.\n"
        "WARNING: THERE IS NO MWM VALIDITY CHECK!\n";
  };

  if (argc < 5)
  {
    ShowUsage();
    return -1;
  }

  auto const IsEqualUsage = [argv](char const * s) { return 0 == std::strcmp(argv[1], s); };
  char const * olderMWMPath{argv[2]}, * newerMWMPath{argv[3]}, * diffPath{argv[4]};

  if (IsEqualUsage("make"))
  {
    if (generator::mwm_diff::MakeDiff(olderMWMPath, newerMWMPath, diffPath))
      return 0;
  }
  else if (IsEqualUsage("apply"))
  {
    base::Cancellable cancellable;
    auto const res = generator::mwm_diff::ApplyDiff(olderMWMPath, newerMWMPath, diffPath, cancellable);
    if (res == generator::mwm_diff::DiffApplicationResult::Ok)
      return 0;
  }
  else
    ShowUsage();

  return -1;  // Failed, Cancelled.
}
