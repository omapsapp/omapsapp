#include "drape/hw_texture.hpp"

#include "drape/drape_global.hpp"
#include "drape/gl_functions.hpp"
#include "drape/utils/gpu_mem_tracker.hpp"

#include "base/logging.hpp"

#include "std/target_os.hpp"

#if defined(OMIM_OS_IPHONE)
#include "drape/hw_texture_ios.hpp"
#endif

#if defined(OMIM_METAL_AVAILABLE)
extern drape_ptr<dp::HWTextureAllocator> CreateMetalAllocator();
extern ref_ptr<dp::HWTextureAllocator> GetDefaultMetalAllocator();
#endif

extern drape_ptr<dp::HWTextureAllocator> CreateVulkanAllocator();
extern ref_ptr<dp::HWTextureAllocator> GetDefaultVulkanAllocator();

namespace dp
{
std::string DebugPrint(HWTexture::Params const & p)
{
  std::ostringstream ss;
  ss << "Width = " << p.m_width
     << "; Height = " << p.m_height
     << "; Format = " << DebugPrint(p.m_format)
     << "; IsRenderTarget = " << p.m_isRenderTarget;
  return ss.str();
}

void UnpackFormat(ref_ptr<dp::GraphicsContext> context, TextureFormat format,
                  glConst & layout, glConst & pixelType)
{
  auto const apiVersion = context->GetApiVersion();
  CHECK(apiVersion == dp::ApiVersion::OpenGLES2 || apiVersion == dp::ApiVersion::OpenGLES3, ());

  switch (format)
  {
  case TextureFormat::RGBA8:
    layout = gl_const::GLRGBA;
    pixelType = gl_const::GL8BitOnChannel;
    return;

  case TextureFormat::Alpha:
    // On OpenGL ES3 GLAlpha is not supported, we use GLRed instead.
    layout = apiVersion == dp::ApiVersion::OpenGLES2 ? gl_const::GLAlpha : gl_const::GLRed;
    pixelType = gl_const::GL8BitOnChannel;
    return;

  case TextureFormat::RedGreen:
    // On OpenGL ES2 2-channel textures are not supported.
    layout = (apiVersion == dp::ApiVersion::OpenGLES2) ? gl_const::GLRGBA : gl_const::GLRedGreen;
    pixelType = gl_const::GL8BitOnChannel;
    return;

  case TextureFormat::DepthStencil:
    // OpenGLES2 does not support texture-based depth-stencil.
    CHECK(apiVersion != dp::ApiVersion::OpenGLES2, ());
    layout = gl_const::GLDepthStencil;
    pixelType = gl_const::GLUnsignedInt24_8Type;
    return;

  case TextureFormat::Depth:
    layout = gl_const::GLDepthComponent;
    pixelType = gl_const::GLUnsignedIntType;
    return;

  case TextureFormat::Unspecified:
    CHECK(false, ());
    return;
  }
  UNREACHABLE();
}

glConst DecodeTextureFilter(TextureFilter filter)
{
  switch (filter)
  {
  case TextureFilter::Linear: return gl_const::GLLinear;
  case TextureFilter::Nearest: return gl_const::GLNearest;
  }
  UNREACHABLE();
}

glConst DecodeTextureWrapping(TextureWrapping wrapping)
{
  switch (wrapping)
  {
  case TextureWrapping::ClampToEdge: return gl_const::GLClampToEdge;
  case TextureWrapping::Repeat: return gl_const::GLRepeat;
  }
  UNREACHABLE();
}

HWTexture::~HWTexture()
{
#if defined(TRACK_GPU_MEM)
  dp::GPUMemTracker::Inst().RemoveDeallocated("Texture", m_textureID);
  dp::GPUMemTracker::Inst().RemoveDeallocated("PBO", m_pixelBufferID);
#endif
}

void HWTexture::Create(ref_ptr<dp::GraphicsContext> context, Params const & params)
{
  Create(context, params, nullptr);
}

void HWTexture::Create(ref_ptr<dp::GraphicsContext>, Params const & params, ref_ptr<void>)
{
  m_params = params;

#if defined(TRACK_GPU_MEM)
  uint32_t const memSize = (CHAR_BIT * bytesPerPixel * m_params.m_width * m_params.m_height) >> 3;
  dp::GPUMemTracker::Inst().AddAllocated("Texture", m_textureID, memSize);
  dp::GPUMemTracker::Inst().SetUsed("Texture", m_textureID, memSize);
  if (params.m_usePixelBuffer)
  {
    dp::GPUMemTracker::Inst().AddAllocated("PBO", m_pixelBufferID, m_pixelBufferSize);
    dp::GPUMemTracker::Inst().SetUsed("PBO", m_pixelBufferID, m_pixelBufferSize);
  }
#endif
}

TextureFormat HWTexture::GetFormat() const { return m_params.m_format; }

uint32_t HWTexture::GetWidth() const
{
  ASSERT(Validate(), ());
  return m_params.m_width;
}

uint32_t HWTexture::GetHeight() const
{
  ASSERT(Validate(), ());
  return m_params.m_height;
}

float HWTexture::GetS(uint32_t x) const
{
  ASSERT(Validate(), ());
  return x / static_cast<float>(m_params.m_width);
}

float HWTexture::GetT(uint32_t y) const
{
  ASSERT(Validate(), ());
  return y / static_cast<float>(m_params.m_height);
}

uint32_t HWTexture::GetID() const { return m_textureID; }

OpenGLHWTexture::~OpenGLHWTexture()
{
  if (m_textureID != 0)
    GLFunctions::glDeleteTexture(m_textureID);

  if (m_pixelBufferID != 0)
    GLFunctions::glDeleteBuffer(m_pixelBufferID);
}

void OpenGLHWTexture::Create(ref_ptr<dp::GraphicsContext> context, Params const & params,
                             ref_ptr<void> data)
{
  Base::Create(context, params, data);

  CHECK(context && m_textureID == 0 && m_pixelBufferID == 0, ());

  // OpenGL ES 2.0 does not support PBO.
  if (params.m_usePixelBuffer && context->GetApiVersion() == dp::ApiVersion::OpenGLES3)
  {
    m_pixelBufferElementSize = GetBytesPerPixel(m_params.m_format);
    if (m_pixelBufferElementSize > 0)
    {
      // Buffer size = 10%
      m_pixelBufferSize = static_cast<uint32_t>(0.1 * m_params.m_width * m_params.m_height * m_pixelBufferElementSize);
    }
  }

  m_textureID = GLFunctions::glGenTexture();
  Bind(context);

  UnpackFormat(context, m_params.m_format, m_unpackedLayout, m_unpackedPixelType);

  auto const f = DecodeTextureFilter(m_params.m_filter);
  GLFunctions::glTexImage2D(m_params.m_width, m_params.m_height,
                            m_unpackedLayout, m_unpackedPixelType, data.get());
  GLFunctions::glTexParameter(gl_const::GLMinFilter, f);
  GLFunctions::glTexParameter(gl_const::GLMagFilter, f);
  GLFunctions::glTexParameter(gl_const::GLWrapS, DecodeTextureWrapping(m_params.m_wrapSMode));
  GLFunctions::glTexParameter(gl_const::GLWrapT, DecodeTextureWrapping(m_params.m_wrapTMode));

  if (m_pixelBufferSize > 0)
  {
    m_pixelBufferID = GLFunctions::glGenBuffer();
    GLFunctions::glBindBuffer(m_pixelBufferID, gl_const::GLPixelBufferWrite);
    GLFunctions::glBufferData(gl_const::GLPixelBufferWrite, m_pixelBufferSize, nullptr,
                              gl_const::GLDynamicDraw);
    GLFunctions::glBindBuffer(0, gl_const::GLPixelBufferWrite);
  }

  GLFunctions::glBindTexture(0);
  GLFunctions::glFlush();
}

void OpenGLHWTexture::UploadData(ref_ptr<dp::GraphicsContext>, uint32_t x, uint32_t y,
                                 uint32_t width, uint32_t height, ref_ptr<void> data)
{
  ASSERT(Validate(), ());
  uint32_t const mappingSize = height * width * m_pixelBufferElementSize;
  if (m_pixelBufferID != 0 && m_pixelBufferSize >= mappingSize)
  {
    ASSERT_GREATER(mappingSize, 0, ());
    GLFunctions::glBindBuffer(m_pixelBufferID, gl_const::GLPixelBufferWrite);
    GLFunctions::glBufferSubData(gl_const::GLPixelBufferWrite, mappingSize, data.get(), 0);
    GLFunctions::glTexSubImage2D(x, y, width, height, m_unpackedLayout, m_unpackedPixelType, nullptr);
    GLFunctions::glBindBuffer(0, gl_const::GLPixelBufferWrite);
  }
  else
  {
    GLFunctions::glTexSubImage2D(x, y, width, height, m_unpackedLayout, m_unpackedPixelType, data.get());
  }
}

void OpenGLHWTexture::Bind(ref_ptr<dp::GraphicsContext> context) const
{
  UNUSED_VALUE(context);
  ASSERT(Validate(), ());
  if (m_textureID != 0)
    GLFunctions::glBindTexture(GetID());
}

void OpenGLHWTexture::SetFilter(TextureFilter filter)
{
  ASSERT(Validate(), ());
  if (m_params.m_filter != filter)
  {
    m_params.m_filter = filter;
    auto const f = DecodeTextureFilter(m_params.m_filter);
    GLFunctions::glTexParameter(gl_const::GLMinFilter, f);
    GLFunctions::glTexParameter(gl_const::GLMagFilter, f);
  }
}

bool OpenGLHWTexture::Validate() const
{
  return GetID() != 0;
}

drape_ptr<HWTexture> OpenGLHWTextureAllocator::CreateTexture(ref_ptr<dp::GraphicsContext> context)
{
  UNUSED_VALUE(context);
  return make_unique_dp<OpenGLHWTexture>();
}

void OpenGLHWTextureAllocator::Flush()
{
  GLFunctions::glFlush();
}

drape_ptr<HWTextureAllocator> CreateAllocator(ref_ptr<dp::GraphicsContext> context)
{
  CHECK(context != nullptr, ());
  auto const apiVersion = context->GetApiVersion();
  if (apiVersion == dp::ApiVersion::Metal)
  {
#if defined(OMIM_METAL_AVAILABLE)
    return CreateMetalAllocator();
#endif
    CHECK(false, ("Metal rendering is supported now only on iOS."));
    return nullptr;
  }

  if (apiVersion == dp::ApiVersion::Vulkan)
    return CreateVulkanAllocator();

  if (apiVersion == dp::ApiVersion::OpenGLES3)
    return make_unique_dp<OpenGLHWTextureAllocator>();

#if defined(OMIM_OS_IPHONE)
  return make_unique_dp<HWTextureAllocatorApple>();
#else
  return make_unique_dp<OpenGLHWTextureAllocator>();
#endif
}

ref_ptr<HWTextureAllocator> GetDefaultAllocator(ref_ptr<dp::GraphicsContext> context)
{
  CHECK(context != nullptr, ());
  auto const apiVersion = context->GetApiVersion();
  if (apiVersion == dp::ApiVersion::Metal)
  {
#if defined(OMIM_METAL_AVAILABLE)
    return GetDefaultMetalAllocator();
#endif
    CHECK(false, ("Metal rendering is supported now only on iOS."));
    return nullptr;
  }

  if (apiVersion == dp::ApiVersion::Vulkan)
    return GetDefaultVulkanAllocator();

  static OpenGLHWTextureAllocator s_allocator;
  return make_ref<HWTextureAllocator>(&s_allocator);
}
}  // namespace dp
