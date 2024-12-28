#include "map/search_api.hpp"

#include "map/bookmarks_search_params.hpp"
#include "map/everywhere_search_params.hpp"

#include "search/geometry_utils.hpp"
#include "search/utils.hpp"

#include "storage/downloader_search_params.hpp"

#include "platform/preferred_languages.hpp"

#include "geometry/mercator.hpp"

#include "base/checked_cast.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <map>
#include <string>
#include <type_traits>

using namespace search;
using namespace std;

namespace
{
using BookmarkIdDoc = pair<bookmarks::Id, bookmarks::Doc>;

double const kDistEqualQueryMeters = 100.0;
double const kDistEqualQueryMercator = mercator::MetersToMercator(kDistEqualQueryMeters);

// Cancels search query by |handle|.
void CancelQuery(weak_ptr<ProcessorHandle> & handle)
{
  if (auto queryHandle = handle.lock())
    queryHandle->Cancel();
  handle.reset();
}

bookmarks::Id KmlMarkIdToSearchBookmarkId(kml::MarkId id)
{
  static_assert(is_integral<kml::MarkId>::value, "");
  static_assert(is_integral<bookmarks::Id>::value, "");

  static_assert(is_unsigned<kml::MarkId>::value, "");
  static_assert(is_unsigned<bookmarks::Id>::value, "");

  static_assert(sizeof(bookmarks::Id) == sizeof(kml::MarkId), "");

  return base::asserted_cast<bookmarks::Id>(id);
}

bookmarks::GroupId KmlGroupIdToSearchGroupId(kml::MarkGroupId id)
{
  static_assert(is_integral<kml::MarkGroupId>::value, "");
  static_assert(is_integral<bookmarks::GroupId>::value, "");

  static_assert(is_unsigned<kml::MarkGroupId>::value, "");
  static_assert(is_unsigned<bookmarks::GroupId>::value, "");

  static_assert(sizeof(bookmarks::GroupId) >= sizeof(kml::MarkGroupId), "");

  if (id == kml::kInvalidMarkGroupId)
    return bookmarks::kInvalidGroupId;

  return base::asserted_cast<bookmarks::GroupId>(id);
}

kml::MarkId SearchBookmarkIdToKmlMarkId(bookmarks::Id id) { return static_cast<kml::MarkId>(id); }

void AppendBookmarkIdDocs(vector<BookmarkInfo> const & marks, vector<BookmarkIdDoc> & result)
{
  result.reserve(result.size() + marks.size());

  auto const locale = languages::GetCurrentOrig();
  for (auto const & mark : marks)
  {
    result.emplace_back(KmlMarkIdToSearchBookmarkId(mark.m_bookmarkId),
                        bookmarks::Doc(mark.m_bookmarkData, locale));
  }
}

void AppendBookmarkIds(vector<kml::MarkId> const & marks, vector<bookmarks::Id> & result)
{
  result.reserve(result.size() + marks.size());
  transform(marks.begin(), marks.end(), back_inserter(result), KmlMarkIdToSearchBookmarkId);
}

class BookmarksSearchCallback
{
public:
  using OnResults = BookmarksSearchParams::OnResults;

  BookmarksSearchCallback(SearchAPI::Delegate & delegate, OnResults onResults)
    : m_delegate(delegate), m_onResults(std::move(onResults))
  {
  }

  void operator()(Results const & results)
  {
    if (results.IsEndMarker())
    {
      if (results.IsEndedNormal())
      {
        m_status = BookmarksSearchParams::Status::Completed;
      }
      else
      {
        ASSERT(results.IsEndedCancelled(), ());
        m_status = BookmarksSearchParams::Status::Cancelled;
      }
    }
    else
    {
      ASSERT_EQUAL(m_status, BookmarksSearchParams::Status::InProgress, ());
    }

    auto const & rs = results.GetBookmarksResults();
    ASSERT_LESS_OR_EQUAL(m_results.size(), rs.size(), ());

    for (size_t i = m_results.size(); i < rs.size(); ++i)
      m_results.emplace_back(SearchBookmarkIdToKmlMarkId(rs[i].m_id));

    m_delegate.RunUITask([onResults = m_onResults, results = m_results, status = m_status]() mutable
    {
      onResults(std::move(results), status);
    });
  }

private:
  BookmarksSearchParams::Results m_results;
  BookmarksSearchParams::Status m_status = BookmarksSearchParams::Status::InProgress;

