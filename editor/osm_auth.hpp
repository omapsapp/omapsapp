#pragma once

#include "base/exception.hpp"

#include <string>
#include <utility>

namespace osm
{
struct Oauth2Params
{
  std::string m_clientId;
  std::string m_clientSecret;
  std::string m_scope;
  std::string m_redirectUri;
};


/// All methods that interact with the OSM server are blocking and not asynchronous.
class OsmOAuth
{
public:
  /// Do not use enum class here for easier matching with error_code().
  enum HTTP : int
  {
    OK = 200,
    Found = 302,
    BadXML = 400,
    BadAuth = 401,
    Redacted = 403,
    NotFound = 404,
    WrongMethod = 405,
    Conflict = 409,
    Gone = 410,
    // Most often it means bad reference to another object.
    PreconditionFailed = 412,
    URITooLong = 414,
    TooMuchData = 509
  };

  /// A pair of <http error code, response contents>.
  using Response = std::pair<int, std::string>;

  DECLARE_EXCEPTION(OsmOAuthException, RootException);
  DECLARE_EXCEPTION(NetworkError, OsmOAuthException);
  DECLARE_EXCEPTION(UnexpectedRedirect, OsmOAuthException);
  DECLARE_EXCEPTION(UnsupportedApiRequestMethod, OsmOAuthException);
  DECLARE_EXCEPTION(InvalidKeySecret, OsmOAuthException);
  DECLARE_EXCEPTION(FetchSessionIdError, OsmOAuthException);
  DECLARE_EXCEPTION(LogoutUserError, OsmOAuthException);
  DECLARE_EXCEPTION(LoginUserPasswordServerError, OsmOAuthException);
  DECLARE_EXCEPTION(LoginUserPasswordFailed, OsmOAuthException);
  DECLARE_EXCEPTION(LoginSocialServerError, OsmOAuthException);
  DECLARE_EXCEPTION(LoginSocialFailed, OsmOAuthException);
  DECLARE_EXCEPTION(SendAuthRequestError, OsmOAuthException);
  DECLARE_EXCEPTION(FetchRequestTokenServerError, OsmOAuthException);
  DECLARE_EXCEPTION(FinishAuthorizationServerError, OsmOAuthException);
  DECLARE_EXCEPTION(ResetPasswordServerError, OsmOAuthException);

  static bool IsValid(std::string const & ks);

  /// The constructor. Simply stores a lot of strings in fields.
  OsmOAuth(std::string const & oauth2ClientId, std::string const & oauth2Secret, std::string const & oauth2Scope,
           std::string const & oauth2RedirectUri, std::string baseUrl, std::string apiUrl);

  /// Should be used everywhere in production code instead of servers below.
  static OsmOAuth ServerAuth();
  static OsmOAuth ServerAuth(std::string const & oauthToken);

  /// master.apis.dev.openstreetmap.org
  static OsmOAuth DevServerAuth();
  /// api.openstreetmap.org
  static OsmOAuth ProductionServerAuth();

  void SetAuthToken(std::string const & authToken);
  std::string const & GetAuthToken() const;
  bool IsAuthorized() const;

  /// @returns false if login and/or password are invalid.
  bool AuthorizePassword(std::string const & login, std::string const & password);
  /// @returns false if Facebook credentials are invalid.
  bool AuthorizeFacebook(std::string const & facebookToken);
  /// @returns false if Google credentials are invalid.
  bool AuthorizeGoogle(std::string const & googleToken);
  /// @returns false if email has not been registered on a server.
  //bool ResetPassword(std::string const & email) const;

  /// Throws in case of network errors.
  /// @param[method] The API method, must start with a forward slash.
  Response Request(std::string const & method, std::string const & httpMethod = "GET",
                   std::string const & body = "") const;
  /// Tokenless GET request, for convenience.
  /// @param[api] If false, request is made to m_baseUrl.
  Response DirectRequest(std::string const & method, bool api = true) const;

  // Getters
  std::string GetBaseUrl() const { return m_baseUrl; }
  std::string GetClientId() const { return m_oauth2params.m_clientId; }
  std::string GetClientSecret() const { return m_oauth2params.m_clientSecret; }
  std::string GetScope() const { return m_oauth2params.m_scope; }
  std::string GetRedirectUri() const { return m_oauth2params.m_redirectUri; }

  /// @name Methods for WebView-based authentication.
  //@{
  std::string FinishAuthorization(std::string const & oauth2code) const;
  std::string GetRegistrationURL() const { return m_baseUrl + "/user/new"; }
  std::string GetResetPasswordURL() const { return m_baseUrl + "/user/forgot-password"; }
  std::string GetHistoryURL(std::string const & user) const
  {
    return m_baseUrl + "/user/" + user + "/history";
  }
  std::string GetNotesURL(std::string const & user) const
  {
    return m_baseUrl + "/user/" + user + "/notes";
  }
  std::string BuildOAuth2Url() const;
  //@}

private:
  struct SessionID
  {
    std::string m_cookies;
    std::string m_authenticityToken;
  };

  /// OAuth2 parameters (including secret) for application.
  Oauth2Params const m_oauth2params;
  std::string const m_baseUrl;
  std::string const m_apiUrl;
  /// Token to authenticate every OAuth request.
  //std::string const m_oauth2code;
  std::string m_oauth2token;

  SessionID FetchSessionId(std::string const & subUrl = "/login",
                           std::string const & cookies = "") const;
  /// Log a user out.
  void LogoutUser(SessionID const & sid) const;
  /// Signs a user id using login and password.
  /// @returns false if login or password are invalid.
  bool LoginUserPassword(std::string const & login, std::string const & password,
                         SessionID const & sid) const;
  /// Signs a user in using Facebook token.
  /// @returns false if the social token is invalid.
  bool LoginSocial(std::string const & callbackPart, std::string const & socialToken,
                   SessionID const & sid) const;
  /// @returns non-empty string with oauth_verifier value.
  std::string SendAuthRequest(std::string const & requestTokenKey, SessionID const & lastSid) const;
  /// @returns valid key and secret or throws otherwise.
  std::string FetchRequestToken(SessionID const & sid) const;
  std::string FetchAccessToken(SessionID const & sid) const;
};

std::string DebugPrint(OsmOAuth::Response const & code);

}  // namespace osm
