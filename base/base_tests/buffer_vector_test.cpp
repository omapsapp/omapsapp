#include "testing/testing.hpp"

#include "base/buffer_vector.hpp"
#include "base/string_utils.hpp"

#include <array>
#include <memory>
#include <numeric>

namespace
{
  template <class TCont>
  void CheckVector(TCont & cont, size_t count)
  {
    TEST_EQUAL ( cont.size(), count, () );
    for (size_t i = 0; i < count; ++i)
      TEST_EQUAL ( cont[i], i, () );
  }
}

UNIT_TEST(BufferVector_PushBackAndRealloc)
{
  using ElementT = std::vector<int>;
  ElementT const element({1, 2, 3});

  size_t constexpr kFixedSize = 2;
  {
    buffer_vector<ElementT, kFixedSize> v;
    v.append(kFixedSize, element);

    v.push_back(v[0]);
    TEST_EQUAL(v.size(), kFixedSize + 1, ());

    for (auto const & e : v)
      TEST_EQUAL(e, element, ());
  }

  {
    buffer_vector<ElementT, kFixedSize> v;
    v.append(kFixedSize, element);

    v.emplace_back(3, v[0][1]);
    TEST_EQUAL(v.size(), kFixedSize + 1, ());

    for (size_t i = 0; i < kFixedSize; ++i)
      TEST_EQUAL(v[i], element, ());
    TEST_EQUAL(v[kFixedSize], ElementT({2, 2, 2}), ());
  }
}

UNIT_TEST(BufferVector_Bounds)
{
  buffer_vector<size_t, 2> v;

  for (size_t i = 0; i < 5; ++i)
  {
    v.push_back(i);
    CheckVector(v, i+1);
  }

  v.resize(2);
  CheckVector(v, 2);

  v.resize(3);
  v[2] = 2;
  CheckVector(v, 3);

  v.resize(4);
  v[3] = 3;
  CheckVector(v, 4);

  v.resize(1);
  CheckVector(v, 1);

  v.resize(0);
  CheckVector(v, 0);
}

UNIT_TEST(BufferVector_Swap)
{
  typedef buffer_vector<size_t, 2> value_t;
  buffer_vector<value_t, 2> v1, v2;

  for (size_t i = 0; i < 5; ++i)
  {
    v1.push_back(value_t());
    v2.push_back(value_t());
  }

  // check swap of vectors
  value_t const * d1 = v1.data();
  value_t const * d2 = v2.data();

  swap(v1, v2);
  TEST_EQUAL ( d1, v2.data(), () );
  TEST_EQUAL ( d2, v1.data(), () );

  // check swap in resized data
  {
    v1[0].push_back(666);

    auto const * dd1 = v1[0].data();

    // resize from 5 to 1 => v[0] will stay at the same place
    v1.resize(1);
    TEST_EQUAL ( v1[0].size(), 1, () );
    TEST_EQUAL ( v1[0][0], 666, () );
    TEST_EQUAL ( dd1, v1[0].data(), () );

    v1.resize(7);
    TEST_EQUAL ( v1[0].size(), 1, () );
    TEST_EQUAL ( v1[0][0], 666, () );
  }

  {
    for (size_t i = 0; i < 5; ++i)
      v2[0].push_back(i);

    // inner dynamic buffer should be swapped during resizing
    // (??? but it's not specified by standart of std::vector ???)
    auto const * dd2 = v2[0].data();

    v2.resize(1);
    TEST_EQUAL ( v2[0].size(), 5, () );
    TEST_EQUAL ( dd2, v2[0].data(), () );

    v1.resize(7);
    TEST_EQUAL ( v2[0].size(), 5, () );
    TEST_EQUAL ( dd2, v2[0].data(), () );
  }

  // check resize from static to dynamic buffer
  buffer_vector<value_t, 2> v3;
  v3.push_back(value_t());

  {
    for (size_t i = 0; i < 5; ++i)
      v3[0].push_back(i);

    auto const * dd3 = v3[0].data();

    // resize from static to dynamic buffer => v3[0] will stay at the same place
    v1.resize(7);
    TEST_EQUAL ( v3[0].size(), 5, () );
    TEST_EQUAL ( dd3, v3[0].data(), () );
  }
}

UNIT_TEST(BufferVector_Resize)
{
  for (size_t size = 0; size < 20; ++size)
  {
    buffer_vector<int, 5> v;
    v.resize(size, 3);
    for (size_t i = 0; i < size; ++i)
      TEST_EQUAL(v[i], 3, ());
  }
}

