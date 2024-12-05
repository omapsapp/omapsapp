#include "testing/testing.hpp"

#include "base/dfa_helpers.hpp"
#include "base/levenshtein_dfa.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace levenshtein_dfa_test
{
using namespace std;
using namespace strings;

enum class Status
{
  Accepts,
  Rejects,
  Intermediate
};

struct Result
{
  Result() = default;
  Result(Status status, size_t errorsMade = 0, size_t prefixErrorsMade = 0)
    : m_status(status), m_errorsMade(errorsMade), m_prefixErrorsMade(prefixErrorsMade)
  {
  }

  bool operator==(Result const & rhs) const
  {
    return m_status == rhs.m_status &&
           (m_errorsMade == rhs.m_errorsMade || m_status == Status::Rejects) &&
           (m_prefixErrorsMade == rhs.m_prefixErrorsMade || m_status == Status::Rejects);
  }

  Status m_status = Status::Accepts;
  size_t m_errorsMade = 0;
  size_t m_prefixErrorsMade = 0;
};

string DebugPrint(Status status)
{
  switch (status)
  {
  case Status::Accepts: return "Accepts";
  case Status::Rejects: return "Rejects";
  case Status::Intermediate: return "Intermediate";
  }
  UNREACHABLE();
}

string DebugPrint(Result const & result)
{
  ostringstream os;
  os << "Result [ ";
  os << "status: " << DebugPrint(result.m_status) << ", ";
  os << "errorsMade: " << result.m_errorsMade << ", ";
  os << "prefixErrorsMade: " << result.m_prefixErrorsMade << " ]";
  return os.str();
}

Result GetResult(LevenshteinDFA const & dfa, std::string const & s)
{
  auto it = dfa.Begin();
  DFAMove(it, s);
  if (it.Accepts())
    return Result(Status::Accepts, it.ErrorsMade(), it.PrefixErrorsMade());
  if (it.Rejects())
    return Result(Status::Rejects, it.ErrorsMade(), it.PrefixErrorsMade());
  return Result(Status::Intermediate, it.ErrorsMade(), it.PrefixErrorsMade());
}

bool Accepts(LevenshteinDFA const & dfa, std::string const & s)
{
  return GetResult(dfa, s).m_status == Status::Accepts;
}

bool Rejects(LevenshteinDFA const & dfa, std::string const & s)
{
  return GetResult(dfa, s).m_status == Status::Rejects;
}

bool Intermediate(LevenshteinDFA const & dfa, std::string const & s)
{
  return GetResult(dfa, s).m_status == Status::Intermediate;
}

UNIT_TEST(LevenshteinDFA_Smoke)
{
  {
    LevenshteinDFA dfa("", 0 /* maxErrors */);

    auto it = dfa.Begin();
    TEST(it.Accepts(), ());
    TEST(!it.Rejects(), ());

    it.Move('a');
    TEST(!it.Accepts(), ());
    TEST(it.Rejects(), ());

    it.Move('b');
    TEST(!it.Accepts(), ());
    TEST(it.Rejects(), ());
  }

  {
    LevenshteinDFA dfa("abc", 1 /* maxErrors */);

    TEST(Accepts(dfa, "ab"), ());
    TEST(Accepts(dfa, "abd"), ());
    TEST(Accepts(dfa, "abcd"), ());
    TEST(Accepts(dfa, "bbc"), ());

    TEST(Rejects(dfa, "cba"), ());
    TEST(Rejects(dfa, "abcde"), ());

    TEST(Accepts(dfa, "ac"), ());
    TEST(Accepts(dfa, "acb"), ());
    TEST(Accepts(dfa, "acbc"), ());
    TEST(Rejects(dfa, "acbd"), ());

    TEST(Intermediate(dfa, "a"), ());
  }

  {
    LevenshteinDFA dfa("ленинградский", 2 /* maxErrors */);

    TEST(Accepts(dfa, "ленинградский"), ());
    TEST(Accepts(dfa, "ленингадский"), ());
    TEST(Accepts(dfa, "ленигнрадский"), ());
    TEST(Rejects(dfa, "ленинский"), ());
  }

  {
    LevenshteinDFA dfa("atm", 1 /* maxErrors */);
    TEST(Rejects(dfa, "san"), ());
  }
}

UNIT_TEST(LevenshteinDFA_Prefix)
{
  {
    LevenshteinDFA dfa("москва", 1 /* prefixSize */, 1 /* maxErrors */);
    TEST(Accepts(dfa, "москва"), ());
    TEST(Accepts(dfa, "масква"), ());
    TEST(Accepts(dfa, "моска"), ());
    TEST(Rejects(dfa, "иосква"), ());
  }
  {
    LevenshteinDFA dfa("москва", 0 /* prefixSize */, 1 /* maxErrors */);
    TEST(Accepts(dfa, "москва"), ());
    TEST(Accepts(dfa, "иосква"), ());
    TEST(Accepts(dfa, "моксва"), ());
  }
}

UNIT_TEST(LevenshteinDFA_ErrorsMade)
{
  {
    LevenshteinDFA dfa("москва", 1 /* prefixSize */, 2 /* maxErrors */);

    TEST_EQUAL(GetResult(dfa, "москва"),
               Result(Status::Accepts, 0 /* errorsMade */, 0 /* prefixErrorsMade */), ());
    TEST_EQUAL(GetResult(dfa, "москв"),
               Result(Status::Accepts, 1 /* errorsMade */, 0 /* prefixErrorsMade */), ());
    TEST_EQUAL(GetResult(dfa, "моск"),
               Result(Status::Accepts, 2 /* errorsMade */, 0 /* prefixErrorsMade */), ());
    TEST_EQUAL(GetResult(dfa, "мос").m_status, Status::Intermediate, ());
    TEST_EQUAL(GetResult(dfa, "мос").m_prefixErrorsMade, 0, ());

    TEST_EQUAL(GetResult(dfa, "моксав"),
               Result(Status::Accepts, 2 /* errorsMade */, 2 /* prefixErrorsMade */), ());
    TEST_EQUAL(GetResult(dfa, "максав").m_status, Status::Rejects, ());

    TEST_EQUAL(GetResult(dfa, "мсовк").m_status, Status::Intermediate, ());
    TEST_EQUAL(GetResult(dfa, "мсовк").m_prefixErrorsMade, 2, ());
    TEST_EQUAL(GetResult(dfa, "мсовка"),
               Result(Status::Accepts, 2 /* errorsMade */, 2 /* prefixErrorsMade */), ());
    TEST_EQUAL(GetResult(dfa, "мсовкб").m_status, Status::Rejects, ());
  }

  {
    LevenshteinDFA dfa("aa", 0 /* prefixSize */, 2 /* maxErrors */);
    TEST_EQUAL(GetResult(dfa, "abab"),
               Result(Status::Accepts, 2 /* errorsMade */, 2 /* prefixErrorsMade */), ());
  }

  {
    LevenshteinDFA dfa("mississippi", 0 /* prefixSize */, 0 /* maxErrors */);
    TEST_EQUAL(GetResult(dfa, "misisipi").m_status, Status::Rejects, ());
    TEST_EQUAL(GetResult(dfa, "mississipp").m_status, Status::Intermediate, ());
    TEST_EQUAL(GetResult(dfa, "mississipp").m_prefixErrorsMade, 0, ());
    TEST_EQUAL(GetResult(dfa, "mississippi"),
               Result(Status::Accepts, 0 /* errorsMade */, 0 /* prefixErrorsMade */), ());
  }

  {
    vector<UniString> const allowedMisprints = {MakeUniString("yj")};
    size_t const prefixSize = 1;
    size_t const maxErrors = 1;
    string const str = "yekaterinburg";
    vector<pair<string, Result>> const queries = {
        {"yekaterinburg", Result(Status::Accepts, 0 /* errorsMade */, 0 /* prefixErrorsMade */)},
        {"ekaterinburg", Result(Status::Accepts, 1 /* errorsMade */, 1 /* prefixErrorsMade */)},
        {"jekaterinburg", Result(Status::Accepts, 1 /* errorsMade */, 1 /* prefixErrorsMade */)},
        {"iekaterinburg", Result(Status::Rejects)}};

    for (auto const & q : queries)
    {
      LevenshteinDFA dfa(MakeUniString(q.first), prefixSize, allowedMisprints, maxErrors);
      TEST_EQUAL(GetResult(dfa, str), q.second, ("Query:", q.first, "string:", str));
    }
  }

  {
    LevenshteinDFA dfa("кафе", 1 /* prefixSize */, 1 /* maxErrors */);
    TEST_EQUAL(GetResult(dfa, "кафе"),
               Result(Status::Accepts, 0 /* errorsMade */, 0 /* prefixErrorsMade */), ());
    TEST_EQUAL(GetResult(dfa, "кафер"),
               Result(Status::Accepts, 1 /* errorsMade */, 1 /* prefixErrorsMade */), ());
  }
}

UNIT_TEST(LevenshteinDFA_PrefixDFAModifier)
{
  {
    PrefixDFAModifier<LevenshteinDFA> dfa(LevenshteinDFA("abcde", 2 /* maxErrors */));

    auto it = dfa.Begin();
    DFAMove(it, "ab");

    TEST(!it.Accepts(), ());
    TEST(!it.Rejects(), ());
    TEST_EQUAL(it.PrefixErrorsMade(), 0, ());
    // |maxErrors| for all non-accepting states.
    TEST_EQUAL(it.ErrorsMade(), 2, ());

    DFAMove(it, "c");
    TEST(it.Accepts(), ());
    TEST(!it.Rejects(), ());
    TEST_EQUAL(it.ErrorsMade(), 2, ());
    TEST_EQUAL(it.PrefixErrorsMade(), 0, ());

    DFAMove(it, "d");
    TEST(it.Accepts(), ());
    TEST(!it.Rejects(), ());
    TEST_EQUAL(it.ErrorsMade(), 1, ());
    TEST_EQUAL(it.PrefixErrorsMade(), 0, ());

    DFAMove(it, "e");
    TEST(it.Accepts(), ());
    TEST(!it.Rejects(), ());
    TEST_EQUAL(it.ErrorsMade(), 0, ());
    TEST_EQUAL(it.PrefixErrorsMade(), 0, ());

    DFAMove(it, "fghijklmn");
    TEST(it.Accepts(), ());
    TEST(!it.Rejects(), ());
    TEST_EQUAL(it.ErrorsMade(), 0, ());
    TEST_EQUAL(it.PrefixErrorsMade(), 0, ());
  }
}

UNIT_TEST(LevenshteinDFA_PrefixDFASmoke)
{
  vector<char> const kAlphabet = {'a', 'b', 'c'};
  vector<string> sources;
  vector<string> queries;
  auto generate = [](vector<char> const & alphabet, size_t size, vector<string> & result)
  {
    result.clear();
    result.resize(math::PowUint(alphabet.size(), size));
    for (size_t letterNumber = 0;  letterNumber < size; ++letterNumber)
    {
      for (size_t i = 0; i < result.size(); ++i)
      {
        auto const letterIndex = (i / math::PowUint(alphabet.size(), size - letterNumber - 1)) % alphabet.size();
        result[i].push_back(alphabet[letterIndex]);
      }
    }
  };

  /// @todo What do we check here?
  {
    generate(kAlphabet, 4, sources);
    generate(kAlphabet, 2, queries);

    for (auto const & source : sources)
    {
      for (auto const & query : queries)
      {
        PrefixDFAModifier<LevenshteinDFA> dfa(LevenshteinDFA(source, 2 /* maxErrors */));
        auto it = dfa.Begin();
        for (auto const c : query)
          DFAMove(it, std::string({c}));
      }
    }
  }
}
}  // namespace levenshtein_dfa_test
