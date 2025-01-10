#include "std/target_os.hpp"
#if !defined(OMIM_OS_LINUX)
#error Unsupported OS
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <dlfcn.h>
#include <X11/X.h>

#include <vulkan_wrapper.h>
#include <vulkan/vulkan_xlib.h>
// Workaround for TestFunction::Always compilation issue:
// /usr/include/X11/X.h:441:33: note: expanded from macro 'Always'
#undef Always
// Workaround for storage::Status compilation issue:
// /usr/include/X11/Xlib.h:83:16: note: expanded from macro 'Status'
#undef Status

#include "drape/vulkan/vulkan_context_factory.hpp"

#include "drape/gl_functions.hpp"
#include "drape/gl_includes.hpp"
#include "drape/oglcontext.hpp"

#include <array>
#include <atomic>
#include <memory>

class LinuxVulkanContextFactory : public dp::vulkan::VulkanContextFactory
{
public:
  LinuxVulkanContextFactory() : dp::vulkan::VulkanContextFactory(1, 33, false) {}

  void SetSurface(Display * display, Window window)
  {
    VkXlibSurfaceCreateInfoKHR const createInfo = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .dpy = display,
        .window = window,
    };

    VkResult statusCode;
    CHECK(vkCreateXlibSurfaceKHR, ());
    statusCode = vkCreateXlibSurfaceKHR(m_vulkanInstance, &createInfo, nullptr, &m_surface);
    if (statusCode != VK_SUCCESS)
    {
      LOG_ERROR_VK_CALL(vkCreateXlibSurfaceKHR, statusCode);
      return;
    }

    uint32_t const renderingQueueIndex = m_drawContext->GetRenderingQueueFamilyIndex();
    VkBool32 supportsPresent;
    statusCode = vkGetPhysicalDeviceSurfaceSupportKHR(m_gpu, renderingQueueIndex, m_surface, &supportsPresent);
    if (statusCode != VK_SUCCESS)
    {
      LOG_ERROR_VK_CALL(vkGetPhysicalDeviceSurfaceSupportKHR, statusCode);
      return;
    }
    CHECK_EQUAL(supportsPresent, VK_TRUE, ());

    CHECK(QuerySurfaceSize(), ());

    if (m_drawContext)
      m_drawContext->SetSurface(m_surface, m_surfaceFormat, m_surfaceCapabilities);
  }

  void ResetSurface()
  {
    if (m_drawContext)
      m_drawContext->ResetSurface(false);

    vkDestroySurfaceKHR(m_vulkanInstance, m_surface, nullptr);
  }
};

// Based on: https://github.com/glfw/glfw/blob/master/src/glx_context.c
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#define GLX_PBUFFER_HEIGHT 0x8040
#define GLX_PBUFFER_WIDTH 0x8041
#define GLX_DOUBLEBUFFER 5
#define GLX_DRAWABLE_TYPE 0x8010
#define GLX_RENDER_TYPE 0x8011
#define GLX_WINDOW_BIT 0x00000001
#define GLX_PBUFFER_BIT 0x00000004
#define GLX_RGBA_BIT 0x00000001
#define GLX_RED_SIZE 8
#define GLX_GREEN_SIZE 9
#define GLX_BLUE_SIZE 10
#define GLX_ALPHA_SIZE 11
#define GLX_DEPTH_SIZE 12
#define GLX_STENCIL_SIZE 13

typedef XID GLXDrawable;
typedef struct __GLXcontext * GLXContext;
typedef XID GLXPbuffer;
typedef struct __GLXFBConfig * GLXFBConfig;
typedef void (*__GLXextproc)(void);

typedef __GLXextproc (*PFNGLXGETPROCADDRESSPROC)(const GLubyte * procName);

typedef int (*PFNXFREE)(void *);
typedef GLXFBConfig * (*PFNGLXCHOOSEFBCONFIGPROC)(Display *, int, const int *, int *);
typedef GLXContext (*PFNGLXCREATECONTEXTATTRIBSARB)(Display *, GLXFBConfig, GLXContext, Bool, const int *);
typedef void (*PFNGLXDESTROYCONTEXT)(Display *, GLXContext);
typedef GLXPbuffer (*PFNGLXCREATEPBUFFERPROC)(Display *, GLXFBConfig, const int *);
typedef void (*PFNGLXDESTROYPBUFFER)(Display *, GLXPbuffer);
typedef Bool (*PFNGLXMAKECURRENTPROC)(Display *, GLXDrawable, GLXContext);
typedef void (*PFNGLXSWAPBUFFERSPROC)(Display *, GLXDrawable);

