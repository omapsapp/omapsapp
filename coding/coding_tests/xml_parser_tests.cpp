#include "testing/testing.hpp"

#include "coding/parse_xml.hpp"
#include "coding/reader.hpp"

#include <string>
#include <vector>

namespace
{
std::string const smokeXml = R"(
<root>
</root>
)";

std::string const longXml = R"(
<root>
  <ruler>
    <portrait>
      <anchor vertical="bottom" horizontal="left"/>
      <offset x="10"/>
    </portrait>
  </ruler>
  <compass>
    <portrait>
      <anchor vertical="center"/>
      <relative vertical="top"/>
    </portrait>
    <landscape>
      <relative vertical="top"/>
      <offset x="34" y="48"/>
    </landscape>
  </compass>
</root>
)";

class SmokeDispatcher
{
public:
  void CharData(std::string const &) {}
  void AddAttr(char const *, char const *) {}
  bool Push(std::string_view push)
  {
    TEST_EQUAL(push, "root", ());
    return true;
  }
  void Pop(std::string_view pop) { TEST_EQUAL(pop, "root", ()); }
};

class Dispatcher
{
public:
  using PairsOfStrings = std::vector<std::pair<std::string, std::string>>;
  using Strings = std::vector<std::string>;

  void CharData(std::string const & /* ch */) {}

  void AddAttr(std::string key, std::string value)
  {
    m_addAttrs.emplace_back(std::move(key), std::move(value));
  }

  bool Push(std::string push)
  {
    m_pushes.emplace_back(std::move(push));
    return true;
  }

  void Pop(std::string pop)
  {
    m_pops.emplace_back(std::move(pop));
  }

  void TestAddAttrs(PairsOfStrings const & addAttrs)
  {
    TestEquality(m_addAttrs, addAttrs);
  }

  void TestPushes(Strings const & pushes)
  {
    TestEquality(m_pushes, pushes);
  }

  void TestPops(Strings const & pops)
  {
    TestEquality(m_pops, pops);
  }

private:
  template <typename F>
  void TestEquality(F const & f1, F const & f2)
  {
    TEST_EQUAL(f1.size(), f2.size(), ());
    for (size_t i = 0; i < f1.size(); ++i)
      TEST_EQUAL(f1[i], f2[i], (i));
  }

  PairsOfStrings m_addAttrs;
  Strings m_pushes;
  Strings m_pops;
};

template <typename D>
void TestXML(std::string const & xmlStr, D & dispatcher)
{
  std::vector<uint8_t> xml(xmlStr.cbegin(), xmlStr.cend());
  MemReader reader(xml.data(), xml.size());
  ReaderSource<MemReader> source(reader);

  ParseXML(source, dispatcher);
}

UNIT_TEST(XmlParser_SmokeTest)
{
  Dispatcher d;
  TestXML(smokeXml, d);
  d.TestAddAttrs({});
  d.TestPushes({"root"});
  d.TestPops({"root"});
}

UNIT_TEST(XmlParser_LongTest)
{
  Dispatcher d;
  TestXML(longXml, d);
  d.TestAddAttrs({std::make_pair("vertical", "bottom"), std::make_pair("horizontal", "left"),
                  std::make_pair("x", "10"), std::make_pair("vertical", "center"),
                  std::make_pair("vertical", "top"), std::make_pair("vertical", "top"),
                  std::make_pair("x", "34"), std::make_pair("y", "48")});
  d.TestPushes({"root", "ruler", "portrait", "anchor", "offset", "compass", "portrait", "anchor",
                "relative", "landscape", "relative", "offset"});
  d.TestPops({"anchor", "offset", "portrait", "ruler", "anchor", "relative", "portrait", "relative",
              "offset", "landscape", "compass", "root"});
}
}  // namespace
