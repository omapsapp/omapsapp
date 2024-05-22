#include "testing/testing.hpp"

#include "drape/drape_tests/gl_mock_functions.hpp"
#include "drape/drape_tests/memory_comparer.hpp"
#include "drape/drape_tests/testing_graphics_context.hpp"

#include "drape/batcher.hpp"
#include "drape/gl_constants.hpp"
#include "drape/index_storage.hpp"
#include "drape/vertex_array_buffer.hpp"

#include "base/stl_helpers.hpp"

#include <cstring>
#include <functional>
#include <vector>

#include <gmock/gmock.h>

using testing::_;
using testing::Return;
using testing::InSequence;
using testing::Invoke;
using testing::IgnoreResult;
using testing::AnyOf;
using namespace dp;
using namespace std::placeholders;

namespace
{
struct VAOAcceptor
{
  virtual void FlushFullBucket(RenderState const & /* state */, drape_ptr<RenderBucket> && bucket)
  {
    m_vao.push_back(std::move(bucket));
  }

  std::vector<drape_ptr<RenderBucket>> m_vao;
};

class TestExtension : public dp::BaseRenderStateExtension
{
public:
  bool Less(ref_ptr<dp::BaseRenderStateExtension>) const override { return false; }
  bool Equal(ref_ptr<dp::BaseRenderStateExtension>) const override { return true; }
};

class BatcherExpectations
{
public:
  BatcherExpectations()
    : m_indexBufferID(1)
    , m_dataBufferID(2)
  {}

  template <typename TBatcherCall>
  void RunTest(float * vertexes, void * indexes,
               uint32_t vertexCount, uint8_t vertexComponentCount,
               uint32_t indexCount, TBatcherCall const & fn)
  {
    uint32_t const vertexSize = vertexCount * vertexComponentCount;
    MemoryComparer const dataCmp(vertexes, vertexSize * sizeof(float));
    MemoryComparer const indexCmp(indexes, indexCount * dp::IndexStorage::SizeOfIndex());

    ExpectBufferCreation(vertexSize, indexCount, indexCmp, dataCmp);

    auto renderState = make_unique_dp<TestExtension>();
    auto state = RenderState(0, make_ref(renderState));

    BindingInfo binding(1);
    BindingDecl & decl = binding.GetBindingDecl(0);
    decl.m_attributeName = "position";
    decl.m_componentCount = vertexComponentCount;
    decl.m_componentType = gl_const::GLFloatType;
    decl.m_offset = 0;
    decl.m_stride = 0;

    AttributeProvider provider(1, vertexCount);
    provider.InitStream(0, binding, make_ref(vertexes));

    TestingGraphicsContext context;
    VAOAcceptor vaoAcceptor;
    Batcher batcher(65000, 65000);
    batcher.StartSession(std::bind(&VAOAcceptor::FlushFullBucket, &vaoAcceptor, _1, _2));
    fn(&batcher, state, make_ref(&provider));
    batcher.EndSession(make_ref(&context));

    ExpectBufferDeletion();

    for (size_t i = 0; i < vaoAcceptor.m_vao.size(); ++i)
      vaoAcceptor.m_vao[i].reset();
  }

  void ExpectBufferCreation(uint32_t vertexCount, uint32_t indexCount,
                            MemoryComparer const & indexCmp, MemoryComparer const & vertexCmp)
  {
    InSequence seq;

    // data buffer creation
    EXPECTGL(glGenBuffer()).WillOnce(Return(m_dataBufferID));
    EXPECTGL(glBindBuffer(m_dataBufferID, gl_const::GLArrayBuffer));
    EXPECTGL(glBufferData(gl_const::GLArrayBuffer, vertexCount * sizeof(float), _,
                          gl_const::GLDynamicDraw))
        .WillOnce(Invoke(&vertexCmp, &MemoryComparer::cmpSubBuffer));

    // Index buffer creation
    EXPECTGL(glGenBuffer()).WillOnce(Return(m_indexBufferID));
    EXPECTGL(glBindBuffer(m_indexBufferID, gl_const::GLElementArrayBuffer));
    EXPECTGL(glBufferData(gl_const::GLElementArrayBuffer,
                          indexCount * dp::IndexStorage::SizeOfIndex(), _, gl_const::GLDynamicDraw))
        .WillOnce(Invoke(&indexCmp, &MemoryComparer::cmpSubBuffer));

    EXPECTGL(glBindBuffer(0, gl_const::GLElementArrayBuffer));
    EXPECTGL(glBindBuffer(0, gl_const::GLArrayBuffer));
  }

