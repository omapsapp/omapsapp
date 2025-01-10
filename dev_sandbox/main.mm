#include "iphone/Maps/Classes/MetalContextFactory.h"

#include "drape/gl_functions.hpp"
#include "drape/oglcontext.hpp"
#include "drape/metal/metal_base_context.hpp"
#include "drape/vulkan/vulkan_context_factory.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#if __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#else
#error Unsupported OS
#endif
#include <GLFW/glfw3native.h>

#import <AppKit/NSOpenGL.h>
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <vulkan/vulkan_macos.h>

#include <atomic>
#include <memory>

class MacOSVulkanContextFactory : public dp::vulkan::VulkanContextFactory
{
public:
  MacOSVulkanContextFactory()
    : dp::vulkan::VulkanContextFactory(1, 33, false) {}

  void SetSurface(CAMetalLayer *layer)
  {
    VkMacOSSurfaceCreateInfoMVK createInfo = {
      .sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK,
      .flags = 0,
      .pView = static_cast<const void *>(CFBridgingRetain(layer)),
    };

    VkResult statusCode;
    CHECK(vkCreateMacOSSurfaceMVK, ());
    statusCode = vkCreateMacOSSurfaceMVK(m_vulkanInstance, &createInfo, nullptr,
                                          &m_surface);
    if (statusCode != VK_SUCCESS)
    {
      LOG_ERROR_VK_CALL(vkCreateMacOSSurfaceMVK, statusCode);
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

class MacGLContext : public dp::OGLContext
{
public:
  MacGLContext(MacGLContext * contextToShareWith)
    : m_viewSet(false)
  {
    NSOpenGLPixelFormatAttribute attributes[] = {
      NSOpenGLPFAAccelerated,
      NSOpenGLPFAOpenGLProfile,
      NSOpenGLProfileVersion4_1Core,
      NSOpenGLPFADoubleBuffer,
      NSOpenGLPFAColorSize,
      24,
      NSOpenGLPFAAlphaSize,
      8,
      NSOpenGLPFADepthSize,
      24,
      NSOpenGLPFAStencilSize,
      8,
      0
    };
    m_pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    CHECK(m_pixelFormat, ("Pixel format is not found"));
    m_context = [[NSOpenGLContext alloc] initWithFormat:m_pixelFormat 
                                           shareContext:(contextToShareWith ? contextToShareWith->m_context : nil)];
    int interval = 1;
    [m_context getValues:&interval forParameter:NSOpenGLContextParameterSwapInterval];
  }

  ~MacGLContext()
  {
    @autoreleasepool {
      [m_context clearDrawable];
      m_pixelFormat = nil;
      m_context = nil;
    }
  }

  bool BeginRendering() override { return m_viewSet; }

  void Present() override
  {
    if (m_viewSet)
    {
      std::lock_guard<std::mutex> lock(m_updateSizeMutex);
      [m_context flushBuffer];
    }
  }

  void MakeCurrent() override
  {
    [m_context makeCurrentContext];
  }

  void DoneCurrent() override
  {
    [NSOpenGLContext clearCurrentContext];
  }

  void SetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer) override
  {
    if (framebuffer)
      framebuffer->Bind();
    else
      GLFunctions::glBindFramebuffer(0);
  }

  void SetView(NSView* view)
  {
    [m_context setView: view];
    [m_context update];
    m_viewSet = true;
  }

  void UpdateSize(int w, int h)
  {
    std::lock_guard<std::mutex> lock(m_updateSizeMutex);
    [m_context update];
  }

private:
  NSOpenGLPixelFormat * m_pixelFormat = nil;
  NSOpenGLContext* m_context = nil;
  std::atomic<bool> m_viewSet;

  std::mutex m_updateSizeMutex;
};

class MacGLContextFactory: public dp::GraphicsContextFactory
{
public:
  dp::GraphicsContext * GetDrawContext() override
  {
    bool needNotify = false;
    {
      std::lock_guard<std::mutex> lock(m_contextAccess);
      if (m_drawContext == nullptr)
      {
        m_drawContext = std::make_unique<MacGLContext>(m_uploadContext.get());
        needNotify = true;
      }
    }
    if (needNotify)
      NotifyView();

    std::lock_guard<std::mutex> lock(m_contextAccess);
    return m_drawContext.get();
  }

  dp::GraphicsContext * GetResourcesUploadContext() override
  {
    std::lock_guard<std::mutex> lock(m_contextAccess);
    if (m_uploadContext == nullptr)
      m_uploadContext = std::make_unique<MacGLContext>(m_drawContext.get());
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

  void SetView(NSView* view)
  {
    bool needWait;
    {
      std::lock_guard<std::mutex> lock(m_contextAccess);
      needWait = (m_drawContext == nullptr);
    }
    if (needWait)
    {
      std::unique_lock<std::mutex> lock(m_viewSetMutex);
      m_viewSetCondition.wait(lock, [this] { return m_viewSet; });
    }

    std::lock_guard<std::mutex> lock(m_contextAccess);
    CHECK(m_drawContext, ());
    m_drawContext->SetView(view);
  }

  void UpdateSize(int w, int h)
  {
    std::lock_guard<std::mutex> lock(m_contextAccess);
    if (m_drawContext)
      m_drawContext->UpdateSize(w, h);
  }
  
private:
  void NotifyView()
  {
    std::lock_guard<std::mutex> lock(m_viewSetMutex);
    m_viewSet = true;
    m_viewSetCondition.notify_all();
  }

  static size_t constexpr kGLThreadsCount = 2;

  std::unique_ptr<MacGLContext> m_drawContext;
  std::unique_ptr<MacGLContext> m_uploadContext;

  mutable std::mutex m_contextAccess;

  bool m_isInitialized = false;
  size_t m_initializationCounter = 0;
  std::condition_variable m_initializationCondition;
  std::mutex m_initializationMutex;

  bool m_viewSet = false;
  std::condition_variable m_viewSetCondition;
  std::mutex m_viewSetMutex;
};

drape_ptr<dp::GraphicsContextFactory> CreateContextFactory(GLFWwindow *window, dp::ApiVersion api, m2::PointU size) 
{
  if (api == dp::ApiVersion::Metal) 
  {
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = MTLCreateSystemDefaultDevice();
    layer.opaque = YES;
    layer.displaySyncEnabled = YES;

    NSWindow *nswindow = glfwGetCocoaWindow(window);
    NSScreen *screen = [NSScreen mainScreen];
    CGFloat factor = [screen backingScaleFactor];
    layer.contentsScale = factor;
    nswindow.contentView.layer = layer;
    nswindow.contentView.wantsLayer = YES;

    return make_unique_dp<MetalContextFactory>(layer, size);
  }

  if (api == dp::ApiVersion::Vulkan) 
  {
    CAMetalLayer *layer = [CAMetalLayer layer];
    layer.device = MTLCreateSystemDefaultDevice();
    layer.opaque = YES;
    layer.displaySyncEnabled = YES;

    NSWindow *nswindow = glfwGetCocoaWindow(window);
    NSScreen *screen = [NSScreen mainScreen];
    CGFloat factor = [screen backingScaleFactor];
    layer.contentsScale = factor;
    nswindow.contentView.layer = layer;
    nswindow.contentView.wantsLayer = YES;

    auto contextFactory = make_unique_dp<MacOSVulkanContextFactory>();
    contextFactory->SetSurface(layer);
    return contextFactory;
  }

  if (api == dp::ApiVersion::OpenGLES3) 
  {
    NSWindow *nswindow = glfwGetCocoaWindow(window);
    [nswindow.contentView setWantsBestResolutionOpenGLSurface:YES];
    return make_unique_dp<MacGLContextFactory>();
  }

  ASSERT(false, ("API is not available yet"));
  return nullptr;
}

void OnCreateDrapeEngine(GLFWwindow *window, dp::ApiVersion api, 
                         ref_ptr<dp::GraphicsContextFactory> contextFactory)
{
  if (api == dp::ApiVersion::OpenGLES3) 
  {
    NSWindow *nswindow = glfwGetCocoaWindow(window);
    ref_ptr<MacGLContextFactory> macosContextFactory = contextFactory;
    macosContextFactory->SetView(nswindow.contentView);
  }
}

void PrepareDestroyContextFactory(ref_ptr<dp::GraphicsContextFactory> contextFactory) 
{
  auto const api = contextFactory->GetDrawContext()->GetApiVersion();
  if (api == dp::ApiVersion::Metal || api == dp::ApiVersion::OpenGLES3) 
  {
    // Do nothing
  }
  else if (api == dp::ApiVersion::Vulkan) 
  {
    ref_ptr<MacOSVulkanContextFactory> macosContextFactory = contextFactory;
    macosContextFactory->ResetSurface();
  }
  else
  {
    ASSERT(false, ("API is not available yet"));
  }
}

void UpdateContentScale(GLFWwindow *window, float scale) 
{
  NSWindow *nswindow = glfwGetCocoaWindow(window);
  if (nswindow.contentView.layer)
    nswindow.contentView.layer.contentsScale = scale;
}

void UpdateSize(ref_ptr<dp::GraphicsContextFactory> contextFactory, int w, int h) 
{
  if (!contextFactory || !contextFactory->GetDrawContext())
    return;

  auto const api = contextFactory->GetDrawContext()->GetApiVersion();
  if (api == dp::ApiVersion::OpenGLES3) 
  {
    ref_ptr<MacGLContextFactory> macosContextFactory = contextFactory;
    macosContextFactory->UpdateSize(w, h);
  }
}
