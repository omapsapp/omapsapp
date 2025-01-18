#pragma once

#include "drape/graphics_context.hpp"
#include "drape/pointers.hpp"
#include "drape/vulkan/vulkan_gpu_program.hpp"
#include "drape/vulkan/vulkan_object_manager.hpp"
#include "drape/vulkan/vulkan_param_descriptor.hpp"
#include "drape/vulkan/vulkan_pipeline.hpp"
#include "drape/vulkan/vulkan_staging_buffer.hpp"
#include "drape/vulkan/vulkan_texture.hpp"
#include "drape/vulkan/vulkan_utils.hpp"

#include "geometry/point2d.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace dp
{
namespace vulkan
{
class VulkanBaseContext : public dp::GraphicsContext
{
public:
  VulkanBaseContext(VkInstance vulkanInstance, VkPhysicalDevice gpu,
                    VkPhysicalDeviceProperties const & gpuProperties, VkDevice device,
                    uint32_t renderingQueueFamilyIndex, ref_ptr<VulkanObjectManager> objectManager,
                    drape_ptr<VulkanPipeline> && pipeline, bool hasPartialTextureUpdates);
  ~VulkanBaseContext() override;

  using ContextHandler = std::function<void(uint32_t inflightFrameIndex)>;

  bool BeginRendering() override;
  void EndRendering() override;
  void Present() override;
  void CollectMemory() override;
  void DoneCurrent() override {}
  bool Validate() override { return true; }
  void Resize(int w, int h) override;
  void SetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer) override;
  void ForgetFramebuffer(ref_ptr<dp::BaseFramebuffer> framebuffer) override;
  void ApplyFramebuffer(std::string const & framebufferLabel) override;
  void Init(ApiVersion apiVersion) override;
  void SetPresentAvailable(bool available) override;
  ApiVersion GetApiVersion() const override { return dp::ApiVersion::Vulkan; }
  std::string GetRendererName() const override;
  std::string GetRendererVersion() const override;
  bool HasPartialTextureUpdates() const override;

  void DebugSynchronizeWithCPU() override {}
  void PushDebugLabel(std::string const & label) override {}
  void PopDebugLabel() override {}

  void SetClearColor(Color const & color) override;
  void Clear(uint32_t clearBits, uint32_t storeBits) override;
  void Flush() override {}
  void SetViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;
  void SetScissor(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;
  void SetDepthTestEnabled(bool enabled) override;
  void SetDepthTestFunction(TestFunction depthFunction) override;
  void SetStencilTestEnabled(bool enabled) override;
  void SetStencilFunction(StencilFace face, TestFunction stencilFunction) override;
  void SetStencilActions(StencilFace face, StencilAction stencilFailAction,
                         StencilAction depthFailAction, StencilAction passAction) override;
  void SetStencilReferenceValue(uint32_t stencilReferenceValue) override;
  void SetCullingEnabled(bool enabled) override;

  void SetPrimitiveTopology(VkPrimitiveTopology topology);
  void SetBindingInfo(BindingInfoArray const & bindingInfo, uint8_t bindingInfoCount);
  void SetProgram(ref_ptr<VulkanGpuProgram> program);
  void SetBlendingEnabled(bool blendingEnabled);

  void ApplyParamDescriptor(ParamDescriptor && descriptor);
  void ClearParamDescriptors();

  void SetSurface(VkSurfaceKHR surface, VkSurfaceFormatKHR surfaceFormat,
                  VkSurfaceCapabilitiesKHR const & surfaceCapabilities);
  void ResetSurface(bool allowPipelineDump);

  VkPhysicalDevice GetPhysicalDevice() const { return m_gpu; }
  VkDevice GetDevice() const { return m_device; }
  VkQueue GetQueue() const { return m_queue; }

  VkPhysicalDeviceProperties const & GetGpuProperties() const { return m_gpuProperties; }
  uint32_t GetRenderingQueueFamilyIndex() { return m_renderingQueueFamilyIndex; }

  ref_ptr<VulkanObjectManager> GetObjectManager() const { return m_objectManager; }

  // The following methods return short-live objects. Typically they must not be stored.
  VkCommandBuffer GetCurrentMemoryCommandBuffer() const;
  VkCommandBuffer GetCurrentRenderingCommandBuffer() const;
  ref_ptr<VulkanStagingBuffer> GetDefaultStagingBuffer() const;

  uint32_t GetCurrentInflightFrameIndex() const { return m_inflightFrameIndex; }

  VkPipeline GetCurrentPipeline();
  VkPipelineLayout GetCurrentPipelineLayout() const;
  uint32_t GetCurrentDynamicBufferOffset() const;
  std::vector<ParamDescriptor> const & GetCurrentParamDescriptors() const;
  ref_ptr<VulkanGpuProgram> GetCurrentProgram() const { return m_pipelineKey.m_program; }
  uint32_t GetCurrentFrameIndex() const { return m_frameCounter; }

  VkSampler GetSampler(SamplerKey const & key);

  enum class HandlerType : uint8_t
  {
    PrePresent = 0,
    PostPresent,
    UpdateInflightFrame,

    Count
  };
  uint32_t RegisterHandler(HandlerType handlerType, ContextHandler && handler);
  void UnregisterHandler(uint32_t id);

  void ResetPipelineCache();

  struct AttachmentOp
  {
    VkAttachmentLoadOp m_loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp m_storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  };

  struct AttachmentsOperations
  {
    AttachmentOp m_color;
    AttachmentOp m_depth;
    AttachmentOp m_stencil;
  };

protected:
  void SetRenderingQueue(VkQueue queue);

  void RecreateSwapchain();
  void DestroySwapchain();

  void CreateCommandPool();
  void DestroyCommandPool();

  void CreateCommandBuffers();
  void DestroyCommandBuffers();

  void CreateSyncPrimitives();
  void DestroySyncPrimitives();

  void DestroyRenderPassAndFramebuffers();
  void DestroyRenderPassAndFramebuffer(ref_ptr<BaseFramebuffer> framebuffer);

  void RecreateDepthTexture();

  void RecreateSwapchainAndDependencies();
  void ResetSwapchainAndDependencies();

  AttachmentsOperations GetAttachmensOperations();

  VkRenderPass CreateRenderPass(uint32_t attachmentsCount, AttachmentsOperations const & attachmentsOp,
                                VkFormat colorFormat, VkImageLayout initLayout, VkImageLayout finalLayout,
                                VkFormat depthFormat, VkImageLayout depthInitLayout, VkImageLayout depthFinalLayout);

  VkInstance const m_vulkanInstance;
  VkPhysicalDevice const m_gpu;
  VkPhysicalDeviceProperties const m_gpuProperties;
  VkDevice const m_device;
  uint32_t const m_renderingQueueFamilyIndex;
  bool const m_hasPartialTextureUpdates;

  VkQueue m_queue = {};
  VkCommandPool m_commandPool = {};
  bool m_isActiveRenderPass = false;

  std::array<VkCommandBuffer, kMaxInflightFrames> m_renderingCommandBuffers = {};
  std::array<VkCommandBuffer, kMaxInflightFrames> m_memoryCommandBuffers = {};

  // Swap chain image acquiring.
  std::array<VkSemaphore, kMaxInflightFrames> m_acquireSemaphores = {};
  // Command buffers submission and execution.
  std::array<VkSemaphore, kMaxInflightFrames> m_renderSemaphores = {};
  // All rendering tasks completion.
  std::array<VkFence, kMaxInflightFrames> m_fences = {};

  ref_ptr<VulkanObjectManager> m_objectManager;
  drape_ptr<VulkanPipeline> m_pipeline;
  std::optional<VkSurfaceKHR> m_surface;

  VkSurfaceCapabilitiesKHR m_surfaceCapabilities;
  std::optional<VkSurfaceFormatKHR> m_surfaceFormat;

  VkSwapchainKHR m_swapchain = {};
  std::vector<VkImageView> m_swapchainImageViews;
  std::vector<VkImage> m_swapchainImages;
  uint32_t m_imageIndex = 0;

  drape_ptr<VulkanTexture> m_depthTexture;

  uint32_t m_clearBits = 0;
  uint32_t m_storeBits = 0;
  Color m_clearColor;
  uint32_t m_stencilReferenceValue = 1;

  ref_ptr<BaseFramebuffer> m_currentFramebuffer;

  struct FramebufferData
  {
    VkRenderPass m_renderPass = {};
    uint16_t m_packedAttachmentOperations = 0;
    std::vector<VkFramebuffer> m_framebuffers = {};
  };
  std::map<ref_ptr<BaseFramebuffer>, FramebufferData> m_framebuffersData;

  std::array<std::vector<std::pair<uint32_t, ContextHandler>>,
             static_cast<size_t>(HandlerType::Count)> m_handlers;

  VulkanPipeline::PipelineKey m_pipelineKey;
  std::vector<ParamDescriptor> m_paramDescriptors;

  std::array<drape_ptr<VulkanStagingBuffer>, kMaxInflightFrames> m_defaultStagingBuffers = {};
  std::atomic<bool> m_presentAvailable;
  uint32_t m_frameCounter = 0;
  bool m_needPresent = true;
  uint32_t m_inflightFrameIndex = 0;
};
}  // namespace vulkan
}  // namespace dp