  void ExpectBufferDeletion()
  {
    InSequence seq;
    EXPECTGL(glBindBuffer(0, gl_const::GLElementArrayBuffer));
    EXPECTGL(glDeleteBuffer(m_indexBufferID));
    EXPECTGL(glBindBuffer(0, gl_const::GLArrayBuffer));
    EXPECTGL(glDeleteBuffer(m_dataBufferID));
  }

private:
  uint32_t m_indexBufferID = 0;
  uint32_t m_dataBufferID = 0;
};

} // namespace

UNIT_TEST(BatchLists_Test)
{
  TestingGraphicsContext context;
  uint32_t const kVerticesCount = 12;
  uint32_t const kFloatsCount = 3 * 12; // 3 component on each vertex.
  float data[kFloatsCount];
  for (uint32_t i = 0; i < kFloatsCount; ++i)
    data[i] = static_cast<float>(i);

  std::vector<uint32_t> indexesRaw(kVerticesCount);
  for (uint32_t i = 0; i < kVerticesCount; ++i)
    indexesRaw[i] = i;
  dp::IndexStorage indexes(std::move(indexesRaw));

  BatcherExpectations expectations;
  auto fn = [&context](Batcher * batcher, RenderState const & state, ref_ptr<AttributeProvider> p)
  {
    batcher->InsertTriangleList(make_ref(&context), state, p);
  };
  expectations.RunTest(data, indexes.GetRaw(), kVerticesCount, 3, kVerticesCount, fn);
}

UNIT_TEST(BatchListOfStript_4stride)
{
  TestingGraphicsContext context;
  uint32_t const kVerticesCount = 12;
  uint32_t const kIndicesCount = 18;

  float data[3 * kVerticesCount];
  for (uint32_t i = 0; i < kVerticesCount * 3; ++i)
    data[i] = static_cast<float>(i);

  std::vector<uint32_t> indexesRaw = {0, 1, 2, 1, 3, 2, 4, 5, 6, 5, 7, 6, 8, 9, 10, 9, 11, 10};
  dp::IndexStorage indexes(std::move(indexesRaw));

  BatcherExpectations expectations;
  auto fn = [&context](Batcher * batcher, RenderState const & state, ref_ptr<AttributeProvider> p)
  {
    batcher->InsertListOfStrip(make_ref(&context), state, p, dp::Batcher::VertexPerQuad);
  };

  expectations.RunTest(data, indexes.GetRaw(), kVerticesCount, 3, kIndicesCount, fn);
}

UNIT_TEST(BatchListOfStript_5stride)
{
  TestingGraphicsContext context;
  uint32_t const kVerticesCount = 15;
  uint32_t const kIndicesCount = 27;

  float data[3 * kVerticesCount];
  for (uint32_t i = 0; i < kVerticesCount * 3; ++i)
    data[i] = static_cast<float>(i);

  std::vector<uint32_t> indexesRaw =
  { 0, 1, 2,
    1, 3, 2,
    2, 3, 4,
    5, 6, 7,
    6, 8, 7,
    7, 8, 9,
    10, 11, 12,
    11, 13, 12,
    12, 13, 14
  };
  dp::IndexStorage indexes(std::move(indexesRaw));

  BatcherExpectations expectations;
  auto fn = [&context](Batcher * batcher, RenderState const & state, ref_ptr<AttributeProvider> p)
  {
    batcher->InsertListOfStrip(make_ref(&context), state, p, 5);
  };
  expectations.RunTest(data, indexes.GetRaw(), kVerticesCount, 3, kIndicesCount, fn);
}

