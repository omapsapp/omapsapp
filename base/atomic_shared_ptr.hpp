#pragma once

#include "base/macros.hpp"

#include <memory>

namespace base
{
// Template which provides methods for concurrently using shared pointers.
template <typename T>
class AtomicSharedPtr final
{
public:
  using ContentType = T const;
  using ValueType = std::shared_ptr<ContentType>;

  AtomicSharedPtr() = default;

// TODO drop this condition and the else branch when Apple and Google will finally
// also support the full C++20 standard
// including the partial template specialization of `std::atomic` for `std::shared_ptr<T>`
// mandated by `P0718R2`:
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0718r2.html#3.2
// The support status can be also tracked at:
// https://en.cppreference.com/w/cpp/compiler_support/20#cpp_lib_atomic_shared_ptr_201711L
#if defined(_SHARED_PTR_ATOMIC_H)
  void Set(ValueType value) noexcept { m_wrapped.store(value); }
  ValueType Get() const noexcept { return m_wrapped.load(); }

private:
  std::atomic<ValueType> m_wrapped = std::make_shared<ContentType>();
#else
  void Set(ValueType value) noexcept { atomic_store(&m_wrapped, value); }                                                                                                                               
  ValueType Get() const noexcept { return atomic_load(&m_wrapped); }

 private:
  ValueType m_wrapped = std::make_shared<ContentType>();
#endif

  DISALLOW_COPY_AND_MOVE(AtomicSharedPtr);
};
}  // namespace base
