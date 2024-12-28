#include "coding/compressed_bit_vector.hpp"

#include "coding/write_to_sink.hpp"

#include "base/assert.hpp"

#include <algorithm>
#include <bit>

namespace coding
{
using std::make_unique, std::max, std::min, std::unique_ptr, std::vector;

namespace
{
struct IntersectOp
{
  IntersectOp() {}

  unique_ptr<coding::CompressedBitVector> operator()(coding::DenseCBV const & a,
                                                     coding::DenseCBV const & b) const
  {
    size_t const sizeA = a.NumBitGroups();
    size_t const sizeB = b.NumBitGroups();
    vector<uint64_t> resGroups(min(sizeA, sizeB));
    for (size_t i = 0; i < resGroups.size(); ++i)
      resGroups[i] = a.GetBitGroup(i) & b.GetBitGroup(i);
    return coding::CompressedBitVectorBuilder::FromBitGroups(std::move(resGroups));
  }

  // The intersection of dense and sparse is always sparse.
  unique_ptr<coding::CompressedBitVector> operator()(coding::DenseCBV const & a,
                                                     coding::SparseCBV const & b) const
  {
    vector<uint64_t> resPos;
    for (size_t i = 0; i < b.PopCount(); ++i)
    {
      auto pos = b.Select(i);
      if (a.GetBit(pos))
        resPos.push_back(pos);
    }
    return make_unique<coding::SparseCBV>(std::move(resPos));
  }

  unique_ptr<coding::CompressedBitVector> operator()(coding::SparseCBV const & a,
                                                     coding::DenseCBV const & b) const
  {
    return operator()(b, a);
  }

  unique_ptr<coding::CompressedBitVector> operator()(coding::SparseCBV const & a,
                                                     coding::SparseCBV const & b) const
  {
    vector<uint64_t> resPos;
    set_intersection(a.Begin(), a.End(), b.Begin(), b.End(), back_inserter(resPos));
    return make_unique<coding::SparseCBV>(std::move(resPos));
  }
};

struct SubtractOp
{
  SubtractOp() {}

  unique_ptr<coding::CompressedBitVector> operator()(coding::DenseCBV const & a,
                                                     coding::DenseCBV const & b) const
  {
    size_t const sizeA = a.NumBitGroups();
    size_t const sizeB = b.NumBitGroups();
    vector<uint64_t> resGroups(min(sizeA, sizeB));
    for (size_t i = 0; i < resGroups.size(); ++i)
      resGroups[i] = a.GetBitGroup(i) & ~b.GetBitGroup(i);
    return CompressedBitVectorBuilder::FromBitGroups(std::move(resGroups));
  }

  unique_ptr<coding::CompressedBitVector> operator()(coding::DenseCBV const & a,
                                                     coding::SparseCBV const & b) const
  {
    vector<uint64_t> resGroups(a.NumBitGroups());

    size_t i = 0;
    auto j = b.Begin();
    for (; i < resGroups.size() && j < b.End(); ++i)
    {
      uint64_t const kBitsBegin = i * DenseCBV::kBlockSize;
      uint64_t const kBitsEnd = (i + 1) * DenseCBV::kBlockSize;

      uint64_t mask = 0;
      for (; j < b.End() && *j < kBitsEnd; ++j)
      {
        ASSERT_GREATER_OR_EQUAL(*j, kBitsBegin, ());
        mask |= static_cast<uint64_t>(1) << (*j - kBitsBegin);
      }

      resGroups[i] = a.GetBitGroup(i) & ~mask;
    }

    for (; i < resGroups.size(); ++i)
      resGroups[i] = a.GetBitGroup(i);

    return CompressedBitVectorBuilder::FromBitGroups(std::move(resGroups));
  }

  unique_ptr<coding::CompressedBitVector> operator()(coding::SparseCBV const & a,
                                                     coding::DenseCBV const & b) const
  {
    vector<uint64_t> resPos;
    copy_if(a.Begin(), a.End(), back_inserter(resPos), [&](uint64_t bit)
            {
              return !b.GetBit(bit);
            });
    return CompressedBitVectorBuilder::FromBitPositions(std::move(resPos));
  }

  unique_ptr<coding::CompressedBitVector> operator()(coding::SparseCBV const & a,
                                                     coding::SparseCBV const & b) const
  {
    vector<uint64_t> resPos;
    set_difference(a.Begin(), a.End(), b.Begin(), b.End(), back_inserter(resPos));
    return CompressedBitVectorBuilder::FromBitPositions(std::move(resPos));
  }
};

struct UnionOp
{
  UnionOp() {}

