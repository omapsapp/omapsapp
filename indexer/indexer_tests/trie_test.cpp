#include "testing/testing.hpp"

#include "indexer/trie.hpp"
#include "indexer/trie_builder.hpp"
#include "indexer/trie_reader.hpp"

#include "coding/byte_stream.hpp"
#include "coding/reader.hpp"
#include "coding/write_to_sink.hpp"

#include "base/logging.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <boost/utility/binary.hpp>

namespace trie_test
{
using namespace std;

struct ChildNodeInfo
{
  ChildNodeInfo(bool isLeaf, uint32_t size, char const * edge) : m_isLeaf(isLeaf), m_size(size)
  {
    while (*edge)
      m_edge.push_back(*edge++);
  }

  uint32_t Size() const { return m_size; }
  bool IsLeaf() const { return m_isLeaf; }
  trie::TrieChar const * GetEdge() const { return &m_edge[0]; }
  size_t GetEdgeSize() const { return m_edge.size(); }

  bool m_isLeaf;
  uint32_t m_size;
  vector<trie::TrieChar> m_edge;
};

// The SingleValueSerializer and ValueList classes are similar to
// those in indexer/search_index_values.hpp.
template <typename Primitive>
class SingleValueSerializer
{
public:
#if !defined(OMIM_OS_LINUX)
  static_assert(std::is_trivially_copyable<Primitive>::value, "");
#endif

  template <typename Sink>
  void Serialize(Sink & sink, Primitive const & v) const
  {
    WriteToSink(sink, v);
  }
};

template <typename Primitive>
class ValueList
{
public:
  using Value = Primitive;
  using Serializer = SingleValueSerializer<Value>;

#if !defined(OMIM_OS_LINUX)
  static_assert(std::is_trivially_copyable<Primitive>::value, "");
#endif

  void Init(vector<Value> const & values) { m_values = values; }

  size_t Size() const { return m_values.size(); }

  bool IsEmpty() const { return m_values.empty(); }

  template <typename Sink>
  void Serialize(Sink & sink, Serializer const & /* serializer */) const
  {
    for (auto const & value : m_values)
      WriteToSink(sink, value);
  }

  template <typename Source>
  void Deserialize(Source & src, uint32_t valueCount, Serializer const & /* serializer */)
  {
    m_values.resize(valueCount);
    for (size_t i = 0; i < valueCount; ++i)
      m_values[i] = ReadPrimitiveFromSource<Value>(src);
  }

  template <typename Source>
  void Deserialize(Source & source, Serializer const & /* serializer */)
  {
    m_values.clear();
    while (source.Size() > 0)
      m_values.emplace_back(ReadPrimitiveFromSource<Value>(source));
  }

