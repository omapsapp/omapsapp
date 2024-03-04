#include "testing/testing.hpp"

#include "editor/osm_auth.hpp"

#include <string>

using osm::OsmOAuth;

char const * kValidOsmUser = "OrganicMapsTestUser";
char const * kValidOsmPassword = "12345678";
namespace
{
constexpr char const * kInvalidOsmPassword = "123";
constexpr char const * kForgotPasswordEmail = "osmtest1@organicmaps.app";
}  // namespace

#if defined(OMIM_OS_MAC)
  extern void runMainQueue(void func());
#endif

UNIT_TEST(OSM_Auth_InvalidLogin)
{
#if defined(OMIM_OS_MAC)
  runMainQueue([]() {
#endif

  OsmOAuth auth = OsmOAuth::DevServerAuth();
  bool result;
  TEST_NO_THROW(result = auth.AuthorizePassword(kValidOsmUser, kInvalidOsmPassword), ());
  TEST_EQUAL(result, false, ("invalid password"));
  TEST(!auth.IsAuthorized(), ("Should not be authorized."));

#if defined(OMIM_OS_MAC)
  });
#endif
}

UNIT_TEST(OSM_Auth_Login)
{
#if defined(OMIM_OS_MAC)
  runMainQueue([]() {
#endif

  OsmOAuth auth = OsmOAuth::DevServerAuth();
  bool result;
  TEST_NO_THROW(result = auth.AuthorizePassword(kValidOsmUser, kValidOsmPassword), ());
  TEST_EQUAL(result, true, ("login to test server"));
  TEST(auth.IsAuthorized(), ("Should be authorized."));
  OsmOAuth::Response const perm = auth.Request("/permissions");
  TEST_EQUAL(perm.first, OsmOAuth::HTTP::OK, ("permission request ok"));
  TEST_NOT_EQUAL(perm.second.find("write_api"), std::string::npos, ("can write to api"));

#if defined(OMIM_OS_MAC)
  });
#endif
}

/*
UNIT_TEST(OSM_Auth_ForgotPassword)
{
#if defined(OMIM_OS_MAC)
  runMainQueue([]() {
#endif

  OsmOAuth auth = OsmOAuth::DevServerAuth();
  bool result;
  TEST_NO_THROW(result = auth.ResetPassword(kForgotPasswordEmail), ());
  TEST_EQUAL(result, true, ("Correct email"));
  TEST_NO_THROW(result = auth.ResetPassword("not@registered.email"), ());
  TEST_EQUAL(result, true, ("Server responses OK even if email is incorrect"));


#if defined(OMIM_OS_MAC)
  });
#endif
}
*/