UNIT_TEST(BufferVector_Insert)
{
  for (size_t initialLength = 0; initialLength < 20; ++initialLength)
  {
    for (size_t insertLength = 0; insertLength < 20; ++insertLength)
    {
      for (size_t insertPos = 0; insertPos <= initialLength; ++insertPos)
      {
        buffer_vector<char, 5> b;
        std::vector<char> v;
        for (size_t i = 0; i < initialLength; ++i)
        {
          b.push_back('A' + i);
          v.push_back('A' + i);
        }

        std::vector<int> dataToInsert(insertLength);
        std::iota(dataToInsert.begin(), dataToInsert.end(), 'a');

        b.insert(b.begin() + insertPos, dataToInsert.begin(), dataToInsert.end());
        v.insert(v.begin() + insertPos, dataToInsert.begin(), dataToInsert.end());

        std::vector<char> result(b.begin(), b.end());
        TEST_EQUAL(v, result, (initialLength, insertLength, insertPos));
      }
    }
  }
}

UNIT_TEST(BufferVector_InsertSingleValue)
{
  buffer_vector<char, 3> v;
  v.insert(v.end(), 'x');
  TEST_EQUAL(v.size(), 1, ());
  TEST_EQUAL(v[0], 'x', ());

  v.insert(v.begin(), 'y');
  TEST_EQUAL(v.size(), 2, ());
  TEST_EQUAL(v[0], 'y', ());
  TEST_EQUAL(v[1], 'x', ());

  v.insert(v.begin() + 1, 'z');
  TEST_EQUAL(v.size(), 3, ());
  TEST_EQUAL(v[0], 'y', ());
  TEST_EQUAL(v[1], 'z', ());
  TEST_EQUAL(v[2], 'x', ());
  // Switch to dynamic.
  v.insert(v.begin() + 1, 'q');
  TEST_EQUAL(v.size(), 4, ());
  TEST_EQUAL(v[0], 'y', ());
  TEST_EQUAL(v[1], 'q', ());
  TEST_EQUAL(v[2], 'z', ());
  TEST_EQUAL(v[3], 'x', ());

  v.insert(v.end() - 1, 'c');
  TEST_EQUAL(v[3], 'c', ());
  TEST_EQUAL(v[4], 'x', ());
}

UNIT_TEST(BufferVector_Append)
{
  for (size_t initialLength = 0; initialLength < 20; ++initialLength)
  {
    for (size_t insertLength = 0; insertLength < 20; ++insertLength)
    {
      buffer_vector<char, 5> b;
      std::vector<char> v;
      for (size_t i = 0; i < initialLength; ++i)
      {
        b.push_back('A' + i);
        v.push_back('A' + i);
      }

      std::vector<int> dataToInsert(insertLength);
      std::iota(dataToInsert.begin(), dataToInsert.end(), 'a');

      b.append(dataToInsert.begin(), dataToInsert.end());
      v.insert(v.end(), dataToInsert.begin(), dataToInsert.end());

      std::vector<char> result(b.begin(), b.end());
      TEST_EQUAL(v, result, (initialLength, insertLength));
    }
  }
}

UNIT_TEST(BufferVector_PopBack)
{
  for (size_t len = 1; len < 6; ++len)
  {
    buffer_vector<size_t, 3> v;
    for (size_t i = 0; i < len; ++i)
      v.push_back(i);
    for (size_t i = len; i > 0; --i)
    {
      TEST(!v.empty(), (len, i));
      TEST_EQUAL(v.size(), i, ());
      TEST_EQUAL(v.front(), 0, ());
      TEST_EQUAL(v.back(), i-1, ());
      v.pop_back();
      TEST_EQUAL(v.size(), i-1, ());
    }
    TEST(v.empty(), ());
  }
}