struct GLXFunctions
{
  GLXFunctions()
  {
    std::array<char const *, 3> libs = {
        "libGLX.so.0",
        "libGL.so.1",
        "libGL.so",
    };

    for (char const * lib : libs)
    {
      m_module = dlopen(lib, RTLD_LAZY | RTLD_LOCAL);
      if (m_module)
      {
        break;
      }
    }

    CHECK(m_module != nullptr, ("Failed to initialize GLX"));

    XFree = loadFunction<PFNXFREE>("XFree");

    glXGetProcAddress = loadFunction<PFNGLXGETPROCADDRESSPROC>("glXGetProcAddress");
    glXGetProcAddressARB = loadFunction<PFNGLXGETPROCADDRESSPROC>("glXGetProcAddressARB");

    glXChooseFBConfig = loadGlxFunction<PFNGLXCHOOSEFBCONFIGPROC>("glXChooseFBConfig");
    glXCreateContextAttribsARB = loadGlxFunction<PFNGLXCREATECONTEXTATTRIBSARB>("glXCreateContextAttribsARB");

    glXDestroyContext = loadGlxFunction<PFNGLXDESTROYCONTEXT>("glXDestroyContext");
    glXCreatePbuffer = loadGlxFunction<PFNGLXCREATEPBUFFERPROC>("glXCreatePbuffer");
    glXDestroyPbuffer = loadGlxFunction<PFNGLXDESTROYPBUFFER>("glXDestroyPbuffer");
    glXMakeCurrent = loadGlxFunction<PFNGLXMAKECURRENTPROC>("glXMakeCurrent");
    glXSwapBuffers = loadGlxFunction<PFNGLXSWAPBUFFERSPROC>("glXSwapBuffers");
  }

  ~GLXFunctions()
  {
    if (m_module)
    {
      dlclose(m_module);
    }
  }

  PFNXFREE XFree = nullptr;

  PFNGLXGETPROCADDRESSPROC glXGetProcAddress = nullptr;
  PFNGLXGETPROCADDRESSPROC glXGetProcAddressARB = nullptr;

  PFNGLXCHOOSEFBCONFIGPROC glXChooseFBConfig = nullptr;
  PFNGLXCREATECONTEXTATTRIBSARB glXCreateContextAttribsARB = nullptr;
  PFNGLXDESTROYCONTEXT glXDestroyContext = nullptr;
  PFNGLXCREATEPBUFFERPROC glXCreatePbuffer = nullptr;
  PFNGLXDESTROYPBUFFER glXDestroyPbuffer = nullptr;
  PFNGLXMAKECURRENTPROC glXMakeCurrent = nullptr;
  PFNGLXSWAPBUFFERSPROC glXSwapBuffers = nullptr;

private:
  template <typename T>
  T loadFunction(char const * func)
  {
    auto f = reinterpret_cast<T>(dlsym(m_module, func));
    ASSERT(f, ("Failed to initialize GLX:", func, "is not found"));
    return f;
  }

  template <typename T>
  T loadGlxFunction(char const * func)
  {
    if (auto f = reinterpret_cast<T>(glXGetProcAddress(reinterpret_cast<GLubyte const *>(func))))
      return f;

    if (auto f = reinterpret_cast<T>(glXGetProcAddressARB(reinterpret_cast<GLubyte const *>(func))))
      return f;

    return loadFunction<T>(func);
  }

  void * m_module = nullptr;
};

class LinuxGLContext : public dp::OGLContext
{
public:
  LinuxGLContext(GLXFunctions const & glx, Display * display, Window window, LinuxGLContext * contextToShareWith,
                 bool usePixelBuffer)
    : m_glx(glx), m_display(display), m_window(window)
  {
    static int visualAttribs[] = {
      GLX_DOUBLEBUFFER, True,
      GLX_RENDER_TYPE, GLX_RGBA_BIT,
      GLX_DRAWABLE_TYPE, (usePixelBuffer ? GLX_PBUFFER_BIT : GLX_WINDOW_BIT),
      GLX_RED_SIZE, 8,
      GLX_GREEN_SIZE, 8,
      GLX_BLUE_SIZE, 8,
      GLX_ALPHA_SIZE, 8,
      GLX_DEPTH_SIZE, 24,
      GLX_STENCIL_SIZE, 8,
      None
    };
    int contextAttribs[] = {
      GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
      GLX_CONTEXT_MAJOR_VERSION_ARB, 4, 
      GLX_CONTEXT_MINOR_VERSION_ARB, 1,
      None};
    int fbcount = 0;
    if (GLXFBConfig * config = m_glx.glXChooseFBConfig(display, DefaultScreen(display), visualAttribs, &fbcount))
    {
      m_context =
          m_glx.glXCreateContextAttribsARB(display, config[0], contextToShareWith ? contextToShareWith->m_context : 0, True, contextAttribs);
      CHECK(m_context != nullptr, ("Failed to create GLX context"));

      if (usePixelBuffer)
      {
        int pbufferAttribs[] = {GLX_PBUFFER_WIDTH, 1, GLX_PBUFFER_HEIGHT, 1, None};

        m_pixelBufferHandle = m_glx.glXCreatePbuffer(display, config[0], pbufferAttribs);
        CHECK(m_pixelBufferHandle != 0, ("Failed to create GLX pbuffer"));
      }

      m_glx.XFree(config);
    }
  }