  unique_ptr<coding::CompressedBitVector> operator()(coding::DenseCBV const & a,
                                                     coding::DenseCBV const & b) const
  {
    size_t const sizeA = a.NumBitGroups();
    size_t const sizeB = b.NumBitGroups();

    size_t commonSize = min(sizeA, sizeB);
    size_t resultSize = max(sizeA, sizeB);
    vector<uint64_t> resGroups(resultSize);
    for (size_t i = 0; i < commonSize; ++i)
      resGroups[i] = a.GetBitGroup(i) | b.GetBitGroup(i);
    if (a.NumBitGroups() == resultSize)
    {
      for (size_t i = commonSize; i < resultSize; ++i)
        resGroups[i] = a.GetBitGroup(i);
    }
    else
    {
      for (size_t i = commonSize; i < resultSize; ++i)
        resGroups[i] = b.GetBitGroup(i);
    }
    return CompressedBitVectorBuilder::FromBitGroups(std::move(resGroups));
  }

  unique_ptr<coding::CompressedBitVector> operator()(coding::DenseCBV const & a,
                                                     coding::SparseCBV const & b) const
  {
    size_t const sizeA = a.NumBitGroups();
    size_t const sizeB =
        b.PopCount() == 0 ? 0
                          : static_cast<size_t>((b.Select(static_cast<size_t>(b.PopCount() - 1)) +
                                                 DenseCBV::kBlockSize - 1) /
                                                DenseCBV::kBlockSize);
    if (sizeB > sizeA)
    {
      vector<uint64_t> resPos;
      auto j = b.Begin();
      auto merge = [&](uint64_t va)
      {
        while (j < b.End() && *j < va)
        {
          resPos.push_back(*j);
          ++j;
        }
        resPos.push_back(va);
      };
      a.ForEach(merge);
      for (; j < b.End(); ++j)
        resPos.push_back(*j);
      return CompressedBitVectorBuilder::FromBitPositions(std::move(resPos));
    }

    vector<uint64_t> resGroups(sizeA);

    size_t i = 0;
    auto j = b.Begin();
    for (; i < sizeA || j < b.End(); ++i)
    {
      uint64_t const kBitsBegin = i * DenseCBV::kBlockSize;
      uint64_t const kBitsEnd = (i + 1) * DenseCBV::kBlockSize;

      uint64_t mask = i < sizeA ? a.GetBitGroup(i) : 0;
      for (; j < b.End() && *j < kBitsEnd; ++j)
      {
        ASSERT_GREATER_OR_EQUAL(*j, kBitsBegin, ());
        mask |= static_cast<uint64_t>(1) << (*j - kBitsBegin);
      }

      resGroups[i] = mask;
    }

    return CompressedBitVectorBuilder::FromBitGroups(std::move(resGroups));
  }

  unique_ptr<coding::CompressedBitVector> operator()(coding::SparseCBV const & a,
                                                     coding::DenseCBV const & b) const
  {
    return operator()(b, a);
  }

