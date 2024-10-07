#include "qt/build_style/build_skins.h"

#include "qt/build_style/build_common.h"

#include "platform/platform.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <fstream>
#include <functional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

#include <QtCore/QDir>

namespace
{
enum SkinType
{
  SkinMDPI,
  SkinHDPI,
  SkinXHDPI,
  Skin6Plus,
  SkinXXHDPI,
  SkinXXXHDPI,

  // SkinCount MUST BE last
  SkinCount
};

using SkinInfo = std::tuple<const char*, int, bool>;
SkinInfo const g_skinInfo[SkinCount] =
{
  std::make_tuple("mdpi", 18, false),
  std::make_tuple("hdpi", 27, false),
  std::make_tuple("xhdpi", 36, false),
  std::make_tuple("6plus", 43, false),
  std::make_tuple("xxhdpi", 54, false),
  std::make_tuple("xxxhdpi", 64, false),
};

std::array<SkinType, SkinCount> const g_skinTypes =
{{
  SkinMDPI,
  SkinHDPI,
  SkinXHDPI,
  Skin6Plus,
  SkinXXHDPI,
  SkinXXXHDPI,
}};

inline const char * SkinSuffix(SkinType s) { return std::get<0>(g_skinInfo[s]); }
inline int SkinSize(SkinType s) { return std::get<1>(g_skinInfo[s]); }
inline bool SkinCoorrectColor(SkinType s) { return std::get<2>(g_skinInfo[s]); }

QString GetSkinGeneratorPath()
{
  QString const path = GetExternalPath("skin_generator_tool", "skin_generator_tool.app/Contents/MacOS", "");
  if (path.isEmpty())
    throw std::runtime_error("Can't find skin_generator_tool");
  ASSERT(QFileInfo::exists(path), (path.toStdString()));
  return path;
}

class RAII
{
public:
  RAII(std::function<void()> && f) : m_f(std::move(f)) {}
  ~RAII() { m_f(); }

private:
  std::function<void()> const m_f;
};

std::string trim(std::string && s)
{
  s.erase(std::remove_if(s.begin(), s.end(), &isspace), s.end());
  return std::move(s);
}
}  // namespace

namespace build_style
{
std::unordered_map<std::string, int> GetSkinSizes(QString const & file)
{
  std::unordered_map<std::string, int> skinSizes;

  for (SkinType s : g_skinTypes)
    skinSizes.insert(std::make_pair(SkinSuffix(s), SkinSize(s)));

  try
  {
    std::ifstream ifs(file.toStdString());

    std::string line;
    while (std::getline(ifs, line))
    {
      size_t const pos = line.find('=');
      if (pos == std::string::npos)
        continue;

      std::string name(line.begin(), line.begin() + pos);
      std::string valueTxt(line.begin() + pos + 1, line.end());

      name = trim(std::move(name));
      int value = std::stoi(trim(std::move(valueTxt)));

      if (value <= 0)
        continue;

      skinSizes[name] = value;
    }
  }
  catch (std::exception const & e)
  {
    // reset
    for (SkinType s : g_skinTypes)
      skinSizes[SkinSuffix(s)] = SkinSize(s);
  }

  return skinSizes;
}

void BuildSkinImpl(QString const & styleDir, QString const & suffix,
                   int size, bool colorCorrection, QString const & outputDir)
{
  QString const symbolsDir = JoinPathQt({styleDir, "symbols"});

  // Check symbols directory exists
  if (!QDir(symbolsDir).exists())
    throw std::runtime_error("Symbols directory does not exist");

  // Caller ensures that output directory is clear
  if (QDir(outputDir).exists())
    throw std::runtime_error("Output directory is not clear");

  // Create output skin directory
  if (!QDir().mkdir(outputDir))
    throw std::runtime_error("Cannot create output skin directory");

  // Create symbolic link for symbols/png
  QString const pngOriginDir = styleDir + suffix;
  QString const pngDir = JoinPathQt({styleDir, "symbols", "png"});
  QFile::remove(pngDir);
  if (!QFile::link(pngOriginDir, pngDir))
    throw std::runtime_error("Unable to create symbols/png link");
  RAII const cleaner([=]() { QFile::remove(pngDir); });

  QString const strSize = QString::number(size);
  // Run the script.
  (void)ExecProcess(GetSkinGeneratorPath(), {
      "--symbolWidth", strSize,
      "--symbolHeight", strSize,
      "--symbolsDir", symbolsDir,
      "--skinName", JoinPathQt({outputDir, "basic"}),
      "--skinSuffix=",
      "--colorCorrection", (colorCorrection ? "true" : "false"),
  });

  // Check if files were created.
  if (QFile(JoinPathQt({outputDir, "symbols.png"})).size() == 0 ||
      QFile(JoinPathQt({outputDir, "symbols.sdf"})).size() == 0)
  {
    throw std::runtime_error("Skin files have not been created");
  }
}

void BuildSkins(QString const & styleDir, QString const & outputDir)
{
  QString const resolutionFilePath = JoinPathQt({styleDir, "resolutions.txt"});

  auto const resolution2size = GetSkinSizes(resolutionFilePath);

  for (SkinType s : g_skinTypes)
  {
    QString const suffix = SkinSuffix(s);
    QString const outputSkinDir = JoinPathQt({outputDir, "symbols", suffix, "design"});
    int const size = resolution2size.at(suffix.toStdString()); // SkinSize(s);
    bool const colorCorrection = SkinCoorrectColor(s);

    BuildSkinImpl(styleDir, suffix, size, colorCorrection, outputSkinDir);
  }
}

void ApplySkins(QString const & outputDir)
{
  QString const resourceDir = GetPlatform().ResourcesDir().c_str();

  for (SkinType s : g_skinTypes)
  {
    QString const suffix = SkinSuffix(s);
    QString const outputSkinDir = JoinPathQt({outputDir, "symbols", suffix, "design"});
    QString const resourceSkinDir = JoinPathQt({resourceDir, "symbols", suffix, "design"});

    if (!QFileInfo::exists(resourceSkinDir) && !QDir().mkdir(resourceSkinDir))
      throw std::runtime_error("Cannot create resource skin directory: " + resourceSkinDir.toStdString());

    if (!CopyFile(JoinPathQt({outputSkinDir, "symbols.png"}),
                  JoinPathQt({resourceSkinDir, "symbols.png"})) ||
        !CopyFile(JoinPathQt({outputSkinDir, "symbols.sdf"}),
                  JoinPathQt({resourceSkinDir, "symbols.sdf"})))
    {
      throw std::runtime_error("Cannot copy skins files");
    }
  }
}
}  // namespace build_style