UNIT_TEST(BatchListOfStript_6stride)
{
  TestingGraphicsContext context;
  uint32_t const kVerticesCount = 18;
  uint32_t const kIndicesCount = 36;

  float data[3 * kVerticesCount];
  for (uint32_t i = 0; i < kVerticesCount * 3; ++i)
    data[i] = static_cast<float>(i);

  std::vector<uint32_t> indexesRaw =
  {
    0, 1, 2,
    1, 3, 2,
    2, 3, 4,
    3, 5, 4,
    6, 7, 8,
    7, 9, 8,
    8, 9, 10,
    9, 11, 10,
    12, 13, 14,
    13, 15, 14,
    14, 15, 16,
    15, 17, 16
  };
  dp::IndexStorage indexes(std::move(indexesRaw));

  BatcherExpectations expectations;
  auto fn = [&context](Batcher * batcher, RenderState const & state, ref_ptr<AttributeProvider> p)
  {
    batcher->InsertListOfStrip(make_ref(&context), state, p, 6);
  };
  expectations.RunTest(data, indexes.GetRaw(), kVerticesCount, 3, kIndicesCount, fn);
}

namespace
{
class PartialBatcherTest
{
public:
  struct BufferNode
  {
    BufferNode(uint32_t indexByteCount, uint32_t vertexByteCount,
               void * indexData, void * vertexData)
      : m_indexByteCount(indexByteCount)
      , m_vertexByteCount(vertexByteCount)
      , m_indexData(indexData)
      , m_vertexData(vertexData)
      , m_indexBufferID(0)
      , m_vertexBufferID(0)
    {}

    uint32_t m_indexByteCount;
    uint32_t m_vertexByteCount;
    void * m_indexData;
    void * m_vertexData;
    uint32_t m_indexBufferID;
    uint32_t m_vertexBufferID;
  };

  PartialBatcherTest() = default;

  ~PartialBatcherTest()
  {
    std::for_each(m_comparators.begin(), m_comparators.end(), base::DeleteFunctor());
  }

  void AddBufferNode(BufferNode const & node)
  {
    m_nodes.push_back(node);
    BufferNode & currentNode = m_nodes.back();
    currentNode.m_indexBufferID = m_bufferIDCounter++;
    currentNode.m_vertexBufferID = m_bufferIDCounter++;

    // data buffer creation
    EXPECTGL(glGenBuffer()).WillOnce(Return(currentNode.m_vertexBufferID));
    EXPECTGL(glBindBuffer(currentNode.m_vertexBufferID, gl_const::GLArrayBuffer));

    m_comparators.push_back(
        new MemoryComparer(currentNode.m_vertexData, currentNode.m_vertexByteCount));
    MemoryComparer * vertexComparer = m_comparators.back();

    EXPECTGL(glBufferData(gl_const::GLArrayBuffer, currentNode.m_vertexByteCount, _,
                          gl_const::GLDynamicDraw))
        .WillOnce(Invoke(vertexComparer, &MemoryComparer::cmpSubBuffer));

    // Index buffer creation
    EXPECTGL(glGenBuffer()).WillOnce(Return(currentNode.m_indexBufferID));
    EXPECTGL(glBindBuffer(currentNode.m_indexBufferID, gl_const::GLElementArrayBuffer));

    m_comparators.push_back(
        new MemoryComparer(currentNode.m_indexData, currentNode.m_indexByteCount));
    MemoryComparer * indexComparer = m_comparators.back();

    EXPECTGL(glBufferData(gl_const::GLElementArrayBuffer, currentNode.m_indexByteCount, _,
                          gl_const::GLDynamicDraw))
        .WillOnce(Invoke(indexComparer, &MemoryComparer::cmpSubBuffer));

    EXPECTGL(glBindBuffer(0, gl_const::GLElementArrayBuffer));
    EXPECTGL(glBindBuffer(0, gl_const::GLArrayBuffer));
  }