UNIT_TEST(BufferVector_Assign)
{
  int const arr5[] = {1, 2, 3, 4, 5};
  buffer_vector<int, 5> v(&arr5[0], &arr5[0] + ARRAY_SIZE(arr5));
  for (size_t i = 0; i < ARRAY_SIZE(arr5); ++i)
  {
    TEST_EQUAL(arr5[i], v[i], ());
  }

  int const arr10[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
  v.assign(&arr10[0], &arr10[0] + ARRAY_SIZE(arr10));
  for (size_t i = 0; i < ARRAY_SIZE(arr10); ++i)
  {
    TEST_EQUAL(arr10[i], v[i], ());
  }
}

UNIT_TEST(BufferVector_Equality)
{
  int const arr5[] = {1, 2, 3, 4, 5};
  buffer_vector<int, 5> v1(&arr5[0], &arr5[0] + ARRAY_SIZE(arr5));
  buffer_vector<int, 10> v2(&arr5[0], &arr5[0] + ARRAY_SIZE(arr5));
  buffer_vector<int, 3> v3(&arr5[0], &arr5[0] + ARRAY_SIZE(arr5));
  TEST_EQUAL(v1, v2, ());
  TEST_EQUAL(v1, v3, ());
  TEST_EQUAL(v2, v3, ());
  v1.push_back(999);
  TEST_NOT_EQUAL(v1, v2, ());
}

namespace
{

struct CopyCtorChecker
{
  std::string m_s;

  CopyCtorChecker() = default;
  explicit CopyCtorChecker(char const * s) : m_s(s) {}

  CopyCtorChecker(CopyCtorChecker const & rhs)
  {
    TEST(rhs.m_s.empty(), ("Copy ctor is called only in resize with default element"));
  }
  CopyCtorChecker(CopyCtorChecker && rhs) = default;

  CopyCtorChecker & operator=(CopyCtorChecker &&) = default;
  CopyCtorChecker & operator=(CopyCtorChecker const &)
  {
    TEST(false, ("Assigment operator should not be called"));
    return *this;
  }
};

// void swap(CopyCtorChecker & r1, CopyCtorChecker & r2)
// {
//   r1.m_s.swap(r2.m_s);
// }

typedef buffer_vector<CopyCtorChecker, 2> VectorT;

VectorT GetVector()
{
  VectorT v;
  v.emplace_back("0");
  v.emplace_back("1");
  return v;
}

void TestVector(VectorT const & v, size_t sz)
{
  TEST_EQUAL(v.size(), sz, ());
  for (size_t i = 0; i < sz; ++i)
    TEST_EQUAL(v[i].m_s, strings::to_string(i), ());
}

} // namespace

UNIT_TEST(BufferVector_Move)
{
  VectorT v1 = GetVector();
  TestVector(v1, 2);

  // Make intermediate array to avoid warning (moving to itself).
  std::array<VectorT *, 2> arr = { &v1, &v1 };
  *arr[0] = std::move(*arr[1]);
  TestVector(v1, 2);

  v1.emplace_back("2");
  TestVector(v1, 3);

  VectorT v2(std::move(v1));
  TestVector(v2, 3);
  TestVector(v1, 0);

  v1 = std::move(v2);
  TestVector(v1, 3);
  TestVector(v2, 0);
}

UNIT_TEST(BufferVector_EraseIf)
{
  buffer_vector<int, 2> v;
  v.push_back(1);
  v.push_back(2);
  v.erase_if([] (int x) { return x == 1; });
  TEST_EQUAL(v.size(), 1, ());
  TEST_EQUAL(v[0], 2, ());

  v.push_back(3);
  v.push_back(4);
  v.erase_if([] (int x) { return x == 3; });
  TEST_EQUAL(v.size(), 2, ());
  TEST_EQUAL(v[0], 2, ());
  TEST_EQUAL(v[1], 4, ());

  v.erase_if([] (int) { return true; });
  TEST_EQUAL(v.size(), 0, ());
}

UNIT_TEST(BufferVector_OnlyMoveableItems)
{
  buffer_vector<std::unique_ptr<size_t>, 4> v;

  for (size_t i = 0; i < 10; ++i)
    v.emplace_back(std::make_unique<size_t>(i));

  TEST_EQUAL(v.size(), 10, ());
  for (size_t i = 0; i < 10; ++i)
    TEST_EQUAL(*v[i], i, ());
}

UNIT_TEST(BufferVector_Erase)
{
  buffer_vector<int, 32> v1;
  std::vector<int> v2;
  for (int i = 1; i < 100; ++i)
  {
    v1.push_back(i);
    v2.push_back(i);
  }

  while (v1.size() > 1)
  {
    v1.erase(v1.begin() + v1.size() / 3, v1.begin() + 2 * v1.size () / 3);
    v2.erase(v2.begin() + v2.size() / 3, v2.begin() + 2 * v2.size () / 3);

    TEST_EQUAL(v1.size(), v2.size(), ());
    for (size_t i = 0; i < v1.size(); ++i)
      TEST_EQUAL(v1[i], v2[i], ());
  }
}

UNIT_TEST(BufferVector_StringView)
{
  {
    buffer_vector<std::string_view::value_type, 4> bv = {'a', 'b', 'c', 'd'};
    std::string_view constexpr svStack = "abcd";
    TEST_EQUAL(svStack, std::string_view{bv}, ());

    bv.push_back('e');
    std::string_view constexpr svHeap = "abcde";
    TEST_EQUAL(svHeap, std::string_view{bv}, ());
  }

  {
    buffer_vector<std::u16string_view::value_type, 4> bv = {u'à', u'b', u'ç', u'ð'};
    std::u16string_view constexpr svStack = u"àbçð";
    TEST_EQUAL(svStack, std::u16string_view{bv}, ());

    bv.push_back(u'ë');
    std::u16string_view constexpr svHeap = u"àbçðë";
    TEST_EQUAL(svHeap, std::u16string_view{bv}, ());
  }

  {
    buffer_vector<std::u32string_view::value_type, 4> bv = {U'à', U'b', U'ç', U'ð'};
    std::u32string_view constexpr svStack = U"àbçð";
    TEST_EQUAL(svStack, std::u32string_view{bv}, ());

    bv.push_back(U'ë');
    std::u32string_view constexpr svHeap = U"àbçðë";
    TEST_EQUAL(svHeap, std::u32string_view{bv}, ());
  }
}
