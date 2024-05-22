#include "indexer/feature_source.hpp"

std::string ToString(FeatureStatus fs)
{
  switch (fs)
  {
  case FeatureStatus::Untouched: return "Untouched";
  case FeatureStatus::Deleted: return "Deleted";
  case FeatureStatus::Obsolete: return "Obsolete";
  case FeatureStatus::Modified: return "Modified";
  case FeatureStatus::Created: return "Created";
  };
  return "Undefined";
}

FeatureSource::FeatureSource(MwmSet::MwmHandle const & handle) : m_handle(handle)
{
  if (!m_handle.IsAlive())
    return;

  auto const & value = *m_handle.GetValue();
  m_vector = std::make_unique<FeaturesVector>(value.m_cont, value.GetHeader(), value.m_table.get(),
                                              value.m_metaDeserializer.get());
}

size_t FeatureSource::GetNumFeatures() const
{
  if (!m_handle.IsAlive())
    return 0;

  ASSERT(m_vector, ());
  return m_vector->GetNumFeatures();
}

std::unique_ptr<FeatureType> FeatureSource::GetOriginalFeature(uint32_t index) const
{
  ASSERT(m_handle.IsAlive(), ());
  ASSERT(m_vector, ());
  auto ft = m_vector->GetByIndex(index);
  ft->SetID({ GetMwmId(), index });
  return ft;
}

FeatureStatus FeatureSource::GetFeatureStatus(uint32_t) const { return FeatureStatus::Untouched; }

std::unique_ptr<FeatureType> FeatureSource::GetModifiedFeature(uint32_t) const { return {}; }

void FeatureSource::ForEachAdditionalFeature(m2::RectD const &, int, std::function<void(uint32_t)> const &) const {}