  SearchAPI::Delegate & m_delegate;
  OnResults m_onResults;
};
}  // namespace

SearchAPI::SearchAPI(DataSource & dataSource, storage::Storage const & storage,
                     storage::CountryInfoGetter const & infoGetter, size_t numThreads,
                     Delegate & delegate)
  : m_dataSource(dataSource)
  , m_storage(storage)
  , m_infoGetter(infoGetter)
  , m_delegate(delegate)
  , m_engine(m_dataSource, GetDefaultCategories(), m_infoGetter,
             Engine::Params(languages::GetCurrentMapTwine() /* locale */, numThreads))
{
}

void SearchAPI::OnViewportChanged(m2::RectD const & viewport)
{
  m_viewport = viewport;

  auto const forceSearchInViewport = !m_isViewportInitialized;
  if (!m_isViewportInitialized)
  {
    m_isViewportInitialized = true;
    for (size_t i = 0; i < static_cast<size_t>(Mode::Count); i++)
    {
      auto & intent = m_searchIntents[i];
      // Viewport search will be triggered below, in PokeSearchInViewport().
      if (!intent.m_isDelayed || static_cast<Mode>(i) == Mode::Viewport)
        continue;
      intent.m_params.m_viewport = m_viewport;
      intent.m_params.m_position = m_delegate.GetCurrentPosition();
      Search(intent);
    }
  }

  PokeSearchInViewport(forceSearchInViewport);
}

bool SearchAPI::SearchEverywhere(EverywhereSearchParams params)
{
  SearchParams p;
  p.m_query = std::move(params.m_query);
  p.m_inputLocale = std::move(params.m_inputLocale);
  p.m_mode = Mode::Everywhere;
  p.m_position = m_delegate.GetCurrentPosition();
  SetViewportIfPossible(p);  // Search request will be delayed if viewport is not available.
  p.m_maxNumResults = SearchParams::kDefaultNumResultsEverywhere;
  p.m_suggestsEnabled = true;
  p.m_needAddress = true;
  p.m_needHighlighting = true;
  p.m_categorialRequest = params.m_isCategory;
  if (params.m_timeout)
    p.m_timeout = *params.m_timeout;

  p.m_onResults = EverywhereSearchCallback(*this, std::move(params.m_onResults));

  return Search(std::move(p), true /* forceSearch */);
}

bool SearchAPI::SearchInViewport(ViewportSearchParams params)
{
  // Save params first for the PokeSearchInViewport function.
  m_viewportParams = params;

  SearchParams p;
  p.m_query = std::move(params.m_query);
  p.m_inputLocale = std::move(params.m_inputLocale);
  p.m_position = m_delegate.GetCurrentPosition();
  SetViewportIfPossible(p);  // Search request will be delayed if viewport is not available.
  p.m_maxNumResults = SearchParams::kDefaultNumResultsInViewport;
  p.m_mode = Mode::Viewport;
  p.m_suggestsEnabled = false;
  p.m_needAddress = false;
  p.m_needHighlighting = false;
  p.m_categorialRequest = params.m_isCategory;

  if (params.m_timeout)
    p.m_timeout = *params.m_timeout;

  if (params.m_onStarted)
  {
    p.m_onStarted = [this, onStarted = std::move(params.m_onStarted)]() mutable
    {
      RunUITask([onStarted = std::move(onStarted)]() { onStarted(); });
    };
  }

  p.m_onResults = ViewportSearchCallback(m_viewport, *this, std::move(params.m_onCompleted));

  return Search(std::move(p), false /* forceSearch */);
}