  template <typename ToDo>
  void ForEach(ToDo && toDo) const
  {
    for (auto const & value : m_values)
      toDo(value);
  }

private:
  vector<Value> m_values;
};

#define ZENC bits::ZigZagEncode
#define MKSC(x) static_cast<signed char>(x)
#define MKUC(x) static_cast<uint8_t>(x)

UNIT_TEST(TrieBuilder_WriteNode_Smoke)
{
  vector<uint8_t> buf;
  PushBackByteSink<vector<uint8_t>> sink(buf);
  ChildNodeInfo children[] = {
      ChildNodeInfo(true, 1, "1A"), ChildNodeInfo(false, 2, "B"), ChildNodeInfo(false, 3, "zz"),
      ChildNodeInfo(true, 4,
                    "abcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghijabcdefghij"),
      ChildNodeInfo(true, 5, "a")};

  ValueList<char> valueList;
  valueList.Init({'1', '2', '3'});
  trie::WriteNode(sink, SingleValueSerializer<char>(), 0, valueList, &children[0],
                  &children[0] + ARRAY_SIZE(children));
  uint8_t const expected[] = {
      BOOST_BINARY(11000101),  // Header: [0b11] [0b000101]
      3,                       // Number of values
      '1', '2', '3',           // Values
      BOOST_BINARY(10000001),  // Child 1: header: [+leaf] [-supershort]  [2 symbols]
      MKUC(ZENC(MKSC('1'))), MKUC(ZENC(MKSC('A') - MKSC('1'))),  // Child 1: edge
      1,                                                         // Child 1: size
      MKUC(64 | ZENC(MKSC('B') - MKSC('1'))),  // Child 2: header: [-leaf] [+supershort]
      2,                                       // Child 2: size
      BOOST_BINARY(00000001),                // Child 3: header: [-leaf] [-supershort]  [2 symbols]
      MKUC(ZENC(MKSC('z') - MKSC('B'))), 0,  // Child 3: edge
      3,                                     // Child 3: size
      BOOST_BINARY(10111111),  // Child 4: header: [+leaf] [-supershort]  [>= 63 symbols]
      69,                      // Child 4: edgeSize - 1
      MKUC(ZENC(MKSC('a') - MKSC('z'))), 2, 2, 2, 2, 2, 2, 2, 2, 2,  // Child 4: edge
      MKUC(ZENC(MKSC('a') - MKSC('j'))), 2, 2, 2, 2, 2, 2, 2, 2, 2,  // Child 4: edge
      MKUC(ZENC(MKSC('a') - MKSC('j'))), 2, 2, 2, 2, 2, 2, 2, 2, 2,  // Child 4: edge
      MKUC(ZENC(MKSC('a') - MKSC('j'))), 2, 2, 2, 2, 2, 2, 2, 2, 2,  // Child 4: edge
      MKUC(ZENC(MKSC('a') - MKSC('j'))), 2, 2, 2, 2, 2, 2, 2, 2, 2,  // Child 4: edge
      MKUC(ZENC(MKSC('a') - MKSC('j'))), 2, 2, 2, 2, 2, 2, 2, 2, 2,  // Child 4: edge
      MKUC(ZENC(MKSC('a') - MKSC('j'))), 2, 2, 2, 2, 2, 2, 2, 2, 2,  // Child 4: edge
      4,                                                             // Child 4: size
      MKUC(BOOST_BINARY(11000000) | ZENC(0)),  // Child 5: header: [+leaf] [+supershort]
  };

  TEST_EQUAL(buf, vector<uint8_t>(&expected[0], &expected[0] + ARRAY_SIZE(expected)), ());
}

UNIT_TEST(TrieBuilder_Build)
{
  uint32_t const kBase = 3;
  int const kMaxLen = 3;

  vector<string> possibleStrings(1, string{});
  for (int len = 1; len <= kMaxLen; ++len)
  {
    for (uint32_t i = 0, p = math::PowUint(kBase, len); i < p; ++i)
    {
      string s(len, 'A');
      uint32_t t = i;
      for (int l = len - 1; l >= 0; --l, t /= kBase)
        s[l] += (t % kBase);
      possibleStrings.push_back(s);
    }
  }
  sort(possibleStrings.begin(), possibleStrings.end());
  // LOG(LINFO, (possibleStrings));

  int const count = static_cast<int>(possibleStrings.size());
  for (int i0 = -1; i0 < count; ++i0)
  {
    for (int i1 = i0; i1 < count; ++i1)
    {
      for (int i2 = i1; i2 < count; ++i2)
      {
        using Key = buffer_vector<trie::TrieChar, 8>;
        using Value = uint32_t;
        using KeyValuePair = pair<Key, Value>;

        vector<KeyValuePair> v;
        auto makeKey = [](string const & s)
        {
          return Key(s.begin(), s.end());
        };
        if (i0 >= 0)
          v.emplace_back(makeKey(possibleStrings[i0]), i0);
        if (i1 >= 0)
          v.emplace_back(makeKey(possibleStrings[i1]), i1 + 10);
        if (i2 >= 0)
          v.emplace_back(makeKey(possibleStrings[i2]), i2 + 100);
        vector<string> vs;
        for (size_t i = 0; i < v.size(); ++i)
          vs.push_back(string(v[i].first.begin(), v[i].first.end()));

        vector<uint8_t> buf;
        PushBackByteSink<vector<uint8_t>> sink(buf);
        SingleValueSerializer<uint32_t> serializer;
        trie::Build<PushBackByteSink<vector<uint8_t>>, Key, ValueList<uint32_t>,
                    SingleValueSerializer<uint32_t>>(sink, serializer, v);
        reverse(buf.begin(), buf.end());

        MemReader memReader = MemReader(&buf[0], buf.size());
        auto const root = trie::ReadTrie<MemReader, ValueList<uint32_t>>(memReader, serializer);
        vector<KeyValuePair> res;
        auto addKeyValuePair = [&res](Key const & k, Value const & v)
        {
          res.emplace_back(k, v);
        };
        trie::ForEachRef(*root, addKeyValuePair, Key{});
        sort(res.begin(), res.end());
        TEST_EQUAL(v, res, ());
      }
    }
  }
}

} // namespace trie_test