  void CloseExpection()
  {
    for (auto const & node : m_nodes)
    {
      EXPECTGL(glBindBuffer(0, gl_const::GLElementArrayBuffer));
      EXPECTGL(glDeleteBuffer(node.m_indexBufferID));
      EXPECTGL(glBindBuffer(0, gl_const::GLArrayBuffer));
      EXPECTGL(glDeleteBuffer(node.m_vertexBufferID));
    }
  }

private:
  uint32_t m_bufferIDCounter = 1;

  std::vector<BufferNode> m_nodes;
  std::vector<MemoryComparer *> m_comparators;
};
}  // namespace

UNIT_TEST(BatchListOfStript_partial)
{
  TestingGraphicsContext context;
  uint32_t const VertexCount = 16;
  uint32_t const ComponentCount = 3;
  uint32_t const VertexArraySize = VertexCount * ComponentCount;
  uint32_t const IndexCount = 24;

  uint32_t const FirstBufferVertexPortion = 12;
  uint32_t const SecondBufferVertexPortion = VertexCount - FirstBufferVertexPortion;
  uint32_t const FirstBufferIndexPortion = 18;
  uint32_t const SecondBufferIndexPortion = IndexCount - FirstBufferIndexPortion;

  float vertexData[VertexArraySize];
  for (uint32_t i = 0; i < VertexArraySize; ++i)
    vertexData[i] = static_cast<float>(i);

  std::vector<uint32_t> indexDataRaw =
  { 0, 1, 2,
    1, 3, 2,
    4, 5, 6,
    5, 7, 6,
    8, 9, 10,
    9, 11, 10,
    0, 1, 2, // start new buffer
    1, 3, 2
  };
  dp::IndexStorage indexData(std::move(indexDataRaw));

  PartialBatcherTest::BufferNode node1(FirstBufferIndexPortion * dp::IndexStorage::SizeOfIndex(),
                                       FirstBufferVertexPortion * ComponentCount * sizeof(float),
                                       indexData.GetRaw(), vertexData);

  PartialBatcherTest::BufferNode node2(SecondBufferIndexPortion * dp::IndexStorage::SizeOfIndex(),
                                       SecondBufferVertexPortion * ComponentCount * sizeof(float),
                                       indexData.GetRaw(FirstBufferIndexPortion),
                                       vertexData + FirstBufferVertexPortion * ComponentCount);

  using IndexVertexCount = std::pair<uint32_t, uint32_t>;
  std::vector<IndexVertexCount> srcData;
  srcData.emplace_back(30, 12);
  srcData.emplace_back(30, 13);
  srcData.emplace_back(18, 30);
  srcData.emplace_back(19, 30);

  for (size_t i = 0; i < srcData.size(); ++i)
  {
    InSequence seq;
    PartialBatcherTest test;
    test.AddBufferNode(node1);
    test.AddBufferNode(node2);
    test.CloseExpection();

    auto renderState = make_unique_dp<TestExtension>();
    auto state = RenderState(0, make_ref(renderState));

    BindingInfo binding(1);
    BindingDecl & decl = binding.GetBindingDecl(0);
    decl.m_attributeName = "position";
    decl.m_componentCount = ComponentCount;
    decl.m_componentType = gl_const::GLFloatType;
    decl.m_offset = 0;
    decl.m_stride = 0;

    AttributeProvider provider(1, VertexCount);
    provider.InitStream(0, binding, make_ref(vertexData));

    VAOAcceptor vaoAcceptor;
    Batcher batcher(srcData[i].first, srcData[i].second);
    batcher.StartSession(std::bind(&VAOAcceptor::FlushFullBucket, &vaoAcceptor, _1, _2));
    batcher.InsertListOfStrip(make_ref(&context), state, make_ref(&provider), 4);
    batcher.EndSession(make_ref(&context));

    for (size_t i = 0; i < vaoAcceptor.m_vao.size(); ++i)
      vaoAcceptor.m_vao[i].reset();
  }
}