bool SearchAPI::SearchInDownloader(storage::DownloaderSearchParams params)
{
  SearchParams p;
  p.m_query = params.m_query;
  p.m_inputLocale = params.m_inputLocale;
  p.m_position = m_delegate.GetCurrentPosition();
  SetViewportIfPossible(p);  // Search request will be delayed if viewport is not available.
  p.m_maxNumResults = SearchParams::kDefaultNumResultsEverywhere;
  p.m_mode = Mode::Downloader;
  p.m_suggestsEnabled = false;
  p.m_needAddress = false;
  p.m_needHighlighting = false;

  p.m_onResults = DownloaderSearchCallback(*this, m_dataSource, m_infoGetter, m_storage, std::move(params));

  return Search(std::move(p), true /* forceSearch */);
}

bool SearchAPI::SearchInBookmarks(search::BookmarksSearchParams params)
{
  SearchParams p;
  p.m_query = std::move(params.m_query);
  p.m_position = m_delegate.GetCurrentPosition();
  SetViewportIfPossible(p);  // Search request will be delayed if viewport is not available.
  p.m_maxNumResults = SearchParams::kDefaultNumBookmarksResults;
  p.m_mode = Mode::Bookmarks;
  p.m_suggestsEnabled = false;
  p.m_needAddress = false;

  p.m_bookmarksGroupId = params.m_groupId;
  p.m_onResults = BookmarksSearchCallback(m_delegate, std::move(params.m_onResults));

  return Search(std::move(p), true /* forceSearch */);
}

void SearchAPI::PokeSearchInViewport(bool forceSearch)
{
  if (!m_isViewportInitialized || !IsViewportSearchActive())
    return;

  // Copy is intentional here, to skip possible duplicating requests.
  auto params = m_searchIntents[static_cast<size_t>(Mode::Viewport)].m_params;
  SetViewportIfPossible(params);
  params.m_position = m_delegate.GetCurrentPosition();
  params.m_onResults = ViewportSearchCallback(m_viewport, *this, m_viewportParams.m_onCompleted);

  Search(std::move(params), forceSearch);
}

void SearchAPI::CancelSearch(Mode mode)
{
  ASSERT_NOT_EQUAL(mode, Mode::Count, ());

  if (mode == Mode::Viewport)
  {
    m_delegate.ClearViewportSearchResults();
  }

  auto & intent = m_searchIntents[static_cast<size_t>(mode)];
  intent.m_params.Clear();
  CancelQuery(intent.m_handle);
}

void SearchAPI::CancelAllSearches()
{
  for (size_t i = 0; i < static_cast<size_t>(Mode::Count); ++i)
    CancelSearch(static_cast<Mode>(i));
}

void SearchAPI::RunUITask(function<void()> fn) { return m_delegate.RunUITask(std::move(fn)); }

bool SearchAPI::IsViewportSearchActive() const
{
  return !m_searchIntents[static_cast<size_t>(Mode::Viewport)].m_params.m_query.empty();
}

void SearchAPI::ShowViewportSearchResults(Results::ConstIter begin, Results::ConstIter end,
                                          bool clear)
{
  return m_delegate.ShowViewportSearchResults(begin, end, clear);
}

ProductInfo SearchAPI::GetProductInfo(Result const & result) const
{
  return m_delegate.GetProductInfo(result);
}

void SearchAPI::EnableIndexingOfBookmarksDescriptions(bool enable)
{
  m_engine.EnableIndexingOfBookmarksDescriptions(enable);
}

void SearchAPI::EnableIndexingOfBookmarkGroup(kml::MarkGroupId const & groupId, bool enable)
{
  if (enable)
    m_indexableGroups.insert(groupId);
  else
    m_indexableGroups.erase(groupId);

  m_engine.EnableIndexingOfBookmarkGroup(KmlGroupIdToSearchGroupId(groupId), enable);
}

