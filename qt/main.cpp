#include "qt/info_dialog.hpp"
#include "qt/mainwindow.hpp"
#include "qt/screenshoter.hpp"

#include "qt/qt_common/helpers.hpp"

#include "map/framework.hpp"

#include "platform/platform.hpp"
#include "platform/settings.hpp"

#include "coding/reader.hpp"

#include "base/logging.hpp"
#include "base/macros.hpp"

#include "build_style/build_style.h"

#include <QtGlobal>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileDialog>

#include <sstream>

#include <gflags/gflags.h>


DEFINE_string(data_path, "", "Path to data directory.");
DEFINE_string(log_abort_level, base::ToString(base::GetDefaultLogAbortLevel()), "Log messages severity that causes termination.");
DEFINE_string(resources_path, "", "Path to resources directory.");
DEFINE_string(kml_path, "", "Activates screenshot mode. Path to a kml file or a directory with kml files to take screenshots.");
DEFINE_string(points, "", "Activates screenshot mode. Points on the map and zoom level "
              "[1..18] in format \"lat,lon,zoom[;lat,lon,zoom]\" or path to a file with points in "
              "the same format. Each point and zoom define a place on the map to take screenshot.");
DEFINE_string(rects, "", "Activates screenshot mode. Rects on the map in format"
              "\"lat_leftBottom,lon_leftBottom,lat_rightTop,lon_rightTop"
              "[;lat_leftBottom,lon_leftBottom,lat_rightTop,lon_rightTop]\" or path to a file with "
              "rects in the same format. Each rect defines a place on the map to take screenshot.");
DEFINE_string(dst_path, "", "Path to a directory to save screenshots.");
DEFINE_string(lang, "", "Device language.");
DEFINE_int32(width, 0, "Screenshot width.");
DEFINE_int32(height, 0, "Screenshot height.");
DEFINE_double(dpi_scale, 0.0, "Screenshot dpi scale (mdpi = 1.0, hdpi = 1.5, xhdpiScale = 2.0, 6plus = 2.4, xxhdpi = 3.0, xxxhdpi = 3.5).");