  unique_ptr<coding::CompressedBitVector> operator()(coding::SparseCBV const & a,
                                                     coding::SparseCBV const & b) const
  {
    vector<uint64_t> resPos;
    set_union(a.Begin(), a.End(), b.Begin(), b.End(), back_inserter(resPos));
    return CompressedBitVectorBuilder::FromBitPositions(std::move(resPos));
  }
};

template <typename TBinaryOp>
unique_ptr<coding::CompressedBitVector> Apply(TBinaryOp const & op, CompressedBitVector const & lhs,
                                              CompressedBitVector const & rhs)
{
  using strat = CompressedBitVector::StorageStrategy;
  auto const stratA = lhs.GetStorageStrategy();
  auto const stratB = rhs.GetStorageStrategy();
  if (stratA == strat::Dense && stratB == strat::Dense)
  {
    DenseCBV const & a = static_cast<DenseCBV const &>(lhs);
    DenseCBV const & b = static_cast<DenseCBV const &>(rhs);
    return op(a, b);
  }
  if (stratA == strat::Dense && stratB == strat::Sparse)
  {
    DenseCBV const & a = static_cast<DenseCBV const &>(lhs);
    SparseCBV const & b = static_cast<SparseCBV const &>(rhs);
    return op(a, b);
  }
  if (stratA == strat::Sparse && stratB == strat::Dense)
  {
    SparseCBV const & a = static_cast<SparseCBV const &>(lhs);
    DenseCBV const & b = static_cast<DenseCBV const &>(rhs);
    return op(a, b);
  }
  if (stratA == strat::Sparse && stratB == strat::Sparse)
  {
    SparseCBV const & a = static_cast<SparseCBV const &>(lhs);
    SparseCBV const & b = static_cast<SparseCBV const &>(rhs);
    return op(a, b);
  }

  return nullptr;
}

// Returns true if a bit vector with popCount bits set out of totalBits
// is fit to be represented as a DenseCBV. Note that we do not
// account for possible irregularities in the distribution of bits.
// In particular, we do not break the bit vector into blocks that are
// stored separately although this might turn out to be a good idea.
bool DenseEnough(uint64_t popCount, uint64_t totalBits)
{
  // Settle at 30% for now.
  return popCount * 10 >= totalBits * 3;
}

template <typename TBitPositions>
unique_ptr<CompressedBitVector> BuildFromBitPositions(TBitPositions && setBits)
{
  if (setBits.empty())
    return make_unique<SparseCBV>(std::forward<TBitPositions>(setBits));
  uint64_t const maxBit = *max_element(setBits.begin(), setBits.end());

  if (DenseEnough(setBits.size(), maxBit))
    return make_unique<DenseCBV>(std::forward<TBitPositions>(setBits));

  return make_unique<SparseCBV>(std::forward<TBitPositions>(setBits));
}
}  // namespace

// static
uint64_t const DenseCBV::kBlockSize;

DenseCBV::DenseCBV(vector<uint64_t> const & setBits)
{
  if (setBits.empty())
  {
    return;
  }
  uint64_t const maxBit = *max_element(setBits.begin(), setBits.end());
  size_t const sz = 1 + static_cast<size_t>(maxBit / kBlockSize);
  m_bitGroups.resize(sz);
  m_popCount = static_cast<uint64_t>(setBits.size());
  for (uint64_t pos : setBits)
  {
    m_bitGroups[static_cast<size_t>(pos / kBlockSize)] |= static_cast<uint64_t>(1)
                                                          << (pos % kBlockSize);
  }
}

// static
unique_ptr<DenseCBV> DenseCBV::BuildFromBitGroups(vector<uint64_t> && bitGroups)
{
  unique_ptr<DenseCBV> cbv(new DenseCBV());
  cbv->m_popCount = 0;
  for (size_t i = 0; i < bitGroups.size(); ++i)
    cbv->m_popCount += std::popcount(bitGroups[i]);
  cbv->m_bitGroups = std::move(bitGroups);
  return cbv;
}

uint64_t DenseCBV::GetBitGroup(size_t i) const
{
  return i < m_bitGroups.size() ? m_bitGroups[i] : 0;
}

uint64_t DenseCBV::PopCount() const { return m_popCount; }

bool DenseCBV::GetBit(uint64_t pos) const
{
  uint64_t bitGroup = GetBitGroup(static_cast<size_t>(pos / kBlockSize));
  return ((bitGroup >> (pos % kBlockSize)) & 1) > 0;
}

unique_ptr<CompressedBitVector> DenseCBV::LeaveFirstSetNBits(uint64_t n) const
{
  if (PopCount() <= n)
    return Clone();

  vector<uint64_t> groups;
  for (size_t i = 0; i < m_bitGroups.size() && n != 0; ++i)
  {
    uint64_t group = m_bitGroups[i];
    uint32_t const bits = std::popcount(group);
    if (bits <= n)
    {
      n -= bits;
      groups.push_back(group);
    }
    else
    {
      uint64_t part = 0;
      while (n != 0)
      {
        part = part | (group & -group);
        group = group & (group - 1);
        --n;
      }
      groups.push_back(part);
    }
  }
  return CompressedBitVectorBuilder::FromBitGroups(std::move(groups));
}

CompressedBitVector::StorageStrategy DenseCBV::GetStorageStrategy() const
{
  return CompressedBitVector::StorageStrategy::Dense;
}

void DenseCBV::Serialize(Writer & writer) const
{
  uint8_t header = static_cast<uint8_t>(GetStorageStrategy());
  WriteToSink(writer, header);
  rw::WriteVectorOfPOD(writer, m_bitGroups);
}

unique_ptr<CompressedBitVector> DenseCBV::Clone() const
{
  DenseCBV * cbv = new DenseCBV();
  cbv->m_popCount = m_popCount;
  cbv->m_bitGroups = m_bitGroups;
  return unique_ptr<CompressedBitVector>(cbv);
}

SparseCBV::SparseCBV(vector<uint64_t> const & setBits) : m_positions(setBits)
{
  ASSERT(is_sorted(m_positions.begin(), m_positions.end()), ());
}

SparseCBV::SparseCBV(vector<uint64_t> && setBits) : m_positions(std::move(setBits))
{
  ASSERT(is_sorted(m_positions.begin(), m_positions.end()), ());
}

uint64_t SparseCBV::Select(size_t i) const
{
  ASSERT_LESS(i, m_positions.size(), ());
  return m_positions[i];
}

uint64_t SparseCBV::PopCount() const { return m_positions.size(); }

bool SparseCBV::GetBit(uint64_t pos) const
{
  return binary_search(m_positions.begin(), m_positions.end(), pos);
}

unique_ptr<CompressedBitVector> SparseCBV::LeaveFirstSetNBits(uint64_t n) const
{
  if (PopCount() <= n)
    return Clone();
  vector<uint64_t> positions(m_positions.begin(), m_positions.begin() + static_cast<size_t>(n));
  return CompressedBitVectorBuilder::FromBitPositions(std::move(positions));
}

CompressedBitVector::StorageStrategy SparseCBV::GetStorageStrategy() const
{
  return CompressedBitVector::StorageStrategy::Sparse;
}

void SparseCBV::Serialize(Writer & writer) const
{
  uint8_t header = static_cast<uint8_t>(GetStorageStrategy());
  WriteToSink(writer, header);
  rw::WriteVectorOfPOD(writer, m_positions);
}

unique_ptr<CompressedBitVector> SparseCBV::Clone() const
{
  SparseCBV * cbv = new SparseCBV();
  cbv->m_positions = m_positions;
  return unique_ptr<CompressedBitVector>(cbv);
}

// static
unique_ptr<CompressedBitVector> CompressedBitVectorBuilder::FromBitPositions(
    vector<uint64_t> const & setBits)
{
  return BuildFromBitPositions(setBits);
}

// static
unique_ptr<CompressedBitVector> CompressedBitVectorBuilder::FromBitPositions(
    vector<uint64_t> && setBits)
{
  return BuildFromBitPositions(std::move(setBits));
}

// static
unique_ptr<CompressedBitVector> CompressedBitVectorBuilder::FromBitGroups(
    vector<uint64_t> && bitGroups)
{
  static uint64_t const kBlockSize = DenseCBV::kBlockSize;

  while (!bitGroups.empty() && bitGroups.back() == 0)
    bitGroups.pop_back();
  if (bitGroups.empty())
    return make_unique<SparseCBV>(std::move(bitGroups));

  uint64_t const maxBit = kBlockSize * (bitGroups.size() - 1) + bits::FloorLog(bitGroups.back());
  uint64_t popCount = 0;
  for (size_t i = 0; i < bitGroups.size(); ++i)
    popCount += std::popcount(bitGroups[i]);

  if (DenseEnough(popCount, maxBit))
    return DenseCBV::BuildFromBitGroups(std::move(bitGroups));

  vector<uint64_t> setBits;
  for (size_t i = 0; i < bitGroups.size(); ++i)
  {
    for (size_t j = 0; j < kBlockSize; ++j)
    {
      if (((bitGroups[i] >> j) & 1) > 0)
        setBits.push_back(kBlockSize * i + j);
    }
  }
  return make_unique<SparseCBV>(setBits);
}

std::string DebugPrint(CompressedBitVector::StorageStrategy strat)
{
  switch (strat)
  {
  case CompressedBitVector::StorageStrategy::Dense: return "Dense";
  case CompressedBitVector::StorageStrategy::Sparse: return "Sparse";
  }
  UNREACHABLE();
}

// static
unique_ptr<CompressedBitVector> CompressedBitVector::Intersect(CompressedBitVector const & lhs,
                                                               CompressedBitVector const & rhs)
{
  static IntersectOp const op;
  return Apply(op, lhs, rhs);
}

// static
unique_ptr<CompressedBitVector> CompressedBitVector::Subtract(CompressedBitVector const & lhs,
                                                              CompressedBitVector const & rhs)
{
  static SubtractOp const op;
  return Apply(op, lhs, rhs);
}

// static
unique_ptr<CompressedBitVector> CompressedBitVector::Union(CompressedBitVector const & lhs,
                                                           CompressedBitVector const & rhs)
{
  static UnionOp const op;
  return Apply(op, lhs, rhs);
}

// static
bool CompressedBitVector::IsEmpty(unique_ptr<CompressedBitVector> const & cbv)
{
  return !cbv || cbv->PopCount() == 0;
}

// static
bool CompressedBitVector::IsEmpty(CompressedBitVector const * cbv)
{
  return !cbv || cbv->PopCount() == 0;
}
}  // namespace coding
