#include "platform/preferred_languages.hpp"
#include "platform/settings.hpp"

#include "coding/string_utf8_multilang.hpp"

#include "base/buffer_vector.hpp"
#include "base/macros.hpp"
#include "base/string_utils.hpp"

#include "std/target_os.hpp"

#include <cstdlib>  // getenv
#include <cstring>  // strlen
#include <string>

#if defined(OMIM_OS_MAC) || defined(OMIM_OS_IPHONE)
  #include <CoreFoundation/CFLocale.h>
  #include <CoreFoundation/CFString.h>
#elif defined(OMIM_OS_WINDOWS)
  #include "std/windows.hpp"
  // for XP it's not defined
  #define MUI_LANGUAGE_NAME 0x8
#elif defined(OMIM_OS_LINUX)
  #include <cstdlib>
#elif defined(OMIM_OS_ANDROID)
  /// Body for this function is inside android/app/src/main/cpp sources
  std::string GetAndroidSystemLanguage();
#else
  #error "Define language preferences for your platform"
#endif

#ifdef OMIM_OS_WINDOWS
struct MSLocale
{
  uint16_t m_code;
  char const * m_name;
};
/// Used on Windows XP which lacks LCIDToLocaleName function
/// Taken from MSDN: http://msdn.microsoft.com/en-us/library/cc233968(v=PROT.10).aspx
static const MSLocale gLocales[] = {{0x1,"ar"},{0x2,"bg"},{0x3,"ca"},{0x4,"zh-Hans"},{0x5,"cs"},{0x6,"da"},{0x7,"de"},{0x8,"el"},{0x9,"en"},{0xa,"es"},{0xb,"fi"},{0xc,"fr"},{0xd,"he"},{0xe,"hu"},{0xf,"is"},{0x10,"it"},{0x11,"ja"},{0x12,"ko"},{0x13,"nl"},{0x14,"no"},{0x15,"pl"},{0x16,"pt"},{0x17,"rm"},{0x18,"ro"},{0x19,"ru"},{0x1a,"hr"},{0x1b,"sk"},{0x1c,"sq"},{0x1d,"sv"},{0x1e,"th"},{0x1f,"tr"},{0x20,"ur"},{0x21,"id"},{0x22,"uk"},{0x23,"be"},{0x24,"sl"},{0x25,"et"},{0x26,"lv"},{0x27,"lt"},{0x28,"tg"},{0x29,"fa"},{0x2a,"vi"},{0x2b,"hy"},{0x2c,"az"},{0x2d,"eu"},{0x2e,"hsb"},{0x2f,"mk"},{0x32,"tn"},{0x34,"xh"},{0x35,"zu"},{0x36,"af"},{0x37,"ka"},{0x38,"fo"},{0x39,"hi"},{0x3a,"mt"},{0x3b,"se"},{0x3c,"ga"},{0x3e,"ms"},{0x3f,"kk"},{0x40,"ky"},{0x41,"sw"},{0x42,"tk"},{0x43,"uz"},{0x44,"tt"},{0x45,"bn"},{0x46,"pa"},{0x47,"gu"},{0x48,"or"},{0x49,"ta"},{0x4a,"te"},{0x4b,"kn"},{0x4c,"ml"},{0x4d,"as"},{0x4e,"mr"},{0x4f,"sa"},{0x50,"mn"},{0x51,"bo"},{0x52,"cy"},{0x53,"km"},{0x54,"lo"},{0x56,"gl"},{0x57,"kok"},{0x5a,"syr"},{0x5b,"si"},{0x5d,"iu"},{0x5e,"am"},{0x5f,"tzm"},{0x61,"ne"},{0x62,"fy"},{0x63,"ps"},{0x64,"fil"},{0x65,"dv"},{0x68,"ha"},{0x6a,"yo"},{0x6b,"quz"},{0x6c,"nso"},{0x6d,"ba"},{0x6e,"lb"},{0x6f,"kl"},{0x70,"ig"},{0x78,"ii"},{0x7a,"arn"},{0x7c,"moh"},{0x7e,"br"},{0x80,"ug"},{0x81,"mi"},{0x82,"oc"},{0x83,"co"},{0x84,"gsw"},{0x85,"sah"},{0x86,"qut"},{0x87,"rw"},{0x88,"wo"},{0x8c,"prs"},{0x91,"gd"},{0x401,"ar-SA"},{0x402,"bg-BG"},{0x403,"ca-ES"},{0x404,"zh-TW"},{0x405,"cs-CZ"},{0x406,"da-DK"},{0x407,"de-DE"},{0x408,"el-GR"},{0x409,"en-US"},{0x40a,"es-ES_tradnl"},{0x40b,"fi-FI"},{0x40c,"fr-FR"},{0x40d,"he-IL"},{0x40e,"hu-HU"},{0x40f,"is-IS"},{0x410,"it-IT"},{0x411,"ja-JP"},{0x412,"ko-KR"},{0x413,"nl-NL"},{0x414,"nb-NO"},{0x415,"pl-PL"},{0x416,"pt-BR"},{0x417,"rm-CH"},{0x418,"ro-RO"},{0x419,"ru-RU"},{0x41a,"hr-HR"},{0x41b,"sk-SK"},{0x41c,"sq-AL"},{0x41d,"sv-SE"},{0x41e,"th-TH"},{0x41f,"tr-TR"},{0x420,"ur-PK"},{0x421,"id-ID"},{0x422,"uk-UA"},{0x423,"be-BY"},{0x424,"sl-SI"},{0x425,"et-EE"},{0x426,"lv-LV"},{0x427,"lt-LT"},{0x428,"tg-Cyrl-TJ"},{0x429,"fa-IR"},{0x42a,"vi-VN"},{0x42b,"hy-AM"},{0x42c,"az-Latn-AZ"},{0x42d,"eu-ES"},{0x42e,"wen-DE"},{0x42f,"mk-MK"},{0x430,"st-ZA"},{0x431,"ts-ZA"},{0x432,"tn-ZA"},{0x433,"ven-ZA"},{0x434,"xh-ZA"},{0x435,"zu-ZA"},{0x436,"af-ZA"},{0x437,"ka-GE"},{0x438,"fo-FO"},{0x439,"hi-IN"},{0x43a,"mt-MT"},{0x43b,"se-NO"},{0x43e,"ms-MY"},{0x43f,"kk-KZ"},{0x440,"ky-KG"},{0x441,"sw-KE"},{0x442,"tk-TM"},{0x443,"uz-Latn-UZ"},{0x444,"tt-RU"},{0x445,"bn-IN"},{0x446,"pa-IN"},{0x447,"gu-IN"},{0x448,"or-IN"},{0x449,"ta-IN"},{0x44a,"te-IN"},{0x44b,"kn-IN"},{0x44c,"ml-IN"},{0x44d,"as-IN"},{0x44e,"mr-IN"},{0x44f,"sa-IN"},{0x450,"mn-MN"},{0x451,"bo-CN"},{0x452,"cy-GB"},{0x453,"km-KH"},{0x454,"lo-LA"},{0x455,"my-MM"},{0x456,"gl-ES"},{0x457,"kok-IN"},{0x458,"mni"},{0x459,"sd-IN"},{0x45a,"syr-SY"},{0x45b,"si-LK"},{0x45c,"chr-US"},{0x45d,"iu-Cans-CA"},{0x45e,"am-ET"},{0x45f,"tmz"},{0x461,"ne-NP"},{0x462,"fy-NL"},{0x463,"ps-AF"},{0x464,"fil-PH"},{0x465,"dv-MV"},{0x466,"bin-NG"},{0x467,"fuv-NG"},{0x468,"ha-Latn-NG"},{0x469,"ibb-NG"},{0x46a,"yo-NG"},{0x46b,"quz-BO"},{0x46c,"nso-ZA"},{0x46d,"ba-RU"},{0x46e,"lb-LU"},{0x46f,"kl-GL"},{0x470,"ig-NG"},{0x471,"kr-NG"},{0x472,"gaz-ET"},{0x473,"ti-ER"},{0x474,"gn-PY"},{0x475,"haw-US"},{0x477,"so-SO"},{0x478,"ii-CN"},{0x479,"pap-AN"},{0x47a,"arn-CL"},{0x47c,"moh-CA"},{0x47e,"br-FR"},{0x480,"ug-CN"},{0x481,"mi-NZ"},{0x482,"oc-FR"},{0x483,"co-FR"},{0x484,"gsw-FR"},{0x485,"sah-RU"},{0x486,"qut-GT"},{0x487,"rw-RW"},{0x488,"wo-SN"},{0x48c,"prs-AF"},{0x48d,"plt-MG"},{0x491,"gd-GB"},{0x801,"ar-IQ"},{0x804,"zh-CN"},{0x807,"de-CH"},{0x809,"en-GB"},{0x80a,"es-MX"},{0x80c,"fr-BE"},{0x810,"it-CH"},{0x813,"nl-BE"},{0x814,"nn-NO"},{0x816,"pt-PT"},{0x818,"ro-MO"},{0x819,"ru-MO"},{0x81a,"sr-Latn-CS"},{0x81d,"sv-FI"},{0x820,"ur-IN"},{0x82c,"az-Cyrl-AZ"},{0x82e,"dsb-DE"},{0x83b,"se-SE"},{0x83c,"ga-IE"},{0x83e,"ms-BN"},{0x843,"uz-Cyrl-UZ"},{0x845,"bn-BD"},{0x846,"pa-PK"},{0x850,"mn-Mong-CN"},{0x851,"bo-BT"},{0x859,"sd-PK"},{0x85d,"iu-Latn-CA"},{0x85f,"tzm-Latn-DZ"},{0x861,"ne-IN"},{0x86b,"quz-EC"},{0x873,"ti-ET"},{0xc01,"ar-EG"},{0xc04,"zh-HK"},{0xc07,"de-AT"},{0xc09,"en-AU"},{0xc0a,"es-ES"},{0xc0c,"fr-CA"},{0xc1a,"sr-Cyrl-CS"},{0xc3b,"se-FI"},{0xc5f,"tmz-MA"},{0xc6b,"quz-PE"},{0x1001,"ar-LY"},{0x1004,"zh-SG"},{0x1007,"de-LU"},{0x1009,"en-CA"},{0x100a,"es-GT"},{0x100c,"fr-CH"},{0x101a,"hr-BA"},{0x103b,"smj-NO"},{0x1401,"ar-DZ"},{0x1404,"zh-MO"},{0x1407,"de-LI"},{0x1409,"en-NZ"},{0x140a,"es-CR"},{0x140c,"fr-LU"},{0x141a,"bs-Latn-BA"},{0x143b,"smj-SE"},{0x1801,"ar-MA"},{0x1809,"en-IE"},{0x180a,"es-PA"},{0x180c,"fr-MC"},{0x181a,"sr-Latn-BA"},{0x183b,"sma-NO"},{0x1c01,"ar-TN"},{0x1c09,"en-ZA"},{0x1c0a,"es-DO"},{0x1c0c,"fr-WestIndies"},{0x1c1a,"sr-Cyrl-BA"},{0x1c3b,"sma-SE"},{0x2001,"ar-OM"},{0x2009,"en-JM"},{0x200a,"es-VE"},{0x200c,"fr-RE"},{0x201a,"bs-Cyrl-BA"},{0x203b,"sms-FI"},{0x2401,"ar-YE"},{0x2409,"en-CB"},{0x240a,"es-CO"},{0x240c,"fr-CG"},{0x241a,"sr-Latn-RS"},{0x243b,"smn-FI"},{0x2801,"ar-SY"},{0x2809,"en-BZ"},{0x280a,"es-PE"},{0x280c,"fr-SN"},{0x281a,"sr-Cyrl-RS"},{0x2c01,"ar-JO"},{0x2c09,"en-TT"},{0x2c0a,"es-AR"},{0x2c0c,"fr-CM"},{0x2c1a,"sr-Latn-ME"},{0x3001,"ar-LB"},{0x3009,"en-ZW"},{0x300a,"es-EC"},{0x300c,"fr-CI"},{0x301a,"sr-Cyrl-ME"},{0x3401,"ar-KW"},{0x3409,"en-PH"},{0x340a,"es-CL"},{0x340c,"fr-ML"},{0x3801,"ar-AE"},{0x3809,"en-ID"},{0x380a,"es-UY"},{0x380c,"fr-MA"},{0x3c01,"ar-BH"},{0x3c09,"en-HK"},{0x3c0a,"es-PY"},{0x3c0c,"fr-HT"},{0x4001,"ar-QA"},{0x4009,"en-IN"},{0x400a,"es-BO"},{0x4409,"en-MY"},{0x440a,"es-SV"},{0x4809,"en-SG"},{0x480a,"es-HN"},{0x4c0a,"es-NI"},{0x500a,"es-PR"},{0x540a,"es-US"},{0x641a,"bs-Cyrl"},{0x681a,"bs-Latn"},{0x6c1a,"sr-Cyrl"},{0x701a,"sr-Latn"},{0x703b,"smn"},{0x742c,"az-Cyrl"},{0x743b,"sms"},{0x7804,"zh"},{0x7814,"nn"},{0x781a,"bs"},{0x782c,"az-Latn"},{0x783b,"sma"},{0x7843,"uz-Cyrl"},{0x7850,"mn-Cyrl"},{0x785d,"iu-Cans"},{0x7c04,"zh-Hant"},{0x7c14,"nb"},{0x7c1a,"sr"},{0x7c28,"tg-Cyrl"},{0x7c2e,"dsb"},{0x7c3b,"smj"},{0x7c43,"uz-Latn"},{0x7c50,"mn-Mong"},{0x7c5d,"iu-Latn"},{0x7c5f,"tzm-Latn"},{0x7c68,"ha-Latn"},};
#endif

