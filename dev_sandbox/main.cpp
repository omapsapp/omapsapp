#include "dev_sandbox/imgui_renderer.hpp"

#include "map/framework.hpp"

#include "platform/platform.hpp"
#include "platform/settings.hpp"

#include "coding/reader.hpp"

#include "base/logging.hpp"
#include "base/macros.hpp"

#include "std/target_os.hpp"

#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gflags/gflags.h>

#if defined(OMIM_OS_WINDOWS)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(OMIM_OS_LINUX)
#define GLFW_EXPOSE_NATIVE_X11
#elif defined(OMIM_OS_MAC)
#define GLFW_EXPOSE_NATIVE_COCOA
#else
#error Unsupported plaform
#endif
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/imgui.h>

DEFINE_string(data_path, "", "Path to data directory.");
DEFINE_string(log_abort_level, base::ToString(base::GetDefaultLogAbortLevel()),
              "Log messages severity that causes termination.");
DEFINE_string(resources_path, "", "Path to resources directory.");
DEFINE_string(lang, "", "Device language.");

#if defined(OMIM_OS_MAC) || defined(OMIM_OS_LINUX)
drape_ptr<dp::GraphicsContextFactory> CreateContextFactory(GLFWwindow * window, dp::ApiVersion api, m2::PointU size);
void PrepareDestroyContextFactory(ref_ptr<dp::GraphicsContextFactory> contextFactory);
void OnCreateDrapeEngine(GLFWwindow * window, dp::ApiVersion api, ref_ptr<dp::GraphicsContextFactory> contextFactory);
void UpdateContentScale(GLFWwindow * window, float scale);
void UpdateSize(ref_ptr<dp::GraphicsContextFactory> contextFactory, int w, int h);
#endif

#if defined(OMIM_OS_LINUX)
// Workaround for storage::Status compilation issue:
// /usr/include/X11/Xlib.h:83:16: note: expanded from macro 'Status'
#undef Status
#endif

namespace
{
bool ValidateLogAbortLevel(char const * flagname, std::string const & value)
{
  if (auto level = base::FromString(value); !level)
  {
    std::cerr << "Invalid value for --" << flagname << ": " << value << ", must be one of: ";
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

void errorCallback(int error, char const * description) { LOG(LERROR, ("GLFW (", error, "):", description)); }

std::function<void(int w, int h)> onResize;
std::function<void(double x, double y, int button, int action, int mods)> onMouseButton;
std::function<void(double x, double y)> onMouseMove;
std::function<void(double x, double y, double xOffset, double yOffset)> onScroll;
std::function<void(int key, int scancode, int action, int mods)> onKeyboardButton;
std::function<void(float xscale, float yscale)> onContentScale;

df::Touch GetTouch(double x, double y)
{
  return df::Touch{.m_location = m2::PointF(static_cast<float>(x), static_cast<float>(y)), .m_id = 0};
}

df::Touch GetSymmetricalTouch(Framework & framework, df::Touch const & touch)
{
  m2::PointD const pixelCenter = framework.GetVisiblePixelCenter();
  m2::PointD const symmetricalLocation = pixelCenter + pixelCenter - m2::PointD(touch.m_location);

  df::Touch result;
  result.m_id = touch.m_id + 1;
  result.m_location = symmetricalLocation;

  return result;
}

df::TouchEvent GetTouchEvent(Framework & framework, double x, double y, int mods, df::TouchEvent::ETouchType type)
{
  df::TouchEvent event;
  event.SetTouchType(type);
  event.SetFirstTouch(GetTouch(x, y));
  if (mods & GLFW_MOD_SUPER)
    event.SetSecondTouch(GetSymmetricalTouch(framework, event.GetFirstTouch()));
  return event;
}

void FormatMapSize(uint64_t sizeInBytes, std::string & units, size_t & sizeToDownload)
{
  int const mbInBytes = 1024 * 1024;
  int const kbInBytes = 1024;
  if (sizeInBytes > mbInBytes)
  {
    sizeToDownload = (sizeInBytes + mbInBytes - 1) / mbInBytes;
    units = "MB";
  }
  else if (sizeInBytes > kbInBytes)
  {
    sizeToDownload = (sizeInBytes + kbInBytes - 1) / kbInBytes;
    units = "KB";
  }
  else
  {
    sizeToDownload = sizeInBytes;
    units = "B";
  }
}

std::string_view GetMyPoisitionText(location::EMyPositionMode mode)
{
  switch(mode)
  {
  case location::EMyPositionMode::PendingPosition: return "Pending";
  case location::EMyPositionMode::NotFollowNoPosition: return "No position";
  case location::EMyPositionMode::NotFollow: return "Not follow";
  case location::EMyPositionMode::Follow: return "Follow";
  case location::EMyPositionMode::FollowAndRotate: return "Follow and Rotate";
  }
  return "";
}

dp::ApiVersion GetApiVersion(char const * apiLabel)
{
  std::string_view v(apiLabel);
  if (v == "Metal") return dp::ApiVersion::Metal;
  if (v == "Vulkan") return dp::ApiVersion::Vulkan;
  if (v == "OpenGL") return  dp::ApiVersion::OpenGLES3;
  return dp::ApiVersion::Invalid;
}

#if defined(OMIM_OS_LINUX)
class LinuxGuiThread : public base::TaskLoop
{
public:
  PushResult Push(Task && task) override
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tasks.emplace_back(std::move(task));
    return {true, base::TaskLoop::kNoId};
  }

  PushResult Push(Task const & task) override
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tasks.emplace_back(task);
    return {true, base::TaskLoop::kNoId};
  }

  void ExecuteTasks() 
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto & task : m_tasks)
      task();
    m_tasks.clear();
  }

