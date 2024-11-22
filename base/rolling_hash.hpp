#pragma once

#include "base/assert.hpp"
#include "base/math.hpp"

#ifdef DEBUG
#include <queue>
#endif

template <typename T, typename HashT> class RabinKarpRollingHasher
{
public:
  using value_type = T;
  using hash_type = HashT;

  explicit RabinKarpRollingHasher(HashT multiplier)
    : m_multiplier(multiplier)
  {
  }

  template <typename Iter>
  hash_type Init(Iter it, uint64_t windowSize)
  {
    ASSERT_GREATER(m_windowSize, 0, ());
    m_windowSize = windowSize;
    m_removeMultiplier = math::PowUint(m_multiplier, m_windowSize - 1);
#ifdef DEBUG
    while (!m_queue.empty()) m_queue.pop();
#endif
    m_hash = 0;
    for (uint64_t i = 0; i < m_windowSize; ++it, ++i)
    {
      m_hash = m_hash * m_multiplier + *it;
#ifdef DEBUG
      m_queue.push(*it);
#endif
    }
    return m_hash;
  }

  hash_type Scroll(T const remove, T const add)
  {
    ASSERT_NOT_EQUAL(m_removeMultiplier, 0, (m_multiplier, m_windowSize, remove, add));
    ASSERT_NOT_EQUAL(m_windowSize, 0, (m_multiplier, remove, add));
#ifdef DEBUG
    ASSERT_EQUAL(m_queue.front(), remove, ());
    m_queue.pop();
    m_queue.push(add);
#endif
    m_hash -= m_removeMultiplier * remove;
    m_hash = m_hash * m_multiplier + add;
    return m_hash;
  }

private:
  hash_type m_hash = 0;
  hash_type m_multiplier = 1;
  hash_type m_removeMultiplier = 1;
  uint64_t m_windowSize = 1;

#ifdef DEBUG
  std::queue<value_type> m_queue;
#endif
};

class RabinKarpRollingHasher32 : public RabinKarpRollingHasher<uint32_t, uint32_t>
{
public:
  RabinKarpRollingHasher32()
    : RabinKarpRollingHasher(1103515245U) {}
};

class RabinKarpRollingHasher64 : public RabinKarpRollingHasher<uint64_t, uint64_t>
{
public:
  RabinKarpRollingHasher64()
    : RabinKarpRollingHasher(6364136223846793005ULL) {}
};

using RollingHasher32 = RabinKarpRollingHasher32;
using RollingHasher64 = RabinKarpRollingHasher64;