namespace languages
{
struct SystemLanguages
{
  buffer_vector<std::string, 4> m_langs;

  SystemLanguages()
  {
    /// @DebugNote
    // Hardcode draw text language.
    //m_langs.push_back("hi");
    //return;

#if defined(OMIM_OS_MAC) || defined(OMIM_OS_IPHONE) || defined(OMIM_OS_LINUX)
    // check environment variables
    char const * p = std::getenv("LANGUAGE");
    if (p && strlen(p))  // LANGUAGE can contain several values divided by ':'
      strings::Tokenize(p, ":", [this](std::string_view s) { m_langs.push_back(std::string(s)); });
    else if ((p = getenv("LC_ALL")))
      m_langs.push_back(p);
    else if ((p = getenv("LC_MESSAGES")))
      m_langs.push_back(p);
    else if ((p = getenv("LANG")))
      m_langs.push_back(p);

  #if defined(OMIM_OS_MAC) || defined(OMIM_OS_IPHONE)
    else
    {
      // Mac and iOS implementation
      CFArrayRef langs = CFLocaleCopyPreferredLanguages();
      char buf[30];
      for (CFIndex i = 0; i < CFArrayGetCount(langs); ++i)
      {
        CFStringRef strRef = (CFStringRef)CFArrayGetValueAtIndex(langs, i);
        CFStringGetCString(strRef, buf, 30, kCFStringEncodingUTF8);
        m_langs.push_back(buf);
      }
      CFRelease(langs);
    }
  #endif

#elif defined(OMIM_OS_WINDOWS)
    // if we're on Vista or above, take list of preferred languages
    typedef BOOL (WINAPI *PGETUSERPREFERREDUILANGUAGES)(DWORD, PULONG, PWCHAR, PULONG);
    PGETUSERPREFERREDUILANGUAGES p =
        reinterpret_cast<PGETUSERPREFERREDUILANGUAGES>(
            GetProcAddress(GetModuleHandleA("Kernel32.dll"), "GetUserPreferredUILanguages"));
    if (p)
    {
      // Vista or above, get buffer size first
      ULONG numLangs;
      WCHAR * buf = NULL;
      ULONG bufSize = 0;
      CHECK_EQUAL(TRUE, p(MUI_LANGUAGE_NAME, &numLangs, buf, &bufSize), ());
      CHECK_GREATER(bufSize, 0U, ("GetUserPreferredUILanguages failed"));
      buf = new WCHAR[++bufSize];
      p(MUI_LANGUAGE_NAME, &numLangs, buf, &bufSize);
      size_t len;
      WCHAR * pCurr = buf;
      while ((len = wcslen(pCurr)))
      {
        char * utf8Buf = new char[len*2];
        CHECK_NOT_EQUAL(WideCharToMultiByte(CP_UTF8, 0, pCurr, -1, utf8Buf, len*2, NULL, NULL), 0, ());
        m_langs.push_back(utf8Buf);
        delete[] utf8Buf;
        pCurr += len + 1;
      }
      delete[] buf;
    }

    if (m_langs.empty())
    {
      // used mostly on WinXP
      LANGID langId = GetUserDefaultLangID();
      for (size_t i = 0; i < ARRAY_SIZE(gLocales); ++i)
        if (gLocales[i].m_code == langId)
        {
          m_langs.push_back(gLocales[i].m_name);
          break;
        }
    }

#elif defined(OMIM_OS_ANDROID)
    m_langs.push_back(GetAndroidSystemLanguage());
#else
    #error "Define language preferences for your platform"
#endif
  }
};

buffer_vector<std::string, 4> const & GetSystemPreferred()
{
  static SystemLanguages const langs;
  return langs.m_langs;
}

std::string GetPreferred()
{
  // generate output string
  std::string result;
  for (auto const & e : GetSystemPreferred())
  {
    result.append(e);
    result.push_back('|');
  }

  if (result.empty())
    result = "default";
  else
    result.pop_back();
  return result;
}

std::string GetCurrentOrig()
{
  auto const & arr = GetSystemPreferred();
  if (arr.empty())
    return "en";
  else
    return arr[0];
}

std::string Normalize(std::string_view lang)
{
  return std::string{lang.substr(0, lang.find_first_of("-_ "))};
}

std::string GetCurrentNorm()
{
  return Normalize(GetCurrentOrig());
}

std::string GetCurrentMapLanguage()
{
  std::string languageCode;
  if (!settings::Get(settings::kMapLanguageCode, languageCode) || languageCode.empty())
  {
    for (auto const & systemLanguage : GetSystemPreferred())
    {
      auto normalizedLang = Normalize(systemLanguage);
      if (StringUtf8Multilang::GetLangIndex(normalizedLang) != StringUtf8Multilang::kUnsupportedLanguageCode)
        return normalizedLang;
    }
    return std::string(StringUtf8Multilang::GetLangByCode(StringUtf8Multilang::kDefaultCode));
  }
  return languageCode;
}

std::string GetTwine(std::string const & lang)
{
  // Special cases for different Chinese variations.
  if (lang.find("zh") == 0)
  {
    std::string lower = lang;
    strings::AsciiToLower(lower);

    // Traditional Chinese.
    for (char const * s : {"hant", "tw", "hk", "mo"})
    {
      if (lower.find(s) != std::string::npos)
        return "zh-Hant";
    }

    // Simplified Chinese by default for all other cases.
    return "zh-Hans";
  }
  // Use short (2 or 3 chars) versions for all other languages.
  return Normalize(lang);
}

std::string GetCurrentTwine()
{
  return GetTwine(GetCurrentOrig());
}

std::string GetCurrentMapTwine()
{
  return GetTwine(GetCurrentMapLanguage());
}

}  // namespace languages