namespace
{
bool ValidateLogAbortLevel(char const * flagname, std::string const & value)
{
  if (auto level = base::FromString(value); !level)
  {
    std::cerr << "Invalid value for --" << flagname << ": "<< value << ", must be one of: ";
    auto const & names = base::GetLogLevelNames();
    for (size_t i = 0; i < names.size(); ++i)
    {
      if (i != 0)
        std::cerr << ", ";
      std::cerr << names[i];
    }
    std::cerr << '\n';
    return false;
  }
  return true;
}

bool const g_logAbortLevelDummy = gflags::RegisterFlagValidator(&FLAGS_log_abort_level, &ValidateLogAbortLevel);

class FinalizeBase
{
public:
  ~FinalizeBase()
  {
    // optional - clean allocated data in protobuf library
    // useful when using memory and resource leak utilites
    // google::protobuf::ShutdownProtobufLibrary();
  }
  };

#if defined(OMIM_OS_WINDOWS) //&& defined(PROFILER_COMMON)
  class InitializeFinalize : public FinalizeBase
  {
    FILE * m_errFile;
    base::ScopedLogLevelChanger const m_debugLog;
  public:
    InitializeFinalize() : m_debugLog(LDEBUG)
    {
      // App runs without error console under win32.
      m_errFile = ::freopen(".\\mapsme.log", "w", stderr);

      //_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_DELAY_FREE_MEM_DF);
      //_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    }
    ~InitializeFinalize()
    {
      ::fclose(m_errFile);
    }
  };
#else
  typedef FinalizeBase InitializeFinalize;
#endif
}  // namespace

int main(int argc, char * argv[])
{
  // Our double parsing code (base/string_utils.hpp) needs dots as a floating point delimiters, not commas.
  // TODO: Refactor our doubles parsing code to use locale-independent delimiters.
  // For example, https://github.com/google/double-conversion can be used.
  // See http://dbaron.org/log/20121222-locale for more details.
  (void)::setenv("LC_NUMERIC", "C", 1);

  Platform & platform = GetPlatform();

  LOG(LINFO, ("Organic Maps", platform.Version(), "built with QT:", QT_VERSION_STR, "runtime QT:", qVersion(),
    "detected CPU cores:", platform.CpuCores()));

  gflags::SetUsageMessage("Desktop application.");
  gflags::SetVersionString(platform.Version());
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (!FLAGS_resources_path.empty())
    platform.SetResourceDir(FLAGS_resources_path);
  if (!FLAGS_data_path.empty())
    platform.SetWritableDirForTests(FLAGS_data_path);

  if (auto const logLevel = base::FromString(FLAGS_log_abort_level); logLevel)
    base::g_LogAbortLevel = *logLevel;
  else
    LOG(LCRITICAL, ("Invalid log level:", FLAGS_log_abort_level));

  Q_INIT_RESOURCE(resources_common);

  InitializeFinalize mainGuard;
  UNUSED_VALUE(mainGuard);

  QApplication app(argc, argv);
  app.setDesktopFileName("app.organicmaps.desktop");
  platform.SetupMeasurementSystem();


#ifdef BUILD_DESIGNER
    QApplication::setApplicationName("Organic Maps Designer");
#else
    QApplication::setApplicationName("Organic Maps");
#endif


#ifdef DEBUG
  static bool constexpr developerMode = true;
#else
  static bool constexpr developerMode = false;
#endif
  bool outvalue;
  if (!settings::Get(settings::kDeveloperMode, outvalue))
    settings::Set(settings::kDeveloperMode, developerMode);

  // Display EULA if needed.
  char const * settingsEULA = "EulaAccepted";
  bool eulaAccepted = false;
  if (!settings::Get(settingsEULA, eulaAccepted) || !eulaAccepted)
  {
    std::string buffer;
    {
      ReaderPtr<Reader> reader = platform.GetReader("copyright.html");
      reader.ReadAsString(buffer);
    }
    qt::InfoDialog eulaDialog(QCoreApplication::applicationName(), buffer.c_str(), nullptr, {"Accept", "Decline"});
    eulaAccepted = (eulaDialog.exec() == 1);
    settings::Set(settingsEULA, eulaAccepted);
  }

  int returnCode = -1;
  if (eulaAccepted)   // User has accepted EULA
  {
    std::unique_ptr<qt::ScreenshotParams> screenshotParams;

    if (!FLAGS_lang.empty())
      (void)::setenv("LANGUAGE", FLAGS_lang.c_str(), 1);

    if (!FLAGS_kml_path.empty() || !FLAGS_points.empty() || !FLAGS_rects.empty())
    {
      screenshotParams = std::make_unique<qt::ScreenshotParams>();
      if (!FLAGS_kml_path.empty())
      {
        screenshotParams->m_kmlPath = FLAGS_kml_path;
        screenshotParams->m_mode = qt::ScreenshotParams::Mode::KmlFiles;
      }
      else if (!FLAGS_points.empty())
      {
        screenshotParams->m_points = FLAGS_points;
        screenshotParams->m_mode = qt::ScreenshotParams::Mode::Points;
      }
      else if (!FLAGS_rects.empty())
      {
        screenshotParams->m_rects = FLAGS_rects;
        screenshotParams->m_mode = qt::ScreenshotParams::Mode::Rects;
      }
      if (!FLAGS_dst_path.empty())
        screenshotParams->m_dstPath = FLAGS_dst_path;
      if (FLAGS_width > 0)
        screenshotParams->m_width = FLAGS_width;
      if (FLAGS_height > 0)
        screenshotParams->m_height = FLAGS_height;
      if (FLAGS_dpi_scale >= df::VisualParams::kMdpiScale && FLAGS_dpi_scale <= df::VisualParams::kXxxhdpiScale)
        screenshotParams->m_dpiScale = FLAGS_dpi_scale;
    }

    qt::common::SetDefaultSurfaceFormat(QApplication::platformName());

    FrameworkParams frameworkParams;

#ifdef BUILD_DESIGNER
    QString mapcssFilePath;
    if (argc >= 2 && platform.IsFileExistsByFullPath(argv[1]))
        mapcssFilePath = argv[1];
    if (0 == mapcssFilePath.length())
      mapcssFilePath = QFileDialog::getOpenFileName(nullptr, "Open style.mapcss file", "~/", "MapCSS Files (*.mapcss)");
    if (mapcssFilePath.isEmpty())
      return returnCode;

    try
    {
      build_style::BuildIfNecessaryAndApply(mapcssFilePath);
    }
    catch (std::exception const & e)
    {
      QMessageBox msgBox;
      msgBox.setWindowTitle("Error");
      msgBox.setText(e.what());
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.setDefaultButton(QMessageBox::Ok);
      msgBox.exec();
      return returnCode;
    }

#endif // BUILD_DESIGNER

    Framework framework(frameworkParams);
    qt::MainWindow w(framework, std::move(screenshotParams), QApplication::primaryScreen()->geometry()
#ifdef BUILD_DESIGNER
                     , mapcssFilePath
#endif // BUILD_DESIGNER
                     );
    w.show();
    returnCode = QApplication::exec();
  }

#ifdef BUILD_DESIGNER
  if (build_style::NeedRecalculate && !mapcssFilePath.isEmpty())
  {
    try
    {
      build_style::RunRecalculationGeometryScript(mapcssFilePath);
    }
    catch (std::exception & e)
    {
      QMessageBox msgBox;
      msgBox.setWindowTitle("Error");
      msgBox.setText(e.what());
      msgBox.setStandardButtons(QMessageBox::Ok);
      msgBox.setDefaultButton(QMessageBox::Ok);
      msgBox.exec();
    }
  }
#endif // BUILD_DESIGNER

  LOG_SHORT(LINFO, ("Organic Maps finished with code", returnCode));
  return returnCode;
}
