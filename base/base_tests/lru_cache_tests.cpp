#include "testing/testing.hpp"

#include "base/lru_cache.hpp"

#include <cstddef>
#include <map>

template <typename Key, typename Value>
class LruCacheTest
{
public:
  using Loader = std::function<void(Key const & key, Value & value)>;

  LruCacheTest(size_t maxCacheSize, Loader const & loader) : m_cache(maxCacheSize), m_loader(loader)
  {
  }

  Value const & GetValue(Key const & key)
  {
    bool found;
    auto & value = m_cache.Find(key, found);
    if (!found)
      m_loader(key, value);
    return value;
  }

  bool IsValid() const { return m_cache.IsValidForTesting(); }

private:
  LruCache<Key, Value> m_cache;
  Loader m_loader;
};

template <typename Key, typename Value>
class LruCacheKeyAgeTest
{
public:
  void InsertKey(Key const & key) { m_keyAge.InsertKey(key); }
  void UpdateAge(Key const & key) { m_keyAge.UpdateAge(key); }
  Key const & GetLruKey() const { return m_keyAge.GetLruKey(); }
  void RemoveLru() { m_keyAge.RemoveLru(); }
  bool IsValid() const { return m_keyAge.IsValidForTesting(); }

  size_t GetAge() const { return m_keyAge.m_age; }
  std::map<size_t, Key> const & GetAgeToKey() const { return m_keyAge.m_ageToKey; };
  std::unordered_map<Key, size_t> const & GetKeyToAge() const { return m_keyAge.m_keyToAge; };

private:
  typename LruCache<Key, Value>::KeyAge m_keyAge;
};

template <typename Key, typename Value>
void TestAge(LruCacheKeyAgeTest<Key, Value> const & keyAge,
             size_t expectedAge, std::map<size_t, Key> const & expectedAgeToKey,
             std::unordered_map<Key, size_t> const & expectedKeyToAge)
{
  TEST(keyAge.IsValid(), ());
  TEST_EQUAL(keyAge.GetAge(), expectedAge, ());
  TEST_EQUAL(keyAge.GetAgeToKey(), expectedAgeToKey, ());
  TEST_EQUAL(keyAge.GetKeyToAge(), expectedKeyToAge, ());
}

UNIT_TEST(LruCacheAgeTest)
{
  using Key = int;
  using Value = double;
  LruCacheKeyAgeTest<Key, Value> age;

  TEST_EQUAL(age.GetAge(), 0, ());
  TEST(age.GetAgeToKey().empty(), ());
  TEST(age.GetKeyToAge().empty(), ());

  age.InsertKey(10);
  {
    std::map<size_t, Key> const expectedAgeToKey({{1 /* age */, 10 /* key */}});
    std::unordered_map<Key, size_t> const expectedKeyToAge({{10 /* key */, 1 /* age */}});
    TestAge(age, 1 /* cache age */, expectedAgeToKey, expectedKeyToAge);
  }

  age.InsertKey(9);
  {
    std::map<size_t, Key> const expectedAgeToKey({{1, 10}, {2, 9}});
    std::unordered_map<Key, size_t> const expectedKeyToAge({{10, 1}, {9, 2}});
    TestAge(age, 2 /* cache age */, expectedAgeToKey, expectedKeyToAge);
  }

  age.RemoveLru();
  {
    std::map<size_t, Key> const expectedAgeToKey({{2, 9}});
    std::unordered_map<Key, size_t> const expectedKeyToAge({{9, 2}});
    TestAge(age, 2 /* cache age */, expectedAgeToKey, expectedKeyToAge);
  }

  age.InsertKey(11);
  {
    std::map<size_t, Key> const expectedAgeToKey({{2, 9}, {3, 11}});
    std::unordered_map<Key, size_t> const expectedKeyToAge({{9, 2}, {11, 3}});
    TestAge(age, 3 /* cache age */, expectedAgeToKey, expectedKeyToAge);
  }

  age.UpdateAge(9);
  {
    std::map<size_t, Key> const expectedAgeToKey({{4, 9}, {3, 11}});
    std::unordered_map<Key, size_t> const expectedKeyToAge({{9, 4}, {11, 3}});
    TestAge(age, 4 /* cache age */, expectedAgeToKey, expectedKeyToAge);
  }

  age.RemoveLru();
  {
    std::map<size_t, Key> const expectedAgeToKey({{4, 9}});
    std::unordered_map<Key, size_t> const expectedKeyToAge({{9, 4}});
    TestAge(age, 4 /* cache age */, expectedAgeToKey, expectedKeyToAge);
  }

  age.InsertKey(12);
  {
    std::map<size_t, Key> const expectedAgeToKey({{4, 9}, {5, 12}});
    std::unordered_map<Key, size_t> const expectedKeyToAge({{9, 4}, {12, 5}});
    TestAge(age, 5 /* cache age */, expectedAgeToKey, expectedKeyToAge);
  }
}

UNIT_TEST(LruCacheSmokeTest)
{
  using Key = int;
  using Value = int;

  {
    LruCacheTest<Key, Value> cache(1 /* maxCacheSize */, [](Key, Value & v) { v = 1; } /* loader */);
    TEST_EQUAL(cache.GetValue(1), 1, ());
    TEST_EQUAL(cache.GetValue(2), 1, ());
    TEST_EQUAL(cache.GetValue(3), 1, ());
    TEST_EQUAL(cache.GetValue(4), 1, ());
    TEST_EQUAL(cache.GetValue(1), 1, ());
    TEST(cache.IsValid(), ());
  }

  {
    LruCacheTest<Key, Value> cache(3 /* maxCacheSize */, [](Key k, Value & v) { v = k; } /* loader */);
    TEST_EQUAL(cache.GetValue(1), 1, ());
    TEST_EQUAL(cache.GetValue(2), 2, ());
    TEST_EQUAL(cache.GetValue(2), 2, ());
    TEST_EQUAL(cache.GetValue(5), 5, ());
    TEST_EQUAL(cache.GetValue(1), 1, ());
    TEST(cache.IsValid(), ());
  }
}

UNIT_TEST(LruCacheLoaderCallsTest)
{
  using Key = int;
  using Value = int;
  bool shouldLoadBeCalled = true;
  auto loader = [&shouldLoadBeCalled](Key k, Value & v) {
    TEST(shouldLoadBeCalled, ());
    v = k;
  };

  LruCacheTest<Key, Value> cache(3 /* maxCacheSize */, loader);
  TEST(cache.IsValid(), ());
  cache.GetValue(1);
  cache.GetValue(2);
  cache.GetValue(3);
  TEST(cache.IsValid(), ());
  shouldLoadBeCalled = false;
  cache.GetValue(3);
  cache.GetValue(2);
  cache.GetValue(1);
  TEST(cache.IsValid(), ());
  shouldLoadBeCalled = true;
  cache.GetValue(4);
  TEST(cache.IsValid(), ());
  shouldLoadBeCalled = false;
  cache.GetValue(1);
  TEST(cache.IsValid(), ());
}
