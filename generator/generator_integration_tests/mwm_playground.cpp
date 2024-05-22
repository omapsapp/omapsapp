#include "testing/testing.hpp"

#include "routing/cross_mwm_connector_serialization.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/data_source.hpp"

#include "platform/local_country_file.hpp"
#include "platform/platform.hpp"

#include "coding/map_uint32_to_val.hpp"

#include "base/scope_guard.hpp"

#include "3party/succinct/elias_fano_compressed_list.hpp"

namespace
{
platform::LocalCountryFile MakeFile(std::string name, int64_t version)
{
  return { GetPlatform().WritableDir() + std::to_string(version),
           platform::CountryFile(std::move(name)), version };
}
} // namespace

UNIT_TEST(CrossMwmWeights)
{
  classificator::Load();

  FrozenDataSource dataSource;
  auto const res = dataSource.RegisterMap(MakeFile("Austria_Lower Austria_Wien", 220314));
  TEST_EQUAL(res.second, MwmSet::RegResult::Success, ());

  auto const handle = dataSource.GetMwmHandleById(res.first);
  auto reader = handle.GetValue()->m_cont.GetReader(CROSS_MWM_FILE_TAG);
  LOG(LINFO, ("Section size =", reader.Size()));

  using CrossMwmID = base::GeoObjectId;
  using namespace routing;
  CrossMwmConnector<CrossMwmID> connector;
  CrossMwmConnectorBuilder<CrossMwmID> builder(connector);

  builder.DeserializeTransitions(VehicleType::Car, reader);
  builder.DeserializeWeights(reader);

  //std::vector<connector::Weight> weights(connector.GetNumEnters() * connector.GetNumExits());

  using IdxWeightT = std::pair<uint32_t, uint32_t>;
  std::vector<IdxWeightT> idx2weight;

  connector.ForEachEnter([&](uint32_t enterIdx, Segment const &)
  {
    connector.ForEachExit([&](uint32_t exitIdx, Segment const &)
    {
      uint32_t const idx = connector.GetWeightIndex(enterIdx, exitIdx);
      uint32_t const weight = connector.GetWeight(enterIdx, exitIdx);
      //weights[idx] = weight;

      if (weight > 0)
        idx2weight.emplace_back(idx, weight);
    });
  });

  /*
  std::string const filePath = "cross_mwm_weights.ef";
  SCOPE_GUARD(deleteFile, [&filePath]() { base::DeleteFileX(filePath); });

  {
    succinct::elias_fano_compressed_list efList(weights);
    succinct::mapper::freeze(efList, filePath.c_str());

    LOG(LINFO, ("EF list size =", FileReader(filePath).Size()));
  }

  base::EraseIf(weights, [](connector::Weight w) { return w == 0; });
  LOG(LINFO, ("Raw no-zeroes size =", weights.size() * sizeof(connector::Weight)));

  {
    succinct::elias_fano_compressed_list efList(weights);
    succinct::mapper::freeze(efList, filePath.c_str());

    LOG(LINFO, ("EF list no-zeroes size =", FileReader(filePath).Size()));
  }
  */

  CrossMwmConnectorBuilderEx<CrossMwmID> builder1;
  std::vector<uint8_t> buffer;
  builder1.SetAndWriteWeights(std::move(idx2weight), buffer);
  LOG(LINFO, ("Map size =", buffer.size()));
}