  ~LinuxGLContext() override
  {
    if (m_pixelBufferHandle)
    {
      m_glx.glXDestroyPbuffer(m_display, m_pixelBufferHandle);
      m_pixelBufferHandle = 0;
    }
    if (m_context)
    {
      m_glx.glXDestroyContext(m_display, m_context);
      m_context = nullptr;
    }
  }

  void Present() override
  {
    if (!m_pixelBufferHandle)
      m_glx.glXSwapBuffers(m_display, m_window);
  }

  void MakeCurrent() override
  {
    if (!m_glx.glXMakeCurrent(m_display, m_pixelBufferHandle ? m_pixelBufferHandle : m_window, m_context))
      LOG(LERROR, ("MakeCurrent(): glXMakeCurrent failed"));
  }

  void DoneCurrent() override
  {
    if (!m_glx.glXMakeCurrent(m_display, None, nullptr))
      LOG(LERROR, ("DoneCurrent(): glXMakeCurrent failed"));
  }

  void SetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer) override
  {
    if (framebuffer)
      framebuffer->Bind();
    else
      GLFunctions::glBindFramebuffer(0);
  }

private:
  GLXFunctions const & m_glx;

  Display * m_display = nullptr;
  Window m_window = 0;
  GLXDrawable m_pixelBufferHandle = 0;
  GLXContext m_context = nullptr;
};

class LinuxContextFactory : public dp::GraphicsContextFactory
{
public:
  LinuxContextFactory(Display * display, Window window) : m_display(display), m_window(window) {}

  dp::GraphicsContext * GetDrawContext() override
  {
    std::lock_guard<std::mutex> lock(m_contextAccess);
    if (m_drawContext == nullptr)
      m_drawContext = std::make_unique<LinuxGLContext>(m_glx, m_display, m_window, m_uploadContext.get(), false);
    return m_drawContext.get();
  }

  dp::GraphicsContext * GetResourcesUploadContext() override
  {
    std::lock_guard<std::mutex> lock(m_contextAccess);
    if (m_uploadContext == nullptr)
      m_uploadContext = std::make_unique<LinuxGLContext>(m_glx, m_display, 0, m_drawContext.get(), true);
    return m_uploadContext.get();
  }

  void WaitForInitialization(dp::GraphicsContext *) override
  {
    std::unique_lock<std::mutex> lock(m_initializationMutex);
    if (m_isInitialized)
      return;

    m_initializationCounter++;
    if (m_initializationCounter >= kGLThreadsCount)
    {
      m_isInitialized = true;
      m_initializationCondition.notify_all();
    }
    else
    {
      m_initializationCondition.wait(lock, [this] { return m_isInitialized; });
    }
  }

  bool IsDrawContextCreated() const override
  {
    std::lock_guard<std::mutex> lock(m_contextAccess);
    return m_drawContext != nullptr;
  }

  bool IsUploadContextCreated() const override
  {
    std::lock_guard<std::mutex> lock(m_contextAccess);
    return m_uploadContext != nullptr;
  }

private:
  static size_t constexpr kGLThreadsCount = 2;

  GLXFunctions m_glx;

  Display * m_display = nullptr;
  Window m_window = 0;

  std::unique_ptr<LinuxGLContext> m_drawContext;
  std::unique_ptr<LinuxGLContext> m_uploadContext;

  mutable std::mutex m_contextAccess;

  bool m_isInitialized = false;
  size_t m_initializationCounter = 0;
  std::condition_variable m_initializationCondition;
  std::mutex m_initializationMutex;
};

drape_ptr<dp::GraphicsContextFactory> CreateContextFactory(GLFWwindow * window, dp::ApiVersion api, m2::PointU size)
{
  if (api == dp::ApiVersion::Vulkan)
  {
    auto contextFactory = make_unique_dp<LinuxVulkanContextFactory>();
    contextFactory->SetSurface(glfwGetX11Display(), glfwGetX11Window(window));
    return contextFactory;
  }

  if (api == dp::ApiVersion::OpenGLES3)
  {
    return make_unique_dp<LinuxContextFactory>(glfwGetX11Display(), glfwGetX11Window(window));
  }

  ASSERT(false, ("API is not available yet"));
  return nullptr;
}

void OnCreateDrapeEngine(GLFWwindow * window, dp::ApiVersion api, ref_ptr<dp::GraphicsContextFactory> contextFactory)
{
  // Do nothing
}

void PrepareDestroyContextFactory(ref_ptr<dp::GraphicsContextFactory> contextFactory)
{
  auto const api = contextFactory->GetDrawContext()->GetApiVersion();
  if (api == dp::ApiVersion::OpenGLES3)
  {
    // Do nothing
  }
  else if (api == dp::ApiVersion::Vulkan)
  {
    ref_ptr<LinuxVulkanContextFactory> linuxContextFactory = contextFactory;
    linuxContextFactory->ResetSurface();
  }
  else
  {
    ASSERT(false, ("API is not available yet"));
  }
}

void UpdateContentScale(GLFWwindow * window, float scale)
{
  // Do nothing
}

void UpdateSize(ref_ptr<dp::GraphicsContextFactory> contextFactory, int w, int h)
{
  // Do nothing
}