private:
  std::vector<Task> m_tasks;
  std::mutex m_mutex;
};
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

  LOG(LINFO, ("Organic Maps: Developer Sandbox", platform.Version(), "detected CPU cores:", platform.CpuCores()));

  gflags::SetUsageMessage("Developer Sandbox.");
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

#if defined(OMIM_OS_LINUX)
  auto guiThread = std::make_unique<LinuxGuiThread>();
  auto guiThreadPtr = guiThread.get();
  platform.SetGuiThread(std::move(guiThread));
#endif

  // Init GLFW.
  glfwSetErrorCallback(errorCallback);
  if (!glfwInit())
  {
    return -1;
  }
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#if defined(OMIM_OS_WINDOWS)
  glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif
  auto monitor = glfwGetPrimaryMonitor();
  auto mode = glfwGetVideoMode(monitor);
  GLFWwindow * window =
      glfwCreateWindow(mode->width, mode->height, "Organic Maps: Developer Sandbox", nullptr, nullptr);
  int fbWidth = 0, fbHeight = 0;
  glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
  float xs = 1.0f, ys = 1.0f;
  glfwGetWindowContentScale(window, &xs, &ys);
  float visualScale = std::max(xs, ys);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::StyleColorsClassic();
  glfwMaximizeWindow(window);

  platform.SetupMeasurementSystem();

  bool outvalue;
  if (!settings::Get(settings::kDeveloperMode, outvalue))
    settings::Set(settings::kDeveloperMode, true);

  if (!FLAGS_lang.empty())
    (void)::setenv("LANGUAGE", FLAGS_lang.c_str(), 1);

  FrameworkParams frameworkParams;
  Framework framework(frameworkParams);

  ImguiRenderer imguiRenderer;
  Framework::DrapeCreationParams drapeParams{
#if defined(OMIM_OS_MAC)
      .m_apiVersion = dp::ApiVersion::Metal,
#else
      .m_apiVersion = dp::ApiVersion::Vulkan,
#endif
      .m_visualScale = visualScale,
      .m_surfaceWidth = fbWidth,
      .m_surfaceHeight = fbHeight,
      .m_renderInjectionHandler = [&](ref_ptr<dp::GraphicsContext> context, 
                                      ref_ptr<dp::TextureManager> textureManager,
                                      ref_ptr<gpu::ProgramManager> programManager,
                                      bool shutdown)
      {
        if (shutdown)
          imguiRenderer.Reset();
        else
          imguiRenderer.Render(context, textureManager, programManager);
      }};
  gui::Skin guiSkin(gui::ResolveGuiSkinFile("default"), visualScale);
  guiSkin.Resize(fbWidth, fbHeight);
  guiSkin.ForEach([&](gui::EWidget widget, gui::Position const & pos) { drapeParams.m_widgetsInitInfo[widget] = pos; });
  drapeParams.m_widgetsInitInfo[gui::WIDGET_SCALE_FPS_LABEL] = gui::Position(dp::LeftTop);

  drape_ptr<dp::GraphicsContextFactory> contextFactory;
  auto CreateDrapeEngine = [&](dp::ApiVersion version)
  {
    drapeParams.m_apiVersion = version;
    drapeParams.m_visualScale = visualScale;
    drapeParams.m_surfaceWidth = fbWidth;
    drapeParams.m_surfaceHeight = fbHeight;
    contextFactory = CreateContextFactory(window, drapeParams.m_apiVersion,
                                          m2::PointU(static_cast<uint32_t>(drapeParams.m_surfaceWidth),
                                          static_cast<uint32_t>(drapeParams.m_surfaceHeight)));
    auto params = drapeParams;
    framework.CreateDrapeEngine(make_ref(contextFactory), std::move(params));
    OnCreateDrapeEngine(window, version, make_ref(contextFactory));
    framework.SetRenderingEnabled(nullptr);
  };
  CreateDrapeEngine(drapeParams.m_apiVersion);

  auto DestroyDrapeEngine = [&]()
  {
    framework.SetRenderingDisabled(true);
    framework.DestroyDrapeEngine();
    PrepareDestroyContextFactory(make_ref(contextFactory));
    contextFactory.reset();
  };

  // Process resizing.
  onResize = [&](int w, int h)
  {
    fbWidth = w;
    fbHeight = h;
    if (fbWidth > 0 && fbHeight > 0)
    {
      UpdateSize(make_ref(contextFactory), fbWidth, fbHeight);
      framework.OnSize(fbWidth, fbHeight);

      guiSkin.Resize(w, h);
      gui::TWidgetsLayoutInfo layout;
      guiSkin.ForEach([&layout](gui::EWidget w, gui::Position const & pos) { layout[w] = pos.m_pixelPivot; });
      framework.SetWidgetLayout(std::move(layout));
      framework.MakeFrameActive();
    }
  };
  glfwSetFramebufferSizeCallback(window, [](GLFWwindow * wnd, int w, int h) { onResize(w, h); });

  // Process change content scale.
  onContentScale = [&](float xscale, float yscale)
  {
    visualScale = std::max(xscale, yscale);
    framework.UpdateVisualScale(visualScale);

    int w = 0, h = 0;
    glfwGetWindowSize(window, &w, &h);
#if defined(OMIM_OS_MAC)
    w *= xscale;
    h *= yscale;
#endif

    if (w != fbWidth || h != fbHeight)
    {
#if defined(OMIM_OS_MAC)
      UpdateContentScale(window, xscale);
#endif
      fbWidth = w;
      fbHeight = h;
      UpdateSize(make_ref(contextFactory), fbWidth, fbHeight);
      framework.OnSize(fbWidth, fbHeight);
    }
  };
  glfwSetWindowContentScaleCallback(window,
                                    [](GLFWwindow *, float xscale, float yscale) { onContentScale(xscale, yscale); });

  // Location handler
  std::optional<ms::LatLon> lastLatLon;
  bool bearingEnabled = false;
  float bearing = 0.0f;
  auto setUserLocation = [&]()
  {
    if (lastLatLon)
    {
      framework.OnLocationUpdate(
          location::GpsInfo{.m_source = location::EUser,
                            .m_timestamp = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                                   std::chrono::system_clock::now().time_since_epoch())
                                                                   .count()) /
                                           1000,
                            .m_latitude = lastLatLon->m_lat,
                            .m_longitude = lastLatLon->m_lon,
                            .m_horizontalAccuracy = 10,
                            .m_bearing = bearingEnabled ? bearing : -1.0f});
      if (bearingEnabled)
      {
        framework.OnCompassUpdate(location::CompassInfo{.m_bearing = base::DegToRad(bearing)});
      }
    }
  };

  // Download maps handler
  std::string downloadButtonLabel;
  std::string retryButtonLabel;
  std::string downloadStatusLabel;
  storage::CountryId lastCountry;
  auto const onCountryChanged = [&](storage::CountryId const & countryId)
  {
    downloadButtonLabel.clear();
    retryButtonLabel.clear();
    downloadStatusLabel.clear();
    lastCountry = countryId;
    // Called by Framework in World zoom level.
    if (countryId.empty())
      return;

    auto const & storage = framework.GetStorage();
    auto status = storage.CountryStatusEx(countryId);
    auto const & countryName = countryId;

    if (status == storage::Status::NotDownloaded)
    {
      std::string units;
      size_t sizeToDownload = 0;
      FormatMapSize(storage.CountrySizeInBytes(countryId).second, units, sizeToDownload);
      std::stringstream str;
      str << "Download (" << countryName << ") " << sizeToDownload << units;
      downloadButtonLabel = str.str();
    }
    else if (status == storage::Status::InQueue)
    {
      std::stringstream str;
      str << countryName << " is waiting for downloading";
      downloadStatusLabel = str.str();
    }
    else if (status != storage::Status::Downloading && status != storage::Status::OnDisk &&
             status != storage::Status::OnDiskOutOfDate)
    {
      std::stringstream str;
      str << "Retry to download " << countryName;
      retryButtonLabel = str.str();
    }
  };
  framework.SetCurrentCountryChangedListener(onCountryChanged);

  framework.GetStorage().Subscribe(
      [&](storage::CountryId const & countryId)
      {
        // Storage also calls notifications for parents, but we are interested in leafs only.
        if (framework.GetStorage().IsLeaf(countryId))
          onCountryChanged(countryId);
      },
      [&](storage::CountryId const & countryId, downloader::Progress const & progress)
      {
        std::stringstream str;
        str << "Downloading (" << countryId << ") " << (progress.m_bytesDownloaded * 100 / progress.m_bytesTotal)
            << "%";
        downloadStatusLabel = str.str();
        framework.MakeFrameActive();
      });

  // Handle mouse buttons.
  bool touchActive = false;
  int touchMods = 0;
  bool setUpLocationByLeftClick = false;
  onMouseButton = [&](double x, double y, int button, int action, int mods)
  {
#if defined(OMIM_OS_LINUX)
    ImGui::GetIO().MousePos = ImVec2(x / visualScale, y / visualScale);
#endif
    if (ImGui::GetIO().WantCaptureMouse)
    {
      framework.MakeFrameActive();
      return;
    }

#if defined(OMIM_OS_MAC)
    x *= visualScale;
    y *= visualScale;
#endif
    lastLatLon = mercator::ToLatLon(framework.PtoG(m2::PointD(x, y)));

    if (setUpLocationByLeftClick)
    {
      setUserLocation();
      return;
    }

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
      framework.TouchEvent(GetTouchEvent(framework, x, y, mods, df::TouchEvent::TOUCH_DOWN));
      touchActive = true;
      touchMods = mods;
    }

    if (touchActive && action == GLFW_RELEASE)
    {
      framework.TouchEvent(GetTouchEvent(framework, x, y, 0, df::TouchEvent::TOUCH_UP));
      touchActive = false;
      touchMods = 0;
    }
  };
  glfwSetMouseButtonCallback(window,
                             [](GLFWwindow * wnd, int button, int action, int mods)
                             {
                               double x, y;
                               glfwGetCursorPos(wnd, &x, &y);
                               onMouseButton(x, y, button, action, mods);
                             });

  // Handle mouse moving.
  onMouseMove = [&](double x, double y)
  {
#if defined(OMIM_OS_LINUX)
    ImGui::GetIO().MousePos = ImVec2(x / visualScale, y / visualScale);
#endif
    if (ImGui::GetIO().WantCaptureMouse)
      framework.MakeFrameActive();

    if (touchActive)
    {
#if defined(OMIM_OS_MAC)
      x *= visualScale;
      y *= visualScale;
#endif
      framework.TouchEvent(
          GetTouchEvent(framework, x, y, touchMods, df::TouchEvent::TOUCH_MOVE));
    }
  };
  glfwSetCursorPosCallback(window, [](GLFWwindow *, double x, double y) { onMouseMove(x, y); });

  // Handle scroll.
  onScroll = [&](double x, double y, double xOffset, double yOffset)
  {
#if defined(OMIM_OS_LINUX)
    ImGui::GetIO().MousePos = ImVec2(x / visualScale, y / visualScale);
#endif
    if (ImGui::GetIO().WantCaptureMouse)
    {
      framework.MakeFrameActive();
      return;
    }

#if defined(OMIM_OS_MAC)
    x *= visualScale;
    y *= visualScale;
#endif
    constexpr double kSensitivity = 0.01;
    double const factor = yOffset * kSensitivity;
    framework.Scale(exp(factor), m2::PointD(x, y), false);
  };
  glfwSetScrollCallback(window,
                        [](GLFWwindow * wnd, double xoffset, double yoffset)
                        {
                          double x, y;
                          glfwGetCursorPos(wnd, &x, &y);
                          onScroll(x, y, xoffset, yoffset);
                        });

  // Keys.
  onKeyboardButton = [&](int key, int scancode, int action, int mods) {};
  glfwSetKeyCallback(window, [](GLFWwindow *, int key, int scancode, int action, int mods)
                     { onKeyboardButton(key, scancode, action, mods); });

  // imGui UI
  bool enableDebugRectRendering = false;
  bool enableAA = false;
  auto imGuiUI = [&]()
  {
    ImGui::SetNextWindowPos(ImVec2(5, 20), ImGuiCond_Appearing);
    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    // Drape controls
    char const * apiLabels[] = {
#if defined(OMIM_OS_MAC)
      "Metal",
      "Vulkan",
      "OpenGL"
#elif defined(OMIM_OS_LINUX)
      "Vulkan",
      "OpenGL"
#endif
    };
    static int currentAPI = 0;
    if (ImGui::Combo("API", &currentAPI, apiLabels, IM_ARRAYSIZE(apiLabels)))
    {
      auto const apiVersion = GetApiVersion(apiLabels[currentAPI]);
      if (framework.GetDrapeEngine()->GetApiVersion() != apiVersion)
      {
        DestroyDrapeEngine();
        CreateDrapeEngine(apiVersion);
      }
    }
    if (ImGui::Checkbox("Debug rect rendering", &enableDebugRectRendering))
      framework.EnableDebugRectRendering(enableDebugRectRendering);
    if (ImGui::Checkbox("Antialiasing", &enableAA))
      framework.GetDrapeEngine()->SetPosteffectEnabled(df::PostprocessRenderer::Antialiasing, enableAA);
    ImGui::NewLine();
    ImGui::Separator();
    ImGui::NewLine();

    // Map controls
    if (ImGui::Button("Scale +"))
      framework.Scale(Framework::SCALE_MAG, true);
    ImGui::SameLine();
    if (ImGui::Button("Scale -"))
      framework.Scale(Framework::SCALE_MIN, true);
    ImGui::Checkbox("Set up location by left click", &setUpLocationByLeftClick);
    if (setUpLocationByLeftClick)
    {
      if (ImGui::Checkbox("Bearing", &bearingEnabled))
        setUserLocation();
      ImGui::SameLine();
      if (ImGui::SliderFloat(" ", &bearing, 0.0f, 360.0f, "%.1f"))
        setUserLocation();
    }
    ImGui::Text("My positon mode: %s", GetMyPoisitionText(framework.GetMyPositionMode()).data());
    if (ImGui::Button("Next Position Mode"))
      framework.SwitchMyPositionNextMode();
    ImGui::NewLine();
    ImGui::Separator();
    ImGui::NewLine();

    // No downloading on Linux at the moment, need to implement http_thread without Qt.
#if !defined(OMIM_OS_LINUX)
    // Download controls
    if (!downloadButtonLabel.empty())
    {
      if (ImGui::Button(downloadButtonLabel.c_str()))
        framework.GetStorage().DownloadNode(lastCountry);
    }
    if (!retryButtonLabel.empty())
    {
      if (ImGui::Button(retryButtonLabel.c_str()))
        framework.GetStorage().RetryDownloadNode(lastCountry);
    }
    if (!downloadStatusLabel.empty())
      ImGui::Text("%s", downloadStatusLabel.c_str());
    if (!downloadButtonLabel.empty() || !retryButtonLabel.empty() || !downloadStatusLabel.empty())
    {
      ImGui::NewLine();
      ImGui::Separator();
      ImGui::NewLine();
    }
#endif

    ImGui::End();
  };

  ImGui_ImplGlfw_InitForOther(window, true);

  // Main loop.
  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents();

#if defined(OMIM_OS_LINUX)
    guiThreadPtr->ExecuteTasks();
#endif

    // Render imGui UI
    ImGui_ImplGlfw_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
#if defined(OMIM_OS_LINUX)
    // Apply correct visual scale on Linux
    // In glfw for Linux, window size and framebuffer size are the same,
    // even if visual scale is not 1.0. It's different from behaviour on Mac.
    io.DisplaySize = ImVec2(fbWidth / visualScale, fbHeight / visualScale);
    io.DisplayFramebufferScale = ImVec2(visualScale, visualScale);
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    io.AddMousePosEvent((float)mouseX / visualScale, (float)mouseY / visualScale);
#endif
    io.IniFilename = nullptr;
    imguiRenderer.Update(imGuiUI);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000 / 30));
  }

  framework.EnterBackground();
  DestroyDrapeEngine();

  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