unordered_set<kml::MarkGroupId> const & SearchAPI::GetIndexableGroups() const
{
  return m_indexableGroups;
}

void SearchAPI::ResetBookmarksEngine()
{
  m_indexableGroups.clear();
  m_engine.ResetBookmarks();
}

void SearchAPI::OnBookmarksCreated(vector<BookmarkInfo> const & marks)
{
  vector<BookmarkIdDoc> data;
  AppendBookmarkIdDocs(marks, data);
  m_engine.OnBookmarksCreated(data);
}

void SearchAPI::OnBookmarksUpdated(vector<BookmarkInfo> const & marks)
{
  vector<BookmarkIdDoc> data;
  AppendBookmarkIdDocs(marks, data);
  m_engine.OnBookmarksUpdated(data);
}

void SearchAPI::OnBookmarksDeleted(vector<kml::MarkId> const & marks)
{
  vector<bookmarks::Id> data;
  AppendBookmarkIds(marks, data);
  m_engine.OnBookmarksDeleted(data);
}

void SearchAPI::OnBookmarksAttached(vector<BookmarkGroupInfo> const & groupInfos)
{
  for (auto const & info : groupInfos)
  {
    vector<bookmarks::Id> data;
    AppendBookmarkIds(info.m_bookmarkIds, data);
    m_engine.OnBookmarksAttachedToGroup(KmlGroupIdToSearchGroupId(info.m_groupId), data);
  }
}

void SearchAPI::OnBookmarksDetached(vector<BookmarkGroupInfo> const & groupInfos)
{
  for (auto const & info : groupInfos)
  {
    vector<bookmarks::Id> data;
    AppendBookmarkIds(info.m_bookmarkIds, data);
    m_engine.OnBookmarksDetachedFromGroup(KmlGroupIdToSearchGroupId(info.m_groupId), data);
  }
}

bool SearchAPI::Search(SearchParams params, bool forceSearch)
{
  if (m_delegate.ParseSearchQueryCommand(params))
    return false;

  auto const mode = params.m_mode;
  auto & intent = m_searchIntents[static_cast<size_t>(mode)];

  if (!forceSearch && QueryMayBeSkipped(intent.m_params, params))
    return false;

  intent.m_params = std::move(params);

  // Cancels previous search request (if any) and initiates a new search request.
  CancelQuery(intent.m_handle);

  intent.m_params.m_minDistanceOnMapBetweenResults = m_delegate.GetMinDistanceBetweenResults();

  Search(intent);

  return true;
}

void SearchAPI::Search(SearchIntent & intent)
{
  if (!m_isViewportInitialized)
  {
    intent.m_isDelayed = true;
    return;
  }

  intent.m_handle = m_engine.Search(intent.m_params);
  intent.m_isDelayed = false;
}

void SearchAPI::SetViewportIfPossible(SearchParams & params)
{
  if (m_isViewportInitialized)
    params.m_viewport = m_viewport;
}

void SearchAPI::SetLocale(std::string const & locale)
{
  m_engine.SetLocale(locale);
}

bool SearchAPI::QueryMayBeSkipped(SearchParams const & prevParams,
                                  SearchParams const & currParams) const
{
  auto const & prevViewport = prevParams.m_viewport;
  auto const & currViewport = currParams.m_viewport;

  if (!prevParams.IsEqualCommon(currParams))
    return false;

  if (!prevViewport.IsValid() ||
      !IsEqualMercator(prevViewport, currViewport, kDistEqualQueryMercator))
  {
    return false;
  }

  if (prevParams.m_position && currParams.m_position &&
      mercator::DistanceOnEarth(*prevParams.m_position, *currParams.m_position) >
          kDistEqualQueryMercator)
  {
    return false;
  }

  if (static_cast<bool>(prevParams.m_position) != static_cast<bool>(currParams.m_position))
    return false;

  return true;
}
