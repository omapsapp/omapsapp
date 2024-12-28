#pragma once

#include "map/bookmark_helpers.hpp"
#include "map/everywhere_search_callback.hpp"
#include "map/search_product_info.hpp"
#include "map/viewport_search_callback.hpp"
#include "map/viewport_search_params.hpp"

#include "search/downloader_search_callback.hpp"
#include "search/engine.hpp"
#include "search/mode.hpp"
#include "search/query_saver.hpp"
#include "search/result.hpp"
#include "search/search_params.hpp"

#include "geometry/point2d.hpp"
#include "geometry/rect2d.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>
#include <string>

class DataSource;

namespace search
{
struct BookmarksSearchParams;
struct EverywhereSearchParams;
struct DiscoverySearchParams;
}

namespace storage
{
class CountryInfoGetter;
class Storage;
struct DownloaderSearchParams;
}

class SearchAPI : public search::DownloaderSearchCallback::Delegate,
                  public search::ViewportSearchCallback::Delegate,
                  public search::EverywhereSearchCallback::Delegate
{
public:
  struct Delegate
  {
    virtual ~Delegate() = default;

    virtual void RunUITask(std::function<void()> /* fn */) {}

    using ResultsIterT = search::Results::ConstIter;
    virtual void ShowViewportSearchResults(ResultsIterT begin, ResultsIterT end, bool clear) {}

    virtual void ClearViewportSearchResults() {}

    virtual std::optional<m2::PointD> GetCurrentPosition() const { return {}; }

    virtual bool ParseSearchQueryCommand(search::SearchParams const & /* params */) { return false; }

    virtual m2::PointD GetMinDistanceBetweenResults() const { return {0, 0}; }

    virtual search::ProductInfo GetProductInfo(search::Result const & result) const { return {}; }
  };

  SearchAPI(DataSource & dataSource, storage::Storage const & storage,
            storage::CountryInfoGetter const & infoGetter, size_t numThreads, Delegate & delegate);
  virtual ~SearchAPI() = default;

  void OnViewportChanged(m2::RectD const & viewport);

  void InitAfterWorldLoaded()
  {
    m_engine.CacheWorldLocalities();
    m_engine.LoadCitiesBoundaries();
  }

  // Search everywhere.
  bool SearchEverywhere(search::EverywhereSearchParams params);

  // Search in the viewport.
  bool SearchInViewport(search::ViewportSearchParams params);

  // Search for maps by countries or cities.
  bool SearchInDownloader(storage::DownloaderSearchParams params);

  // Search for bookmarks.
  bool SearchInBookmarks(search::BookmarksSearchParams params);

  search::Engine & GetEngine() { return m_engine; }
  search::Engine const & GetEngine() const { return m_engine; }

  // When search in viewport is active or delayed, restarts search in
  // viewport. When |forceSearch| is false, request is skipped when it
  // is similar to the previous request in the current
  // search-in-viewport session.
  void PokeSearchInViewport(bool forceSearch = true);

  void CancelSearch(search::Mode mode);
  void CancelAllSearches();
  void ClearCaches() { return m_engine.ClearCaches(); }

  // *SearchCallback::Delegate overrides:
  void RunUITask(std::function<void()> fn) override;
  bool IsViewportSearchActive() const override;
  void ShowViewportSearchResults(search::Results::ConstIter begin,
                                 search::Results::ConstIter end, bool clear) override;
  search::ProductInfo GetProductInfo(search::Result const & result) const override;

  std::list<search::QuerySaver::SearchRequest> const & GetLastSearchQueries() const { return m_searchQuerySaver.Get(); }
  void SaveSearchQuery(search::QuerySaver::SearchRequest const & query) { m_searchQuerySaver.Add(query); }
  void ClearSearchHistory() { m_searchQuerySaver.Clear(); }

  void EnableIndexingOfBookmarksDescriptions(bool enable);

  void SetLocale(std::string const & locale);

  // By default all created bookmarks are saved in BookmarksProcessor
  // but we do not index them in an attempt to save time and memory.
  // This method must be used to enable or disable indexing all current and future
  // bookmarks belonging to |groupId|.
  void EnableIndexingOfBookmarkGroup(kml::MarkGroupId const & groupId, bool enable);
  std::unordered_set<kml::MarkGroupId> const & GetIndexableGroups() const;

  // Returns the bookmarks search to its default, pre-launch state.
  // This includes dropping all bookmark data for created bookmarks (efficiently
  // calling OnBookmarksDeleted with all known bookmarks as an argument),
  // clearing the bookmark search index, and resetting all parameters to
  // their default values.
  void ResetBookmarksEngine();

  void OnBookmarksCreated(std::vector<BookmarkInfo> const & marks);
  void OnBookmarksUpdated(std::vector<BookmarkInfo> const & marks);
  void OnBookmarksDeleted(std::vector<kml::MarkId> const & marks);
  void OnBookmarksAttached(std::vector<BookmarkGroupInfo> const & groupInfos);
  void OnBookmarksDetached(std::vector<BookmarkGroupInfo> const & groupInfos);

private:
  struct SearchIntent
  {
    search::SearchParams m_params;
    std::weak_ptr<search::ProcessorHandle> m_handle;
    bool m_isDelayed = false;
  };

  bool Search(search::SearchParams params, bool forceSearch);
  void Search(SearchIntent & intent);

  void SetViewportIfPossible(search::SearchParams & params);

  bool QueryMayBeSkipped(search::SearchParams const & prevParams,
                         search::SearchParams const & currParams) const;

  DataSource & m_dataSource;
  storage::Storage const & m_storage;
  storage::CountryInfoGetter const & m_infoGetter;
  Delegate & m_delegate;

  search::Engine m_engine;

  search::QuerySaver m_searchQuerySaver;

  // Descriptions of last search queries for different modes. May be
  // used for search requests skipping. This field is not guarded
  // because it must be used from the UI thread only.
  SearchIntent m_searchIntents[static_cast<size_t>(search::Mode::Count)];

  m2::RectD m_viewport;
  bool m_isViewportInitialized = false;

  // Viewport search callback should be changed every time when SearchAPI::PokeSearchInViewport
  // is called and we need viewport search params to construct it.
  search::ViewportSearchParams m_viewportParams;

  // Same as the one in bookmarks::Processor. Duplicated here because
  // it is easier than obtaining the information about a group asynchronously
  // from |m_engine|.
  std::unordered_set<kml::MarkGroupId> m_indexableGroups;
};
