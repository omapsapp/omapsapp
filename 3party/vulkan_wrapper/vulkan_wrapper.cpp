/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// This file is generated.
#ifdef __cplusplus
extern "C" {
#endif

#include "vulkan_wrapper.h"
#include <dlfcn.h>

int InitVulkan(void) {
#if defined(__APPLE__)
    void* libvulkan = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
    if (!libvulkan) {
        libvulkan = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
    }
    if (!libvulkan) {
        libvulkan = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
    }
#else
    void* libvulkan = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!libvulkan) {
        libvulkan = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    }
#endif
    if (!libvulkan) return 0;

    // Vulkan supported, set function addresses
    vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(dlsym(libvulkan, "vkCreateInstance"));
    vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(dlsym(libvulkan, "vkDestroyInstance"));
    vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(dlsym(libvulkan, "vkEnumeratePhysicalDevices"));
    vkGetPhysicalDeviceFeatures =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(dlsym(libvulkan, "vkGetPhysicalDeviceFeatures"));
    vkGetPhysicalDeviceFormatProperties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(dlsym(libvulkan, "vkGetPhysicalDeviceFormatProperties"));
    vkGetPhysicalDeviceImageFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(
        dlsym(libvulkan, "vkGetPhysicalDeviceImageFormatProperties"));
    vkGetPhysicalDeviceProperties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(dlsym(libvulkan, "vkGetPhysicalDeviceProperties"));
    vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
        dlsym(libvulkan, "vkGetPhysicalDeviceQueueFamilyProperties"));
    vkGetPhysicalDeviceMemoryProperties =
        reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(dlsym(libvulkan, "vkGetPhysicalDeviceMemoryProperties"));
    vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(libvulkan, "vkGetInstanceProcAddr"));
    vkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(dlsym(libvulkan, "vkGetDeviceProcAddr"));
    vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(dlsym(libvulkan, "vkCreateDevice"));
    vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(dlsym(libvulkan, "vkDestroyDevice"));
    vkEnumerateInstanceExtensionProperties =
        reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(dlsym(libvulkan, "vkEnumerateInstanceExtensionProperties"));
    vkEnumerateDeviceExtensionProperties =
        reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(dlsym(libvulkan, "vkEnumerateDeviceExtensionProperties"));
    vkEnumerateInstanceLayerProperties =
        reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(dlsym(libvulkan, "vkEnumerateInstanceLayerProperties"));
    vkEnumerateDeviceLayerProperties =
        reinterpret_cast<PFN_vkEnumerateDeviceLayerProperties>(dlsym(libvulkan, "vkEnumerateDeviceLayerProperties"));
    vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(dlsym(libvulkan, "vkGetDeviceQueue"));
    vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(dlsym(libvulkan, "vkQueueSubmit"));
    vkQueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(dlsym(libvulkan, "vkQueueWaitIdle"));
    vkDeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(dlsym(libvulkan, "vkDeviceWaitIdle"));
    vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(dlsym(libvulkan, "vkAllocateMemory"));
    vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(dlsym(libvulkan, "vkFreeMemory"));
    vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(dlsym(libvulkan, "vkMapMemory"));
    vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(dlsym(libvulkan, "vkUnmapMemory"));
    vkFlushMappedMemoryRanges = reinterpret_cast<PFN_vkFlushMappedMemoryRanges>(dlsym(libvulkan, "vkFlushMappedMemoryRanges"));
    vkInvalidateMappedMemoryRanges =
        reinterpret_cast<PFN_vkInvalidateMappedMemoryRanges>(dlsym(libvulkan, "vkInvalidateMappedMemoryRanges"));
    vkGetDeviceMemoryCommitment =
        reinterpret_cast<PFN_vkGetDeviceMemoryCommitment>(dlsym(libvulkan, "vkGetDeviceMemoryCommitment"));
    vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(dlsym(libvulkan, "vkBindBufferMemory"));
    vkBindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(dlsym(libvulkan, "vkBindImageMemory"));
    vkGetBufferMemoryRequirements =
        reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(dlsym(libvulkan, "vkGetBufferMemoryRequirements"));
    vkGetImageMemoryRequirements =
        reinterpret_cast<PFN_vkGetImageMemoryRequirements>(dlsym(libvulkan, "vkGetImageMemoryRequirements"));
    vkGetImageSparseMemoryRequirements =
        reinterpret_cast<PFN_vkGetImageSparseMemoryRequirements>(dlsym(libvulkan, "vkGetImageSparseMemoryRequirements"));
    vkGetPhysicalDeviceSparseImageFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceSparseImageFormatProperties>(
        dlsym(libvulkan, "vkGetPhysicalDeviceSparseImageFormatProperties"));
    vkQueueBindSparse = reinterpret_cast<PFN_vkQueueBindSparse>(dlsym(libvulkan, "vkQueueBindSparse"));
    vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(dlsym(libvulkan, "vkCreateFence"));
    vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(dlsym(libvulkan, "vkDestroyFence"));
    vkResetFences = reinterpret_cast<PFN_vkResetFences>(dlsym(libvulkan, "vkResetFences"));
    vkGetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(dlsym(libvulkan, "vkGetFenceStatus"));
    vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(dlsym(libvulkan, "vkWaitForFences"));
    vkCreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(dlsym(libvulkan, "vkCreateSemaphore"));
    vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(dlsym(libvulkan, "vkDestroySemaphore"));
    vkCreateEvent = reinterpret_cast<PFN_vkCreateEvent>(dlsym(libvulkan, "vkCreateEvent"));
    vkDestroyEvent = reinterpret_cast<PFN_vkDestroyEvent>(dlsym(libvulkan, "vkDestroyEvent"));
    vkGetEventStatus = reinterpret_cast<PFN_vkGetEventStatus>(dlsym(libvulkan, "vkGetEventStatus"));
    vkSetEvent = reinterpret_cast<PFN_vkSetEvent>(dlsym(libvulkan, "vkSetEvent"));
    vkResetEvent = reinterpret_cast<PFN_vkResetEvent>(dlsym(libvulkan, "vkResetEvent"));
    vkCreateQueryPool = reinterpret_cast<PFN_vkCreateQueryPool>(dlsym(libvulkan, "vkCreateQueryPool"));
    vkDestroyQueryPool = reinterpret_cast<PFN_vkDestroyQueryPool>(dlsym(libvulkan, "vkDestroyQueryPool"));
    vkGetQueryPoolResults = reinterpret_cast<PFN_vkGetQueryPoolResults>(dlsym(libvulkan, "vkGetQueryPoolResults"));
    vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(dlsym(libvulkan, "vkCreateBuffer"));
    vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(dlsym(libvulkan, "vkDestroyBuffer"));
    vkCreateBufferView = reinterpret_cast<PFN_vkCreateBufferView>(dlsym(libvulkan, "vkCreateBufferView"));
    vkDestroyBufferView = reinterpret_cast<PFN_vkDestroyBufferView>(dlsym(libvulkan, "vkDestroyBufferView"));
    vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(dlsym(libvulkan, "vkCreateImage"));
    vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(dlsym(libvulkan, "vkDestroyImage"));
    vkGetImageSubresourceLayout =
        reinterpret_cast<PFN_vkGetImageSubresourceLayout>(dlsym(libvulkan, "vkGetImageSubresourceLayout"));
    vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(dlsym(libvulkan, "vkCreateImageView"));
    vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(dlsym(libvulkan, "vkDestroyImageView"));
    vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(dlsym(libvulkan, "vkCreateShaderModule"));
    vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(dlsym(libvulkan, "vkDestroyShaderModule"));
    vkCreatePipelineCache = reinterpret_cast<PFN_vkCreatePipelineCache>(dlsym(libvulkan, "vkCreatePipelineCache"));
    vkDestroyPipelineCache = reinterpret_cast<PFN_vkDestroyPipelineCache>(dlsym(libvulkan, "vkDestroyPipelineCache"));
    vkGetPipelineCacheData = reinterpret_cast<PFN_vkGetPipelineCacheData>(dlsym(libvulkan, "vkGetPipelineCacheData"));
    vkMergePipelineCaches = reinterpret_cast<PFN_vkMergePipelineCaches>(dlsym(libvulkan, "vkMergePipelineCaches"));
    vkCreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(dlsym(libvulkan, "vkCreateGraphicsPipelines"));
    vkCreateComputePipelines = reinterpret_cast<PFN_vkCreateComputePipelines>(dlsym(libvulkan, "vkCreateComputePipelines"));
    vkDestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(dlsym(libvulkan, "vkDestroyPipeline"));
    vkCreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(dlsym(libvulkan, "vkCreatePipelineLayout"));
    vkDestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(dlsym(libvulkan, "vkDestroyPipelineLayout"));
    vkCreateSampler = reinterpret_cast<PFN_vkCreateSampler>(dlsym(libvulkan, "vkCreateSampler"));
    vkDestroySampler = reinterpret_cast<PFN_vkDestroySampler>(dlsym(libvulkan, "vkDestroySampler"));
    vkCreateDescriptorSetLayout =
        reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(dlsym(libvulkan, "vkCreateDescriptorSetLayout"));
    vkDestroyDescriptorSetLayout =
        reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(dlsym(libvulkan, "vkDestroyDescriptorSetLayout"));
    vkCreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(dlsym(libvulkan, "vkCreateDescriptorPool"));
    vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(dlsym(libvulkan, "vkDestroyDescriptorPool"));
    vkResetDescriptorPool = reinterpret_cast<PFN_vkResetDescriptorPool>(dlsym(libvulkan, "vkResetDescriptorPool"));
    vkAllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(dlsym(libvulkan, "vkAllocateDescriptorSets"));
    vkFreeDescriptorSets = reinterpret_cast<PFN_vkFreeDescriptorSets>(dlsym(libvulkan, "vkFreeDescriptorSets"));
    vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(dlsym(libvulkan, "vkUpdateDescriptorSets"));
    vkCreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(dlsym(libvulkan, "vkCreateFramebuffer"));
    vkDestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(dlsym(libvulkan, "vkDestroyFramebuffer"));
    vkCreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(dlsym(libvulkan, "vkCreateRenderPass"));
    vkDestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(dlsym(libvulkan, "vkDestroyRenderPass"));
    vkGetRenderAreaGranularity = reinterpret_cast<PFN_vkGetRenderAreaGranularity>(dlsym(libvulkan, "vkGetRenderAreaGranularity"));
    vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(dlsym(libvulkan, "vkCreateCommandPool"));
    vkDestroyCommandPool = reinterpret_cast<PFN_vkDestroyCommandPool>(dlsym(libvulkan, "vkDestroyCommandPool"));
    vkResetCommandPool = reinterpret_cast<PFN_vkResetCommandPool>(dlsym(libvulkan, "vkResetCommandPool"));
    vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(dlsym(libvulkan, "vkAllocateCommandBuffers"));
    vkFreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(dlsym(libvulkan, "vkFreeCommandBuffers"));
    vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(dlsym(libvulkan, "vkBeginCommandBuffer"));
    vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(dlsym(libvulkan, "vkEndCommandBuffer"));
    vkResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(dlsym(libvulkan, "vkResetCommandBuffer"));
    vkCmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(dlsym(libvulkan, "vkCmdBindPipeline"));
    vkCmdSetViewport = reinterpret_cast<PFN_vkCmdSetViewport>(dlsym(libvulkan, "vkCmdSetViewport"));
    vkCmdSetScissor = reinterpret_cast<PFN_vkCmdSetScissor>(dlsym(libvulkan, "vkCmdSetScissor"));
    vkCmdSetLineWidth = reinterpret_cast<PFN_vkCmdSetLineWidth>(dlsym(libvulkan, "vkCmdSetLineWidth"));
    vkCmdSetDepthBias = reinterpret_cast<PFN_vkCmdSetDepthBias>(dlsym(libvulkan, "vkCmdSetDepthBias"));
    vkCmdSetBlendConstants = reinterpret_cast<PFN_vkCmdSetBlendConstants>(dlsym(libvulkan, "vkCmdSetBlendConstants"));
    vkCmdSetDepthBounds = reinterpret_cast<PFN_vkCmdSetDepthBounds>(dlsym(libvulkan, "vkCmdSetDepthBounds"));
    vkCmdSetStencilCompareMask = reinterpret_cast<PFN_vkCmdSetStencilCompareMask>(dlsym(libvulkan, "vkCmdSetStencilCompareMask"));
    vkCmdSetStencilWriteMask = reinterpret_cast<PFN_vkCmdSetStencilWriteMask>(dlsym(libvulkan, "vkCmdSetStencilWriteMask"));
    vkCmdSetStencilReference = reinterpret_cast<PFN_vkCmdSetStencilReference>(dlsym(libvulkan, "vkCmdSetStencilReference"));
    vkCmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(dlsym(libvulkan, "vkCmdBindDescriptorSets"));
    vkCmdBindIndexBuffer = reinterpret_cast<PFN_vkCmdBindIndexBuffer>(dlsym(libvulkan, "vkCmdBindIndexBuffer"));
    vkCmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(dlsym(libvulkan, "vkCmdBindVertexBuffers"));
    vkCmdDraw = reinterpret_cast<PFN_vkCmdDraw>(dlsym(libvulkan, "vkCmdDraw"));
    vkCmdDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(dlsym(libvulkan, "vkCmdDrawIndexed"));
    vkCmdDrawIndirect = reinterpret_cast<PFN_vkCmdDrawIndirect>(dlsym(libvulkan, "vkCmdDrawIndirect"));
    vkCmdDrawIndexedIndirect = reinterpret_cast<PFN_vkCmdDrawIndexedIndirect>(dlsym(libvulkan, "vkCmdDrawIndexedIndirect"));
    vkCmdDispatch = reinterpret_cast<PFN_vkCmdDispatch>(dlsym(libvulkan, "vkCmdDispatch"));
    vkCmdDispatchIndirect = reinterpret_cast<PFN_vkCmdDispatchIndirect>(dlsym(libvulkan, "vkCmdDispatchIndirect"));
    vkCmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(dlsym(libvulkan, "vkCmdCopyBuffer"));
    vkCmdCopyImage = reinterpret_cast<PFN_vkCmdCopyImage>(dlsym(libvulkan, "vkCmdCopyImage"));
    vkCmdBlitImage = reinterpret_cast<PFN_vkCmdBlitImage>(dlsym(libvulkan, "vkCmdBlitImage"));
    vkCmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(dlsym(libvulkan, "vkCmdCopyBufferToImage"));
    vkCmdCopyImageToBuffer = reinterpret_cast<PFN_vkCmdCopyImageToBuffer>(dlsym(libvulkan, "vkCmdCopyImageToBuffer"));
    vkCmdUpdateBuffer = reinterpret_cast<PFN_vkCmdUpdateBuffer>(dlsym(libvulkan, "vkCmdUpdateBuffer"));
    vkCmdFillBuffer = reinterpret_cast<PFN_vkCmdFillBuffer>(dlsym(libvulkan, "vkCmdFillBuffer"));
    vkCmdClearColorImage = reinterpret_cast<PFN_vkCmdClearColorImage>(dlsym(libvulkan, "vkCmdClearColorImage"));
    vkCmdClearDepthStencilImage =
        reinterpret_cast<PFN_vkCmdClearDepthStencilImage>(dlsym(libvulkan, "vkCmdClearDepthStencilImage"));
    vkCmdClearAttachments = reinterpret_cast<PFN_vkCmdClearAttachments>(dlsym(libvulkan, "vkCmdClearAttachments"));
    vkCmdResolveImage = reinterpret_cast<PFN_vkCmdResolveImage>(dlsym(libvulkan, "vkCmdResolveImage"));
    vkCmdSetEvent = reinterpret_cast<PFN_vkCmdSetEvent>(dlsym(libvulkan, "vkCmdSetEvent"));
    vkCmdResetEvent = reinterpret_cast<PFN_vkCmdResetEvent>(dlsym(libvulkan, "vkCmdResetEvent"));
    vkCmdWaitEvents = reinterpret_cast<PFN_vkCmdWaitEvents>(dlsym(libvulkan, "vkCmdWaitEvents"));
    vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(dlsym(libvulkan, "vkCmdPipelineBarrier"));
    vkCmdBeginQuery = reinterpret_cast<PFN_vkCmdBeginQuery>(dlsym(libvulkan, "vkCmdBeginQuery"));
    vkCmdEndQuery = reinterpret_cast<PFN_vkCmdEndQuery>(dlsym(libvulkan, "vkCmdEndQuery"));
    vkCmdResetQueryPool = reinterpret_cast<PFN_vkCmdResetQueryPool>(dlsym(libvulkan, "vkCmdResetQueryPool"));
    vkCmdWriteTimestamp = reinterpret_cast<PFN_vkCmdWriteTimestamp>(dlsym(libvulkan, "vkCmdWriteTimestamp"));
    vkCmdCopyQueryPoolResults = reinterpret_cast<PFN_vkCmdCopyQueryPoolResults>(dlsym(libvulkan, "vkCmdCopyQueryPoolResults"));
    vkCmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(dlsym(libvulkan, "vkCmdPushConstants"));
    vkCmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(dlsym(libvulkan, "vkCmdBeginRenderPass"));
    vkCmdNextSubpass = reinterpret_cast<PFN_vkCmdNextSubpass>(dlsym(libvulkan, "vkCmdNextSubpass"));
    vkCmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(dlsym(libvulkan, "vkCmdEndRenderPass"));
    vkCmdExecuteCommands = reinterpret_cast<PFN_vkCmdExecuteCommands>(dlsym(libvulkan, "vkCmdExecuteCommands"));
    vkEnumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(dlsym(libvulkan, "vkEnumerateInstanceVersion"));
    vkBindBufferMemory2 = reinterpret_cast<PFN_vkBindBufferMemory2>(dlsym(libvulkan, "vkBindBufferMemory2"));
    vkBindImageMemory2 = reinterpret_cast<PFN_vkBindImageMemory2>(dlsym(libvulkan, "vkBindImageMemory2"));
    vkGetDeviceGroupPeerMemoryFeatures =
        reinterpret_cast<PFN_vkGetDeviceGroupPeerMemoryFeatures>(dlsym(libvulkan, "vkGetDeviceGroupPeerMemoryFeatures"));
    vkCmdSetDeviceMask = reinterpret_cast<PFN_vkCmdSetDeviceMask>(dlsym(libvulkan, "vkCmdSetDeviceMask"));
    vkCmdDispatchBase = reinterpret_cast<PFN_vkCmdDispatchBase>(dlsym(libvulkan, "vkCmdDispatchBase"));
    vkEnumeratePhysicalDeviceGroups =
        reinterpret_cast<PFN_vkEnumeratePhysicalDeviceGroups>(dlsym(libvulkan, "vkEnumeratePhysicalDeviceGroups"));
    vkGetImageMemoryRequirements2 =
        reinterpret_cast<PFN_vkGetImageMemoryRequirements2>(dlsym(libvulkan, "vkGetImageMemoryRequirements2"));
    vkGetBufferMemoryRequirements2 =
        reinterpret_cast<PFN_vkGetBufferMemoryRequirements2>(dlsym(libvulkan, "vkGetBufferMemoryRequirements2"));
    vkGetImageSparseMemoryRequirements2 =
        reinterpret_cast<PFN_vkGetImageSparseMemoryRequirements2>(dlsym(libvulkan, "vkGetImageSparseMemoryRequirements2"));
    vkGetPhysicalDeviceFeatures2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(dlsym(libvulkan, "vkGetPhysicalDeviceFeatures2"));
    vkGetPhysicalDeviceProperties2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(dlsym(libvulkan, "vkGetPhysicalDeviceProperties2"));
    vkGetPhysicalDeviceFormatProperties2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties2>(dlsym(libvulkan, "vkGetPhysicalDeviceFormatProperties2"));
    vkGetPhysicalDeviceImageFormatProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
        dlsym(libvulkan, "vkGetPhysicalDeviceImageFormatProperties2"));
    vkGetPhysicalDeviceQueueFamilyProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties2>(
        dlsym(libvulkan, "vkGetPhysicalDeviceQueueFamilyProperties2"));
    vkGetPhysicalDeviceMemoryProperties2 =
        reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2>(dlsym(libvulkan, "vkGetPhysicalDeviceMemoryProperties2"));
    vkGetPhysicalDeviceSparseImageFormatProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceSparseImageFormatProperties2>(
        dlsym(libvulkan, "vkGetPhysicalDeviceSparseImageFormatProperties2"));
    vkTrimCommandPool = reinterpret_cast<PFN_vkTrimCommandPool>(dlsym(libvulkan, "vkTrimCommandPool"));
    vkGetDeviceQueue2 = reinterpret_cast<PFN_vkGetDeviceQueue2>(dlsym(libvulkan, "vkGetDeviceQueue2"));
    vkCreateSamplerYcbcrConversion =
        reinterpret_cast<PFN_vkCreateSamplerYcbcrConversion>(dlsym(libvulkan, "vkCreateSamplerYcbcrConversion"));
    vkDestroySamplerYcbcrConversion =
        reinterpret_cast<PFN_vkDestroySamplerYcbcrConversion>(dlsym(libvulkan, "vkDestroySamplerYcbcrConversion"));
    vkCreateDescriptorUpdateTemplate =
        reinterpret_cast<PFN_vkCreateDescriptorUpdateTemplate>(dlsym(libvulkan, "vkCreateDescriptorUpdateTemplate"));
    vkDestroyDescriptorUpdateTemplate =
        reinterpret_cast<PFN_vkDestroyDescriptorUpdateTemplate>(dlsym(libvulkan, "vkDestroyDescriptorUpdateTemplate"));
    vkUpdateDescriptorSetWithTemplate =
        reinterpret_cast<PFN_vkUpdateDescriptorSetWithTemplate>(dlsym(libvulkan, "vkUpdateDescriptorSetWithTemplate"));
    vkGetPhysicalDeviceExternalBufferProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceExternalBufferProperties>(
        dlsym(libvulkan, "vkGetPhysicalDeviceExternalBufferProperties"));
    vkGetPhysicalDeviceExternalFenceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceExternalFenceProperties>(
        dlsym(libvulkan, "vkGetPhysicalDeviceExternalFenceProperties"));
    vkGetPhysicalDeviceExternalSemaphoreProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceExternalSemaphoreProperties>(
        dlsym(libvulkan, "vkGetPhysicalDeviceExternalSemaphoreProperties"));
    vkGetDescriptorSetLayoutSupport =
        reinterpret_cast<PFN_vkGetDescriptorSetLayoutSupport>(dlsym(libvulkan, "vkGetDescriptorSetLayoutSupport"));
    vkCmdDrawIndirectCount = reinterpret_cast<PFN_vkCmdDrawIndirectCount>(dlsym(libvulkan, "vkCmdDrawIndirectCount"));
    vkCmdDrawIndexedIndirectCount =
        reinterpret_cast<PFN_vkCmdDrawIndexedIndirectCount>(dlsym(libvulkan, "vkCmdDrawIndexedIndirectCount"));
    vkCreateRenderPass2 = reinterpret_cast<PFN_vkCreateRenderPass2>(dlsym(libvulkan, "vkCreateRenderPass2"));
    vkCmdBeginRenderPass2 = reinterpret_cast<PFN_vkCmdBeginRenderPass2>(dlsym(libvulkan, "vkCmdBeginRenderPass2"));
    vkCmdNextSubpass2 = reinterpret_cast<PFN_vkCmdNextSubpass2>(dlsym(libvulkan, "vkCmdNextSubpass2"));
    vkCmdEndRenderPass2 = reinterpret_cast<PFN_vkCmdEndRenderPass2>(dlsym(libvulkan, "vkCmdEndRenderPass2"));
    vkResetQueryPool = reinterpret_cast<PFN_vkResetQueryPool>(dlsym(libvulkan, "vkResetQueryPool"));
    vkGetSemaphoreCounterValue = reinterpret_cast<PFN_vkGetSemaphoreCounterValue>(dlsym(libvulkan, "vkGetSemaphoreCounterValue"));
    vkWaitSemaphores = reinterpret_cast<PFN_vkWaitSemaphores>(dlsym(libvulkan, "vkWaitSemaphores"));
    vkSignalSemaphore = reinterpret_cast<PFN_vkSignalSemaphore>(dlsym(libvulkan, "vkSignalSemaphore"));
    vkGetBufferDeviceAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(dlsym(libvulkan, "vkGetBufferDeviceAddress"));
    vkGetBufferOpaqueCaptureAddress =
        reinterpret_cast<PFN_vkGetBufferOpaqueCaptureAddress>(dlsym(libvulkan, "vkGetBufferOpaqueCaptureAddress"));
    vkGetDeviceMemoryOpaqueCaptureAddress =
        reinterpret_cast<PFN_vkGetDeviceMemoryOpaqueCaptureAddress>(dlsym(libvulkan, "vkGetDeviceMemoryOpaqueCaptureAddress"));
    vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(dlsym(libvulkan, "vkDestroySurfaceKHR"));
    vkGetPhysicalDeviceSurfaceSupportKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(dlsym(libvulkan, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    vkGetPhysicalDeviceSurfaceFormatsKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(dlsym(libvulkan, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    vkGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
    vkCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(dlsym(libvulkan, "vkCreateSwapchainKHR"));
    vkDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(dlsym(libvulkan, "vkDestroySwapchainKHR"));
    vkGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(dlsym(libvulkan, "vkGetSwapchainImagesKHR"));
    vkAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(dlsym(libvulkan, "vkAcquireNextImageKHR"));
    vkQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(dlsym(libvulkan, "vkQueuePresentKHR"));
    vkGetDeviceGroupPresentCapabilitiesKHR =
        reinterpret_cast<PFN_vkGetDeviceGroupPresentCapabilitiesKHR>(dlsym(libvulkan, "vkGetDeviceGroupPresentCapabilitiesKHR"));
    vkGetDeviceGroupSurfacePresentModesKHR =
        reinterpret_cast<PFN_vkGetDeviceGroupSurfacePresentModesKHR>(dlsym(libvulkan, "vkGetDeviceGroupSurfacePresentModesKHR"));
    vkGetPhysicalDevicePresentRectanglesKHR =
        reinterpret_cast<PFN_vkGetPhysicalDevicePresentRectanglesKHR>(dlsym(libvulkan, "vkGetPhysicalDevicePresentRectanglesKHR"));
    vkAcquireNextImage2KHR = reinterpret_cast<PFN_vkAcquireNextImage2KHR>(dlsym(libvulkan, "vkAcquireNextImage2KHR"));
    vkGetPhysicalDeviceDisplayPropertiesKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceDisplayPropertiesKHR>(dlsym(libvulkan, "vkGetPhysicalDeviceDisplayPropertiesKHR"));
    vkGetPhysicalDeviceDisplayPlanePropertiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceDisplayPlanePropertiesKHR"));
    vkGetDisplayPlaneSupportedDisplaysKHR =
        reinterpret_cast<PFN_vkGetDisplayPlaneSupportedDisplaysKHR>(dlsym(libvulkan, "vkGetDisplayPlaneSupportedDisplaysKHR"));
    vkGetDisplayModePropertiesKHR =
        reinterpret_cast<PFN_vkGetDisplayModePropertiesKHR>(dlsym(libvulkan, "vkGetDisplayModePropertiesKHR"));
    vkCreateDisplayModeKHR = reinterpret_cast<PFN_vkCreateDisplayModeKHR>(dlsym(libvulkan, "vkCreateDisplayModeKHR"));
    vkGetDisplayPlaneCapabilitiesKHR =
        reinterpret_cast<PFN_vkGetDisplayPlaneCapabilitiesKHR>(dlsym(libvulkan, "vkGetDisplayPlaneCapabilitiesKHR"));
    vkCreateDisplayPlaneSurfaceKHR =
        reinterpret_cast<PFN_vkCreateDisplayPlaneSurfaceKHR>(dlsym(libvulkan, "vkCreateDisplayPlaneSurfaceKHR"));
    vkCreateSharedSwapchainsKHR =
        reinterpret_cast<PFN_vkCreateSharedSwapchainsKHR>(dlsym(libvulkan, "vkCreateSharedSwapchainsKHR"));
    vkCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(dlsym(libvulkan, "vkCmdBeginRenderingKHR"));
    vkCmdEndRenderingKHR = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(dlsym(libvulkan, "vkCmdEndRenderingKHR"));
    vkGetPhysicalDeviceFeatures2KHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(dlsym(libvulkan, "vkGetPhysicalDeviceFeatures2KHR"));
    vkGetPhysicalDeviceProperties2KHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2KHR>(dlsym(libvulkan, "vkGetPhysicalDeviceProperties2KHR"));
    vkGetPhysicalDeviceFormatProperties2KHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties2KHR>(dlsym(libvulkan, "vkGetPhysicalDeviceFormatProperties2KHR"));
    vkGetPhysicalDeviceImageFormatProperties2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2KHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceImageFormatProperties2KHR"));
    vkGetPhysicalDeviceQueueFamilyProperties2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceQueueFamilyProperties2KHR"));
    vkGetPhysicalDeviceMemoryProperties2KHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2KHR>(dlsym(libvulkan, "vkGetPhysicalDeviceMemoryProperties2KHR"));
    vkGetPhysicalDeviceSparseImageFormatProperties2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceSparseImageFormatProperties2KHR"));
    vkGetDeviceGroupPeerMemoryFeaturesKHR =
        reinterpret_cast<PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR>(dlsym(libvulkan, "vkGetDeviceGroupPeerMemoryFeaturesKHR"));
    vkCmdSetDeviceMaskKHR = reinterpret_cast<PFN_vkCmdSetDeviceMaskKHR>(dlsym(libvulkan, "vkCmdSetDeviceMaskKHR"));
    vkCmdDispatchBaseKHR = reinterpret_cast<PFN_vkCmdDispatchBaseKHR>(dlsym(libvulkan, "vkCmdDispatchBaseKHR"));
    vkTrimCommandPoolKHR = reinterpret_cast<PFN_vkTrimCommandPoolKHR>(dlsym(libvulkan, "vkTrimCommandPoolKHR"));
    vkEnumeratePhysicalDeviceGroupsKHR =
        reinterpret_cast<PFN_vkEnumeratePhysicalDeviceGroupsKHR>(dlsym(libvulkan, "vkEnumeratePhysicalDeviceGroupsKHR"));
    vkGetPhysicalDeviceExternalBufferPropertiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceExternalBufferPropertiesKHR"));
    vkGetMemoryFdKHR = reinterpret_cast<PFN_vkGetMemoryFdKHR>(dlsym(libvulkan, "vkGetMemoryFdKHR"));
    vkGetMemoryFdPropertiesKHR = reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(dlsym(libvulkan, "vkGetMemoryFdPropertiesKHR"));
    vkGetPhysicalDeviceExternalSemaphorePropertiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceExternalSemaphorePropertiesKHR"));
    vkImportSemaphoreFdKHR = reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(dlsym(libvulkan, "vkImportSemaphoreFdKHR"));
    vkGetSemaphoreFdKHR = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(dlsym(libvulkan, "vkGetSemaphoreFdKHR"));
    vkCmdPushDescriptorSetKHR = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(dlsym(libvulkan, "vkCmdPushDescriptorSetKHR"));
    vkCmdPushDescriptorSetWithTemplateKHR =
        reinterpret_cast<PFN_vkCmdPushDescriptorSetWithTemplateKHR>(dlsym(libvulkan, "vkCmdPushDescriptorSetWithTemplateKHR"));
    vkCreateDescriptorUpdateTemplateKHR =
        reinterpret_cast<PFN_vkCreateDescriptorUpdateTemplateKHR>(dlsym(libvulkan, "vkCreateDescriptorUpdateTemplateKHR"));
    vkDestroyDescriptorUpdateTemplateKHR =
        reinterpret_cast<PFN_vkDestroyDescriptorUpdateTemplateKHR>(dlsym(libvulkan, "vkDestroyDescriptorUpdateTemplateKHR"));
    vkUpdateDescriptorSetWithTemplateKHR =
        reinterpret_cast<PFN_vkUpdateDescriptorSetWithTemplateKHR>(dlsym(libvulkan, "vkUpdateDescriptorSetWithTemplateKHR"));
    vkCreateRenderPass2KHR = reinterpret_cast<PFN_vkCreateRenderPass2KHR>(dlsym(libvulkan, "vkCreateRenderPass2KHR"));
    vkCmdBeginRenderPass2KHR = reinterpret_cast<PFN_vkCmdBeginRenderPass2KHR>(dlsym(libvulkan, "vkCmdBeginRenderPass2KHR"));
    vkCmdNextSubpass2KHR = reinterpret_cast<PFN_vkCmdNextSubpass2KHR>(dlsym(libvulkan, "vkCmdNextSubpass2KHR"));
    vkCmdEndRenderPass2KHR = reinterpret_cast<PFN_vkCmdEndRenderPass2KHR>(dlsym(libvulkan, "vkCmdEndRenderPass2KHR"));
    vkGetSwapchainStatusKHR = reinterpret_cast<PFN_vkGetSwapchainStatusKHR>(dlsym(libvulkan, "vkGetSwapchainStatusKHR"));
    vkGetPhysicalDeviceExternalFencePropertiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceExternalFencePropertiesKHR"));
    vkImportFenceFdKHR = reinterpret_cast<PFN_vkImportFenceFdKHR>(dlsym(libvulkan, "vkImportFenceFdKHR"));
    vkGetFenceFdKHR = reinterpret_cast<PFN_vkGetFenceFdKHR>(dlsym(libvulkan, "vkGetFenceFdKHR"));
    vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR =
        reinterpret_cast<PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR>(
            dlsym(libvulkan, "vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR"));
    vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR>(
            dlsym(libvulkan, "vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR"));
    vkAcquireProfilingLockKHR = reinterpret_cast<PFN_vkAcquireProfilingLockKHR>(dlsym(libvulkan, "vkAcquireProfilingLockKHR"));
    vkReleaseProfilingLockKHR = reinterpret_cast<PFN_vkReleaseProfilingLockKHR>(dlsym(libvulkan, "vkReleaseProfilingLockKHR"));
    vkGetPhysicalDeviceSurfaceCapabilities2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceSurfaceCapabilities2KHR"));
    vkGetPhysicalDeviceSurfaceFormats2KHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormats2KHR>(dlsym(libvulkan, "vkGetPhysicalDeviceSurfaceFormats2KHR"));
    vkGetPhysicalDeviceDisplayProperties2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceDisplayProperties2KHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceDisplayProperties2KHR"));
    vkGetPhysicalDeviceDisplayPlaneProperties2KHR = reinterpret_cast<PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceDisplayPlaneProperties2KHR"));
    vkGetDisplayModeProperties2KHR =
        reinterpret_cast<PFN_vkGetDisplayModeProperties2KHR>(dlsym(libvulkan, "vkGetDisplayModeProperties2KHR"));
    vkGetDisplayPlaneCapabilities2KHR =
        reinterpret_cast<PFN_vkGetDisplayPlaneCapabilities2KHR>(dlsym(libvulkan, "vkGetDisplayPlaneCapabilities2KHR"));
    vkGetImageMemoryRequirements2KHR =
        reinterpret_cast<PFN_vkGetImageMemoryRequirements2KHR>(dlsym(libvulkan, "vkGetImageMemoryRequirements2KHR"));
    vkGetBufferMemoryRequirements2KHR =
        reinterpret_cast<PFN_vkGetBufferMemoryRequirements2KHR>(dlsym(libvulkan, "vkGetBufferMemoryRequirements2KHR"));
    vkGetImageSparseMemoryRequirements2KHR =
        reinterpret_cast<PFN_vkGetImageSparseMemoryRequirements2KHR>(dlsym(libvulkan, "vkGetImageSparseMemoryRequirements2KHR"));
    vkCreateSamplerYcbcrConversionKHR =
        reinterpret_cast<PFN_vkCreateSamplerYcbcrConversionKHR>(dlsym(libvulkan, "vkCreateSamplerYcbcrConversionKHR"));
    vkDestroySamplerYcbcrConversionKHR =
        reinterpret_cast<PFN_vkDestroySamplerYcbcrConversionKHR>(dlsym(libvulkan, "vkDestroySamplerYcbcrConversionKHR"));
    vkBindBufferMemory2KHR = reinterpret_cast<PFN_vkBindBufferMemory2KHR>(dlsym(libvulkan, "vkBindBufferMemory2KHR"));
    vkBindImageMemory2KHR = reinterpret_cast<PFN_vkBindImageMemory2KHR>(dlsym(libvulkan, "vkBindImageMemory2KHR"));
    vkGetDescriptorSetLayoutSupportKHR =
        reinterpret_cast<PFN_vkGetDescriptorSetLayoutSupportKHR>(dlsym(libvulkan, "vkGetDescriptorSetLayoutSupportKHR"));
    vkCmdDrawIndirectCountKHR = reinterpret_cast<PFN_vkCmdDrawIndirectCountKHR>(dlsym(libvulkan, "vkCmdDrawIndirectCountKHR"));
    vkCmdDrawIndexedIndirectCountKHR =
        reinterpret_cast<PFN_vkCmdDrawIndexedIndirectCountKHR>(dlsym(libvulkan, "vkCmdDrawIndexedIndirectCountKHR"));
    vkGetSemaphoreCounterValueKHR =
        reinterpret_cast<PFN_vkGetSemaphoreCounterValueKHR>(dlsym(libvulkan, "vkGetSemaphoreCounterValueKHR"));
    vkWaitSemaphoresKHR = reinterpret_cast<PFN_vkWaitSemaphoresKHR>(dlsym(libvulkan, "vkWaitSemaphoresKHR"));
    vkSignalSemaphoreKHR = reinterpret_cast<PFN_vkSignalSemaphoreKHR>(dlsym(libvulkan, "vkSignalSemaphoreKHR"));
    vkGetPhysicalDeviceFragmentShadingRatesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceFragmentShadingRatesKHR"));
    vkCmdSetFragmentShadingRateKHR =
        reinterpret_cast<PFN_vkCmdSetFragmentShadingRateKHR>(dlsym(libvulkan, "vkCmdSetFragmentShadingRateKHR"));
    vkWaitForPresentKHR = reinterpret_cast<PFN_vkWaitForPresentKHR>(dlsym(libvulkan, "vkWaitForPresentKHR"));
    vkGetBufferDeviceAddressKHR =
        reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(dlsym(libvulkan, "vkGetBufferDeviceAddressKHR"));
    vkGetBufferOpaqueCaptureAddressKHR =
        reinterpret_cast<PFN_vkGetBufferOpaqueCaptureAddressKHR>(dlsym(libvulkan, "vkGetBufferOpaqueCaptureAddressKHR"));
    vkGetDeviceMemoryOpaqueCaptureAddressKHR = reinterpret_cast<PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR>(
        dlsym(libvulkan, "vkGetDeviceMemoryOpaqueCaptureAddressKHR"));
    vkCreateDeferredOperationKHR =
        reinterpret_cast<PFN_vkCreateDeferredOperationKHR>(dlsym(libvulkan, "vkCreateDeferredOperationKHR"));
    vkDestroyDeferredOperationKHR =
        reinterpret_cast<PFN_vkDestroyDeferredOperationKHR>(dlsym(libvulkan, "vkDestroyDeferredOperationKHR"));
    vkGetDeferredOperationMaxConcurrencyKHR =
        reinterpret_cast<PFN_vkGetDeferredOperationMaxConcurrencyKHR>(dlsym(libvulkan, "vkGetDeferredOperationMaxConcurrencyKHR"));
    vkGetDeferredOperationResultKHR =
        reinterpret_cast<PFN_vkGetDeferredOperationResultKHR>(dlsym(libvulkan, "vkGetDeferredOperationResultKHR"));
    vkDeferredOperationJoinKHR = reinterpret_cast<PFN_vkDeferredOperationJoinKHR>(dlsym(libvulkan, "vkDeferredOperationJoinKHR"));
    vkGetPipelineExecutablePropertiesKHR =
        reinterpret_cast<PFN_vkGetPipelineExecutablePropertiesKHR>(dlsym(libvulkan, "vkGetPipelineExecutablePropertiesKHR"));
    vkGetPipelineExecutableStatisticsKHR =
        reinterpret_cast<PFN_vkGetPipelineExecutableStatisticsKHR>(dlsym(libvulkan, "vkGetPipelineExecutableStatisticsKHR"));
    vkGetPipelineExecutableInternalRepresentationsKHR = reinterpret_cast<PFN_vkGetPipelineExecutableInternalRepresentationsKHR>(
        dlsym(libvulkan, "vkGetPipelineExecutableInternalRepresentationsKHR"));
    vkCmdSetEvent2KHR = reinterpret_cast<PFN_vkCmdSetEvent2KHR>(dlsym(libvulkan, "vkCmdSetEvent2KHR"));
    vkCmdResetEvent2KHR = reinterpret_cast<PFN_vkCmdResetEvent2KHR>(dlsym(libvulkan, "vkCmdResetEvent2KHR"));
    vkCmdWaitEvents2KHR = reinterpret_cast<PFN_vkCmdWaitEvents2KHR>(dlsym(libvulkan, "vkCmdWaitEvents2KHR"));
    vkCmdPipelineBarrier2KHR = reinterpret_cast<PFN_vkCmdPipelineBarrier2KHR>(dlsym(libvulkan, "vkCmdPipelineBarrier2KHR"));
    vkCmdWriteTimestamp2KHR = reinterpret_cast<PFN_vkCmdWriteTimestamp2KHR>(dlsym(libvulkan, "vkCmdWriteTimestamp2KHR"));
    vkQueueSubmit2KHR = reinterpret_cast<PFN_vkQueueSubmit2KHR>(dlsym(libvulkan, "vkQueueSubmit2KHR"));
    vkCmdWriteBufferMarker2AMD = reinterpret_cast<PFN_vkCmdWriteBufferMarker2AMD>(dlsym(libvulkan, "vkCmdWriteBufferMarker2AMD"));
    vkGetQueueCheckpointData2NV =
        reinterpret_cast<PFN_vkGetQueueCheckpointData2NV>(dlsym(libvulkan, "vkGetQueueCheckpointData2NV"));
    vkCmdCopyBuffer2KHR = reinterpret_cast<PFN_vkCmdCopyBuffer2KHR>(dlsym(libvulkan, "vkCmdCopyBuffer2KHR"));
    vkCmdCopyImage2KHR = reinterpret_cast<PFN_vkCmdCopyImage2KHR>(dlsym(libvulkan, "vkCmdCopyImage2KHR"));
    vkCmdCopyBufferToImage2KHR = reinterpret_cast<PFN_vkCmdCopyBufferToImage2KHR>(dlsym(libvulkan, "vkCmdCopyBufferToImage2KHR"));
    vkCmdCopyImageToBuffer2KHR = reinterpret_cast<PFN_vkCmdCopyImageToBuffer2KHR>(dlsym(libvulkan, "vkCmdCopyImageToBuffer2KHR"));
    vkCmdBlitImage2KHR = reinterpret_cast<PFN_vkCmdBlitImage2KHR>(dlsym(libvulkan, "vkCmdBlitImage2KHR"));
    vkCmdResolveImage2KHR = reinterpret_cast<PFN_vkCmdResolveImage2KHR>(dlsym(libvulkan, "vkCmdResolveImage2KHR"));
    vkGetDeviceBufferMemoryRequirementsKHR =
        reinterpret_cast<PFN_vkGetDeviceBufferMemoryRequirementsKHR>(dlsym(libvulkan, "vkGetDeviceBufferMemoryRequirementsKHR"));
    vkGetDeviceImageMemoryRequirementsKHR =
        reinterpret_cast<PFN_vkGetDeviceImageMemoryRequirementsKHR>(dlsym(libvulkan, "vkGetDeviceImageMemoryRequirementsKHR"));
    vkGetDeviceImageSparseMemoryRequirementsKHR = reinterpret_cast<PFN_vkGetDeviceImageSparseMemoryRequirementsKHR>(
        dlsym(libvulkan, "vkGetDeviceImageSparseMemoryRequirementsKHR"));
    vkCreateAccelerationStructureKHR =
        reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(dlsym(libvulkan, "vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructureKHR =
        reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(dlsym(libvulkan, "vkDestroyAccelerationStructureKHR"));
    vkCmdBuildAccelerationStructuresKHR =
        reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(dlsym(libvulkan, "vkCmdBuildAccelerationStructuresKHR"));
    vkCmdBuildAccelerationStructuresIndirectKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresIndirectKHR>(
        dlsym(libvulkan, "vkCmdBuildAccelerationStructuresIndirectKHR"));
    vkBuildAccelerationStructuresKHR =
        reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(dlsym(libvulkan, "vkBuildAccelerationStructuresKHR"));
    vkCopyAccelerationStructureKHR =
        reinterpret_cast<PFN_vkCopyAccelerationStructureKHR>(dlsym(libvulkan, "vkCopyAccelerationStructureKHR"));
    vkCopyAccelerationStructureToMemoryKHR =
        reinterpret_cast<PFN_vkCopyAccelerationStructureToMemoryKHR>(dlsym(libvulkan, "vkCopyAccelerationStructureToMemoryKHR"));
    vkCopyMemoryToAccelerationStructureKHR =
        reinterpret_cast<PFN_vkCopyMemoryToAccelerationStructureKHR>(dlsym(libvulkan, "vkCopyMemoryToAccelerationStructureKHR"));
    vkWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkWriteAccelerationStructuresPropertiesKHR>(
        dlsym(libvulkan, "vkWriteAccelerationStructuresPropertiesKHR"));
    vkCmdCopyAccelerationStructureKHR =
        reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(dlsym(libvulkan, "vkCmdCopyAccelerationStructureKHR"));
    vkCmdCopyAccelerationStructureToMemoryKHR = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureToMemoryKHR>(
        dlsym(libvulkan, "vkCmdCopyAccelerationStructureToMemoryKHR"));
    vkCmdCopyMemoryToAccelerationStructureKHR = reinterpret_cast<PFN_vkCmdCopyMemoryToAccelerationStructureKHR>(
        dlsym(libvulkan, "vkCmdCopyMemoryToAccelerationStructureKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        dlsym(libvulkan, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCmdWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(
        dlsym(libvulkan, "vkCmdWriteAccelerationStructuresPropertiesKHR"));
    vkGetDeviceAccelerationStructureCompatibilityKHR = reinterpret_cast<PFN_vkGetDeviceAccelerationStructureCompatibilityKHR>(
        dlsym(libvulkan, "vkGetDeviceAccelerationStructureCompatibilityKHR"));
    vkGetAccelerationStructureBuildSizesKHR =
        reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(dlsym(libvulkan, "vkGetAccelerationStructureBuildSizesKHR"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(dlsym(libvulkan, "vkCmdTraceRaysKHR"));
    vkCreateRayTracingPipelinesKHR =
        reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(dlsym(libvulkan, "vkCreateRayTracingPipelinesKHR"));
    vkGetRayTracingCaptureReplayShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR>(
        dlsym(libvulkan, "vkGetRayTracingCaptureReplayShaderGroupHandlesKHR"));
    vkCmdTraceRaysIndirectKHR = reinterpret_cast<PFN_vkCmdTraceRaysIndirectKHR>(dlsym(libvulkan, "vkCmdTraceRaysIndirectKHR"));
    vkGetRayTracingShaderGroupStackSizeKHR =
        reinterpret_cast<PFN_vkGetRayTracingShaderGroupStackSizeKHR>(dlsym(libvulkan, "vkGetRayTracingShaderGroupStackSizeKHR"));
    vkCmdSetRayTracingPipelineStackSizeKHR =
        reinterpret_cast<PFN_vkCmdSetRayTracingPipelineStackSizeKHR>(dlsym(libvulkan, "vkCmdSetRayTracingPipelineStackSizeKHR"));

#ifdef VK_USE_PLATFORM_ANDROID_KHR
    vkCreateAndroidSurfaceKHR = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(dlsym(libvulkan, "vkCreateAndroidSurfaceKHR"));
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
    vkCreateWaylandSurfaceKHR = reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(dlsym(libvulkan, "vkCreateWaylandSurfaceKHR"));
    vkGetPhysicalDeviceWaylandPresentationSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceWaylandPresentationSupportKHR"));
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
    vkCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(dlsym(libvulkan, "vkCreateWin32SurfaceKHR"));
    vkGetPhysicalDeviceWin32PresentationSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceWin32PresentationSupportKHR"));
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
    vkGetMemoryWin32HandleKHR = reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(dlsym(libvulkan, "vkGetMemoryWin32HandleKHR"));
    vkGetMemoryWin32HandlePropertiesKHR =
        reinterpret_cast<PFN_vkGetMemoryWin32HandlePropertiesKHR>(dlsym(libvulkan, "vkGetMemoryWin32HandlePropertiesKHR"));
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
    vkImportSemaphoreWin32HandleKHR =
        reinterpret_cast<PFN_vkImportSemaphoreWin32HandleKHR>(dlsym(libvulkan, "vkImportSemaphoreWin32HandleKHR"));
    vkGetSemaphoreWin32HandleKHR =
        reinterpret_cast<PFN_vkGetSemaphoreWin32HandleKHR>(dlsym(libvulkan, "vkGetSemaphoreWin32HandleKHR"));
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
    vkImportFenceWin32HandleKHR =
        reinterpret_cast<PFN_vkImportFenceWin32HandleKHR>(dlsym(libvulkan, "vkImportFenceWin32HandleKHR"));
    vkGetFenceWin32HandleKHR = reinterpret_cast<PFN_vkGetFenceWin32HandleKHR>(dlsym(libvulkan, "vkGetFenceWin32HandleKHR"));
#endif

#ifdef VK_USE_PLATFORM_XCB_KHR
    vkCreateXcbSurfaceKHR = reinterpret_cast<PFN_vkCreateXcbSurfaceKHR>(dlsym(libvulkan, "vkCreateXcbSurfaceKHR"));
    vkGetPhysicalDeviceXcbPresentationSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceXcbPresentationSupportKHR"));
#endif

#ifdef VK_USE_PLATFORM_XLIB_KHR
    vkCreateXlibSurfaceKHR = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(dlsym(libvulkan, "vkCreateXlibSurfaceKHR"));
    vkGetPhysicalDeviceXlibPresentationSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceXlibPresentationSupportKHR"));
#endif

#ifdef VK_ENABLE_BETA_EXTENSIONS
    vkGetPhysicalDeviceVideoCapabilitiesKHR =
        reinterpret_cast<PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR>(dlsym(libvulkan, "vkGetPhysicalDeviceVideoCapabilitiesKHR"));
    vkGetPhysicalDeviceVideoFormatPropertiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR>(
        dlsym(libvulkan, "vkGetPhysicalDeviceVideoFormatPropertiesKHR"));
    vkCreateVideoSessionKHR = reinterpret_cast<PFN_vkCreateVideoSessionKHR>(dlsym(libvulkan, "vkCreateVideoSessionKHR"));
    vkDestroyVideoSessionKHR = reinterpret_cast<PFN_vkDestroyVideoSessionKHR>(dlsym(libvulkan, "vkDestroyVideoSessionKHR"));
    vkGetVideoSessionMemoryRequirementsKHR =
        reinterpret_cast<PFN_vkGetVideoSessionMemoryRequirementsKHR>(dlsym(libvulkan, "vkGetVideoSessionMemoryRequirementsKHR"));
    vkBindVideoSessionMemoryKHR =
        reinterpret_cast<PFN_vkBindVideoSessionMemoryKHR>(dlsym(libvulkan, "vkBindVideoSessionMemoryKHR"));
    vkCreateVideoSessionParametersKHR =
        reinterpret_cast<PFN_vkCreateVideoSessionParametersKHR>(dlsym(libvulkan, "vkCreateVideoSessionParametersKHR"));
    vkUpdateVideoSessionParametersKHR =
        reinterpret_cast<PFN_vkUpdateVideoSessionParametersKHR>(dlsym(libvulkan, "vkUpdateVideoSessionParametersKHR"));
    vkDestroyVideoSessionParametersKHR =
        reinterpret_cast<PFN_vkDestroyVideoSessionParametersKHR>(dlsym(libvulkan, "vkDestroyVideoSessionParametersKHR"));
    vkCmdBeginVideoCodingKHR = reinterpret_cast<PFN_vkCmdBeginVideoCodingKHR>(dlsym(libvulkan, "vkCmdBeginVideoCodingKHR"));
    vkCmdEndVideoCodingKHR = reinterpret_cast<PFN_vkCmdEndVideoCodingKHR>(dlsym(libvulkan, "vkCmdEndVideoCodingKHR"));
    vkCmdControlVideoCodingKHR = reinterpret_cast<PFN_vkCmdControlVideoCodingKHR>(dlsym(libvulkan, "vkCmdControlVideoCodingKHR"));
#endif

#ifdef VK_ENABLE_BETA_EXTENSIONS
    vkCmdDecodeVideoKHR = reinterpret_cast<PFN_vkCmdDecodeVideoKHR>(dlsym(libvulkan, "vkCmdDecodeVideoKHR"));
#endif

#ifdef VK_ENABLE_BETA_EXTENSIONS
    vkCmdEncodeVideoKHR = reinterpret_cast<PFN_vkCmdEncodeVideoKHR>(dlsym(libvulkan, "vkCmdEncodeVideoKHR"));
#endif

    vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(dlsym(libvulkan, "vkCreateDebugReportCallbackEXT"));
    vkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(dlsym(libvulkan, "vkDestroyDebugReportCallbackEXT"));
    vkDebugReportMessageEXT = reinterpret_cast<PFN_vkDebugReportMessageEXT>(dlsym(libvulkan, "vkDebugReportMessageEXT"));

#if defined(__APPLE__)
    vkCreateMacOSSurfaceMVK = reinterpret_cast<PFN_vkCreateMacOSSurfaceMVK>(dlsym(libvulkan, "vkCreateMacOSSurfaceMVK"));
#endif

    return 1;
}

// No Vulkan support, do not set function addresses
PFN_vkCreateInstance vkCreateInstance;
PFN_vkDestroyInstance vkDestroyInstance;
PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
PFN_vkGetPhysicalDeviceFormatProperties vkGetPhysicalDeviceFormatProperties;
PFN_vkGetPhysicalDeviceImageFormatProperties vkGetPhysicalDeviceImageFormatProperties;
PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
PFN_vkCreateDevice vkCreateDevice;
PFN_vkDestroyDevice vkDestroyDevice;
PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
PFN_vkEnumerateDeviceExtensionProperties vkEnumerateDeviceExtensionProperties;
PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerProperties;
PFN_vkGetDeviceQueue vkGetDeviceQueue;
PFN_vkQueueSubmit vkQueueSubmit;
PFN_vkQueueWaitIdle vkQueueWaitIdle;
PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
PFN_vkAllocateMemory vkAllocateMemory;
PFN_vkFreeMemory vkFreeMemory;
PFN_vkMapMemory vkMapMemory;
PFN_vkUnmapMemory vkUnmapMemory;
PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges;
PFN_vkGetDeviceMemoryCommitment vkGetDeviceMemoryCommitment;
PFN_vkBindBufferMemory vkBindBufferMemory;
PFN_vkBindImageMemory vkBindImageMemory;
PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
PFN_vkGetImageSparseMemoryRequirements vkGetImageSparseMemoryRequirements;
PFN_vkGetPhysicalDeviceSparseImageFormatProperties vkGetPhysicalDeviceSparseImageFormatProperties;
PFN_vkQueueBindSparse vkQueueBindSparse;
PFN_vkCreateFence vkCreateFence;
PFN_vkDestroyFence vkDestroyFence;
PFN_vkResetFences vkResetFences;
PFN_vkGetFenceStatus vkGetFenceStatus;
PFN_vkWaitForFences vkWaitForFences;
PFN_vkCreateSemaphore vkCreateSemaphore;
PFN_vkDestroySemaphore vkDestroySemaphore;
PFN_vkCreateEvent vkCreateEvent;
PFN_vkDestroyEvent vkDestroyEvent;
PFN_vkGetEventStatus vkGetEventStatus;
PFN_vkSetEvent vkSetEvent;
PFN_vkResetEvent vkResetEvent;
PFN_vkCreateQueryPool vkCreateQueryPool;
PFN_vkDestroyQueryPool vkDestroyQueryPool;
PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
PFN_vkCreateBuffer vkCreateBuffer;
PFN_vkDestroyBuffer vkDestroyBuffer;
PFN_vkCreateBufferView vkCreateBufferView;
PFN_vkDestroyBufferView vkDestroyBufferView;
PFN_vkCreateImage vkCreateImage;
PFN_vkDestroyImage vkDestroyImage;
PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout;
PFN_vkCreateImageView vkCreateImageView;
PFN_vkDestroyImageView vkDestroyImageView;
PFN_vkCreateShaderModule vkCreateShaderModule;
PFN_vkDestroyShaderModule vkDestroyShaderModule;
PFN_vkCreatePipelineCache vkCreatePipelineCache;
PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
PFN_vkGetPipelineCacheData vkGetPipelineCacheData;
PFN_vkMergePipelineCaches vkMergePipelineCaches;
PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
PFN_vkCreateComputePipelines vkCreateComputePipelines;
PFN_vkDestroyPipeline vkDestroyPipeline;
PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
PFN_vkCreateSampler vkCreateSampler;
PFN_vkDestroySampler vkDestroySampler;
PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
PFN_vkResetDescriptorPool vkResetDescriptorPool;
PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
PFN_vkCreateFramebuffer vkCreateFramebuffer;
PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
PFN_vkCreateRenderPass vkCreateRenderPass;
PFN_vkDestroyRenderPass vkDestroyRenderPass;
PFN_vkGetRenderAreaGranularity vkGetRenderAreaGranularity;
PFN_vkCreateCommandPool vkCreateCommandPool;
PFN_vkDestroyCommandPool vkDestroyCommandPool;
PFN_vkResetCommandPool vkResetCommandPool;
PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
PFN_vkEndCommandBuffer vkEndCommandBuffer;
PFN_vkResetCommandBuffer vkResetCommandBuffer;
PFN_vkCmdBindPipeline vkCmdBindPipeline;
PFN_vkCmdSetViewport vkCmdSetViewport;
PFN_vkCmdSetScissor vkCmdSetScissor;
PFN_vkCmdSetLineWidth vkCmdSetLineWidth;
PFN_vkCmdSetDepthBias vkCmdSetDepthBias;
PFN_vkCmdSetBlendConstants vkCmdSetBlendConstants;
PFN_vkCmdSetDepthBounds vkCmdSetDepthBounds;
PFN_vkCmdSetStencilCompareMask vkCmdSetStencilCompareMask;
PFN_vkCmdSetStencilWriteMask vkCmdSetStencilWriteMask;
PFN_vkCmdSetStencilReference vkCmdSetStencilReference;
PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
PFN_vkCmdDraw vkCmdDraw;
PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
PFN_vkCmdDrawIndirect vkCmdDrawIndirect;
PFN_vkCmdDrawIndexedIndirect vkCmdDrawIndexedIndirect;
PFN_vkCmdDispatch vkCmdDispatch;
PFN_vkCmdDispatchIndirect vkCmdDispatchIndirect;
PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
PFN_vkCmdCopyImage vkCmdCopyImage;
PFN_vkCmdBlitImage vkCmdBlitImage;
PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
PFN_vkCmdUpdateBuffer vkCmdUpdateBuffer;
PFN_vkCmdFillBuffer vkCmdFillBuffer;
PFN_vkCmdClearColorImage vkCmdClearColorImage;
PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage;
PFN_vkCmdClearAttachments vkCmdClearAttachments;
PFN_vkCmdResolveImage vkCmdResolveImage;
PFN_vkCmdSetEvent vkCmdSetEvent;
PFN_vkCmdResetEvent vkCmdResetEvent;
PFN_vkCmdWaitEvents vkCmdWaitEvents;
PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
PFN_vkCmdBeginQuery vkCmdBeginQuery;
PFN_vkCmdEndQuery vkCmdEndQuery;
PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp;
PFN_vkCmdCopyQueryPoolResults vkCmdCopyQueryPoolResults;
PFN_vkCmdPushConstants vkCmdPushConstants;
PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
PFN_vkCmdNextSubpass vkCmdNextSubpass;
PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
PFN_vkCmdExecuteCommands vkCmdExecuteCommands;
PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersion;
PFN_vkBindBufferMemory2 vkBindBufferMemory2;
PFN_vkBindImageMemory2 vkBindImageMemory2;
PFN_vkGetDeviceGroupPeerMemoryFeatures vkGetDeviceGroupPeerMemoryFeatures;
PFN_vkCmdSetDeviceMask vkCmdSetDeviceMask;
PFN_vkCmdDispatchBase vkCmdDispatchBase;
PFN_vkEnumeratePhysicalDeviceGroups vkEnumeratePhysicalDeviceGroups;
PFN_vkGetImageMemoryRequirements2 vkGetImageMemoryRequirements2;
PFN_vkGetBufferMemoryRequirements2 vkGetBufferMemoryRequirements2;
PFN_vkGetImageSparseMemoryRequirements2 vkGetImageSparseMemoryRequirements2;
PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2;
PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2;
PFN_vkGetPhysicalDeviceFormatProperties2 vkGetPhysicalDeviceFormatProperties2;
PFN_vkGetPhysicalDeviceImageFormatProperties2 vkGetPhysicalDeviceImageFormatProperties2;
PFN_vkGetPhysicalDeviceQueueFamilyProperties2 vkGetPhysicalDeviceQueueFamilyProperties2;
PFN_vkGetPhysicalDeviceMemoryProperties2 vkGetPhysicalDeviceMemoryProperties2;
PFN_vkGetPhysicalDeviceSparseImageFormatProperties2 vkGetPhysicalDeviceSparseImageFormatProperties2;
PFN_vkTrimCommandPool vkTrimCommandPool;
PFN_vkGetDeviceQueue2 vkGetDeviceQueue2;
PFN_vkCreateSamplerYcbcrConversion vkCreateSamplerYcbcrConversion;
PFN_vkDestroySamplerYcbcrConversion vkDestroySamplerYcbcrConversion;
PFN_vkCreateDescriptorUpdateTemplate vkCreateDescriptorUpdateTemplate;
PFN_vkDestroyDescriptorUpdateTemplate vkDestroyDescriptorUpdateTemplate;
PFN_vkUpdateDescriptorSetWithTemplate vkUpdateDescriptorSetWithTemplate;
PFN_vkGetPhysicalDeviceExternalBufferProperties vkGetPhysicalDeviceExternalBufferProperties;
PFN_vkGetPhysicalDeviceExternalFenceProperties vkGetPhysicalDeviceExternalFenceProperties;
PFN_vkGetPhysicalDeviceExternalSemaphoreProperties vkGetPhysicalDeviceExternalSemaphoreProperties;
PFN_vkGetDescriptorSetLayoutSupport vkGetDescriptorSetLayoutSupport;
PFN_vkCmdDrawIndirectCount vkCmdDrawIndirectCount;
PFN_vkCmdDrawIndexedIndirectCount vkCmdDrawIndexedIndirectCount;
PFN_vkCreateRenderPass2 vkCreateRenderPass2;
PFN_vkCmdBeginRenderPass2 vkCmdBeginRenderPass2;
PFN_vkCmdNextSubpass2 vkCmdNextSubpass2;
PFN_vkCmdEndRenderPass2 vkCmdEndRenderPass2;
PFN_vkResetQueryPool vkResetQueryPool;
PFN_vkGetSemaphoreCounterValue vkGetSemaphoreCounterValue;
PFN_vkWaitSemaphores vkWaitSemaphores;
PFN_vkSignalSemaphore vkSignalSemaphore;
PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;
PFN_vkGetBufferOpaqueCaptureAddress vkGetBufferOpaqueCaptureAddress;
PFN_vkGetDeviceMemoryOpaqueCaptureAddress vkGetDeviceMemoryOpaqueCaptureAddress;
PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
PFN_vkQueuePresentKHR vkQueuePresentKHR;
PFN_vkGetDeviceGroupPresentCapabilitiesKHR vkGetDeviceGroupPresentCapabilitiesKHR;
PFN_vkGetDeviceGroupSurfacePresentModesKHR vkGetDeviceGroupSurfacePresentModesKHR;
PFN_vkGetPhysicalDevicePresentRectanglesKHR vkGetPhysicalDevicePresentRectanglesKHR;
PFN_vkAcquireNextImage2KHR vkAcquireNextImage2KHR;
PFN_vkGetPhysicalDeviceDisplayPropertiesKHR vkGetPhysicalDeviceDisplayPropertiesKHR;
PFN_vkGetPhysicalDeviceDisplayPlanePropertiesKHR vkGetPhysicalDeviceDisplayPlanePropertiesKHR;
PFN_vkGetDisplayPlaneSupportedDisplaysKHR vkGetDisplayPlaneSupportedDisplaysKHR;
PFN_vkGetDisplayModePropertiesKHR vkGetDisplayModePropertiesKHR;
PFN_vkCreateDisplayModeKHR vkCreateDisplayModeKHR;
PFN_vkGetDisplayPlaneCapabilitiesKHR vkGetDisplayPlaneCapabilitiesKHR;
PFN_vkCreateDisplayPlaneSurfaceKHR vkCreateDisplayPlaneSurfaceKHR;
PFN_vkCreateSharedSwapchainsKHR vkCreateSharedSwapchainsKHR;
PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR;
PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR;
PFN_vkGetPhysicalDeviceFeatures2KHR vkGetPhysicalDeviceFeatures2KHR;
PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR;
PFN_vkGetPhysicalDeviceFormatProperties2KHR vkGetPhysicalDeviceFormatProperties2KHR;
PFN_vkGetPhysicalDeviceImageFormatProperties2KHR vkGetPhysicalDeviceImageFormatProperties2KHR;
PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR vkGetPhysicalDeviceQueueFamilyProperties2KHR;
PFN_vkGetPhysicalDeviceMemoryProperties2KHR vkGetPhysicalDeviceMemoryProperties2KHR;
PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR vkGetPhysicalDeviceSparseImageFormatProperties2KHR;
PFN_vkGetDeviceGroupPeerMemoryFeaturesKHR vkGetDeviceGroupPeerMemoryFeaturesKHR;
PFN_vkCmdSetDeviceMaskKHR vkCmdSetDeviceMaskKHR;
PFN_vkCmdDispatchBaseKHR vkCmdDispatchBaseKHR;
PFN_vkTrimCommandPoolKHR vkTrimCommandPoolKHR;
PFN_vkEnumeratePhysicalDeviceGroupsKHR vkEnumeratePhysicalDeviceGroupsKHR;
PFN_vkGetPhysicalDeviceExternalBufferPropertiesKHR vkGetPhysicalDeviceExternalBufferPropertiesKHR;
PFN_vkGetMemoryFdKHR vkGetMemoryFdKHR;
PFN_vkGetMemoryFdPropertiesKHR vkGetMemoryFdPropertiesKHR;
PFN_vkGetPhysicalDeviceExternalSemaphorePropertiesKHR vkGetPhysicalDeviceExternalSemaphorePropertiesKHR;
PFN_vkImportSemaphoreFdKHR vkImportSemaphoreFdKHR;
PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHR;
PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;
PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR;
PFN_vkCreateDescriptorUpdateTemplateKHR vkCreateDescriptorUpdateTemplateKHR;
PFN_vkDestroyDescriptorUpdateTemplateKHR vkDestroyDescriptorUpdateTemplateKHR;
PFN_vkUpdateDescriptorSetWithTemplateKHR vkUpdateDescriptorSetWithTemplateKHR;
PFN_vkCreateRenderPass2KHR vkCreateRenderPass2KHR;
PFN_vkCmdBeginRenderPass2KHR vkCmdBeginRenderPass2KHR;
PFN_vkCmdNextSubpass2KHR vkCmdNextSubpass2KHR;
PFN_vkCmdEndRenderPass2KHR vkCmdEndRenderPass2KHR;
PFN_vkGetSwapchainStatusKHR vkGetSwapchainStatusKHR;
PFN_vkGetPhysicalDeviceExternalFencePropertiesKHR vkGetPhysicalDeviceExternalFencePropertiesKHR;
PFN_vkImportFenceFdKHR vkImportFenceFdKHR;
PFN_vkGetFenceFdKHR vkGetFenceFdKHR;
PFN_vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR vkEnumeratePhysicalDeviceQueueFamilyPerformanceQueryCountersKHR;
PFN_vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR vkGetPhysicalDeviceQueueFamilyPerformanceQueryPassesKHR;
PFN_vkAcquireProfilingLockKHR vkAcquireProfilingLockKHR;
PFN_vkReleaseProfilingLockKHR vkReleaseProfilingLockKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilities2KHR vkGetPhysicalDeviceSurfaceCapabilities2KHR;
PFN_vkGetPhysicalDeviceSurfaceFormats2KHR vkGetPhysicalDeviceSurfaceFormats2KHR;
PFN_vkGetPhysicalDeviceDisplayProperties2KHR vkGetPhysicalDeviceDisplayProperties2KHR;
PFN_vkGetPhysicalDeviceDisplayPlaneProperties2KHR vkGetPhysicalDeviceDisplayPlaneProperties2KHR;
PFN_vkGetDisplayModeProperties2KHR vkGetDisplayModeProperties2KHR;
PFN_vkGetDisplayPlaneCapabilities2KHR vkGetDisplayPlaneCapabilities2KHR;
PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR;
PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR;
PFN_vkGetImageSparseMemoryRequirements2KHR vkGetImageSparseMemoryRequirements2KHR;
PFN_vkCreateSamplerYcbcrConversionKHR vkCreateSamplerYcbcrConversionKHR;
PFN_vkDestroySamplerYcbcrConversionKHR vkDestroySamplerYcbcrConversionKHR;
PFN_vkBindBufferMemory2KHR vkBindBufferMemory2KHR;
PFN_vkBindImageMemory2KHR vkBindImageMemory2KHR;
PFN_vkGetDescriptorSetLayoutSupportKHR vkGetDescriptorSetLayoutSupportKHR;
PFN_vkCmdDrawIndirectCountKHR vkCmdDrawIndirectCountKHR;
PFN_vkCmdDrawIndexedIndirectCountKHR vkCmdDrawIndexedIndirectCountKHR;
PFN_vkGetSemaphoreCounterValueKHR vkGetSemaphoreCounterValueKHR;
PFN_vkWaitSemaphoresKHR vkWaitSemaphoresKHR;
PFN_vkSignalSemaphoreKHR vkSignalSemaphoreKHR;
PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR vkGetPhysicalDeviceFragmentShadingRatesKHR;
PFN_vkCmdSetFragmentShadingRateKHR vkCmdSetFragmentShadingRateKHR;
PFN_vkWaitForPresentKHR vkWaitForPresentKHR;
PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR;
PFN_vkGetBufferOpaqueCaptureAddressKHR vkGetBufferOpaqueCaptureAddressKHR;
PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR vkGetDeviceMemoryOpaqueCaptureAddressKHR;
PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR;
PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR;
PFN_vkGetDeferredOperationMaxConcurrencyKHR vkGetDeferredOperationMaxConcurrencyKHR;
PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR;
PFN_vkDeferredOperationJoinKHR vkDeferredOperationJoinKHR;
PFN_vkGetPipelineExecutablePropertiesKHR vkGetPipelineExecutablePropertiesKHR;
PFN_vkGetPipelineExecutableStatisticsKHR vkGetPipelineExecutableStatisticsKHR;
PFN_vkGetPipelineExecutableInternalRepresentationsKHR vkGetPipelineExecutableInternalRepresentationsKHR;
PFN_vkCmdSetEvent2KHR vkCmdSetEvent2KHR;
PFN_vkCmdResetEvent2KHR vkCmdResetEvent2KHR;
PFN_vkCmdWaitEvents2KHR vkCmdWaitEvents2KHR;
PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR;
PFN_vkCmdWriteTimestamp2KHR vkCmdWriteTimestamp2KHR;
PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR;
PFN_vkCmdWriteBufferMarker2AMD vkCmdWriteBufferMarker2AMD;
PFN_vkGetQueueCheckpointData2NV vkGetQueueCheckpointData2NV;
PFN_vkCmdCopyBuffer2KHR vkCmdCopyBuffer2KHR;
PFN_vkCmdCopyImage2KHR vkCmdCopyImage2KHR;
PFN_vkCmdCopyBufferToImage2KHR vkCmdCopyBufferToImage2KHR;
PFN_vkCmdCopyImageToBuffer2KHR vkCmdCopyImageToBuffer2KHR;
PFN_vkCmdBlitImage2KHR vkCmdBlitImage2KHR;
PFN_vkCmdResolveImage2KHR vkCmdResolveImage2KHR;
PFN_vkGetDeviceBufferMemoryRequirementsKHR vkGetDeviceBufferMemoryRequirementsKHR;
PFN_vkGetDeviceImageMemoryRequirementsKHR vkGetDeviceImageMemoryRequirementsKHR;
PFN_vkGetDeviceImageSparseMemoryRequirementsKHR vkGetDeviceImageSparseMemoryRequirementsKHR;
PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT;
PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT;
PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT;
PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTagEXT;
PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectNameEXT;
PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBeginEXT;
PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEndEXT;
PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsertEXT;
PFN_vkCmdBindTransformFeedbackBuffersEXT vkCmdBindTransformFeedbackBuffersEXT;
PFN_vkCmdBeginTransformFeedbackEXT vkCmdBeginTransformFeedbackEXT;
PFN_vkCmdEndTransformFeedbackEXT vkCmdEndTransformFeedbackEXT;
PFN_vkCmdBeginQueryIndexedEXT vkCmdBeginQueryIndexedEXT;
PFN_vkCmdEndQueryIndexedEXT vkCmdEndQueryIndexedEXT;
PFN_vkCmdDrawIndirectByteCountEXT vkCmdDrawIndirectByteCountEXT;
PFN_vkCreateCuModuleNVX vkCreateCuModuleNVX;
PFN_vkCreateCuFunctionNVX vkCreateCuFunctionNVX;
PFN_vkDestroyCuModuleNVX vkDestroyCuModuleNVX;
PFN_vkDestroyCuFunctionNVX vkDestroyCuFunctionNVX;
PFN_vkCmdCuLaunchKernelNVX vkCmdCuLaunchKernelNVX;
PFN_vkGetImageViewHandleNVX vkGetImageViewHandleNVX;
PFN_vkGetImageViewAddressNVX vkGetImageViewAddressNVX;
PFN_vkCmdDrawIndirectCountAMD vkCmdDrawIndirectCountAMD;
PFN_vkCmdDrawIndexedIndirectCountAMD vkCmdDrawIndexedIndirectCountAMD;
PFN_vkGetShaderInfoAMD vkGetShaderInfoAMD;
PFN_vkGetPhysicalDeviceExternalImageFormatPropertiesNV vkGetPhysicalDeviceExternalImageFormatPropertiesNV;
PFN_vkCmdBeginConditionalRenderingEXT vkCmdBeginConditionalRenderingEXT;
PFN_vkCmdEndConditionalRenderingEXT vkCmdEndConditionalRenderingEXT;
PFN_vkCmdSetViewportWScalingNV vkCmdSetViewportWScalingNV;
PFN_vkReleaseDisplayEXT vkReleaseDisplayEXT;
PFN_vkGetPhysicalDeviceSurfaceCapabilities2EXT vkGetPhysicalDeviceSurfaceCapabilities2EXT;
PFN_vkDisplayPowerControlEXT vkDisplayPowerControlEXT;
PFN_vkRegisterDeviceEventEXT vkRegisterDeviceEventEXT;
PFN_vkRegisterDisplayEventEXT vkRegisterDisplayEventEXT;
PFN_vkGetSwapchainCounterEXT vkGetSwapchainCounterEXT;
PFN_vkGetRefreshCycleDurationGOOGLE vkGetRefreshCycleDurationGOOGLE;
PFN_vkGetPastPresentationTimingGOOGLE vkGetPastPresentationTimingGOOGLE;
PFN_vkCmdSetDiscardRectangleEXT vkCmdSetDiscardRectangleEXT;
PFN_vkSetHdrMetadataEXT vkSetHdrMetadataEXT;
PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;
PFN_vkSetDebugUtilsObjectTagEXT vkSetDebugUtilsObjectTagEXT;
PFN_vkQueueBeginDebugUtilsLabelEXT vkQueueBeginDebugUtilsLabelEXT;
PFN_vkQueueEndDebugUtilsLabelEXT vkQueueEndDebugUtilsLabelEXT;
PFN_vkQueueInsertDebugUtilsLabelEXT vkQueueInsertDebugUtilsLabelEXT;
PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;
PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT;
PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
PFN_vkSubmitDebugUtilsMessageEXT vkSubmitDebugUtilsMessageEXT;
PFN_vkCmdSetSampleLocationsEXT vkCmdSetSampleLocationsEXT;
PFN_vkGetPhysicalDeviceMultisamplePropertiesEXT vkGetPhysicalDeviceMultisamplePropertiesEXT;
PFN_vkGetImageDrmFormatModifierPropertiesEXT vkGetImageDrmFormatModifierPropertiesEXT;
PFN_vkCreateValidationCacheEXT vkCreateValidationCacheEXT;
PFN_vkDestroyValidationCacheEXT vkDestroyValidationCacheEXT;
PFN_vkMergeValidationCachesEXT vkMergeValidationCachesEXT;
PFN_vkGetValidationCacheDataEXT vkGetValidationCacheDataEXT;
PFN_vkCmdBindShadingRateImageNV vkCmdBindShadingRateImageNV;
PFN_vkCmdSetViewportShadingRatePaletteNV vkCmdSetViewportShadingRatePaletteNV;
PFN_vkCmdSetCoarseSampleOrderNV vkCmdSetCoarseSampleOrderNV;
PFN_vkCreateAccelerationStructureNV vkCreateAccelerationStructureNV;
PFN_vkDestroyAccelerationStructureNV vkDestroyAccelerationStructureNV;
PFN_vkGetAccelerationStructureMemoryRequirementsNV vkGetAccelerationStructureMemoryRequirementsNV;
PFN_vkBindAccelerationStructureMemoryNV vkBindAccelerationStructureMemoryNV;
PFN_vkCmdBuildAccelerationStructureNV vkCmdBuildAccelerationStructureNV;
PFN_vkCmdCopyAccelerationStructureNV vkCmdCopyAccelerationStructureNV;
PFN_vkCmdTraceRaysNV vkCmdTraceRaysNV;
PFN_vkCreateRayTracingPipelinesNV vkCreateRayTracingPipelinesNV;
PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
PFN_vkGetRayTracingShaderGroupHandlesNV vkGetRayTracingShaderGroupHandlesNV;
PFN_vkGetAccelerationStructureHandleNV vkGetAccelerationStructureHandleNV;
PFN_vkCmdWriteAccelerationStructuresPropertiesNV vkCmdWriteAccelerationStructuresPropertiesNV;
PFN_vkCompileDeferredNV vkCompileDeferredNV;
PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerPropertiesEXT;
PFN_vkCmdWriteBufferMarkerAMD vkCmdWriteBufferMarkerAMD;
PFN_vkGetPhysicalDeviceCalibrateableTimeDomainsEXT vkGetPhysicalDeviceCalibrateableTimeDomainsEXT;
PFN_vkGetCalibratedTimestampsEXT vkGetCalibratedTimestampsEXT;
PFN_vkCmdDrawMeshTasksNV vkCmdDrawMeshTasksNV;
PFN_vkCmdDrawMeshTasksIndirectNV vkCmdDrawMeshTasksIndirectNV;
PFN_vkCmdDrawMeshTasksIndirectCountNV vkCmdDrawMeshTasksIndirectCountNV;
PFN_vkCmdSetExclusiveScissorNV vkCmdSetExclusiveScissorNV;
PFN_vkCmdSetCheckpointNV vkCmdSetCheckpointNV;
PFN_vkGetQueueCheckpointDataNV vkGetQueueCheckpointDataNV;
PFN_vkInitializePerformanceApiINTEL vkInitializePerformanceApiINTEL;
PFN_vkUninitializePerformanceApiINTEL vkUninitializePerformanceApiINTEL;
PFN_vkCmdSetPerformanceMarkerINTEL vkCmdSetPerformanceMarkerINTEL;
PFN_vkCmdSetPerformanceStreamMarkerINTEL vkCmdSetPerformanceStreamMarkerINTEL;
PFN_vkCmdSetPerformanceOverrideINTEL vkCmdSetPerformanceOverrideINTEL;
PFN_vkAcquirePerformanceConfigurationINTEL vkAcquirePerformanceConfigurationINTEL;
PFN_vkReleasePerformanceConfigurationINTEL vkReleasePerformanceConfigurationINTEL;
PFN_vkQueueSetPerformanceConfigurationINTEL vkQueueSetPerformanceConfigurationINTEL;
PFN_vkGetPerformanceParameterINTEL vkGetPerformanceParameterINTEL;
PFN_vkSetLocalDimmingAMD vkSetLocalDimmingAMD;
PFN_vkGetBufferDeviceAddressEXT vkGetBufferDeviceAddressEXT;
PFN_vkGetPhysicalDeviceToolPropertiesEXT vkGetPhysicalDeviceToolPropertiesEXT;
PFN_vkGetPhysicalDeviceCooperativeMatrixPropertiesNV vkGetPhysicalDeviceCooperativeMatrixPropertiesNV;
PFN_vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV
    vkGetPhysicalDeviceSupportedFramebufferMixedSamplesCombinationsNV;
PFN_vkCreateHeadlessSurfaceEXT vkCreateHeadlessSurfaceEXT;
PFN_vkCmdSetLineStippleEXT vkCmdSetLineStippleEXT;
PFN_vkResetQueryPoolEXT vkResetQueryPoolEXT;
PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT;
PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT;
PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT;
PFN_vkCmdSetViewportWithCountEXT vkCmdSetViewportWithCountEXT;
PFN_vkCmdSetScissorWithCountEXT vkCmdSetScissorWithCountEXT;
PFN_vkCmdBindVertexBuffers2EXT vkCmdBindVertexBuffers2EXT;
PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT;
PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT;
PFN_vkCmdSetDepthCompareOpEXT vkCmdSetDepthCompareOpEXT;
PFN_vkCmdSetDepthBoundsTestEnableEXT vkCmdSetDepthBoundsTestEnableEXT;
PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT;
PFN_vkCmdSetStencilOpEXT vkCmdSetStencilOpEXT;
PFN_vkGetGeneratedCommandsMemoryRequirementsNV vkGetGeneratedCommandsMemoryRequirementsNV;
PFN_vkCmdPreprocessGeneratedCommandsNV vkCmdPreprocessGeneratedCommandsNV;
PFN_vkCmdExecuteGeneratedCommandsNV vkCmdExecuteGeneratedCommandsNV;
PFN_vkCmdBindPipelineShaderGroupNV vkCmdBindPipelineShaderGroupNV;
PFN_vkCreateIndirectCommandsLayoutNV vkCreateIndirectCommandsLayoutNV;
PFN_vkDestroyIndirectCommandsLayoutNV vkDestroyIndirectCommandsLayoutNV;
PFN_vkAcquireDrmDisplayEXT vkAcquireDrmDisplayEXT;
PFN_vkGetDrmDisplayEXT vkGetDrmDisplayEXT;
PFN_vkCreatePrivateDataSlotEXT vkCreatePrivateDataSlotEXT;
PFN_vkDestroyPrivateDataSlotEXT vkDestroyPrivateDataSlotEXT;
PFN_vkSetPrivateDataEXT vkSetPrivateDataEXT;
PFN_vkGetPrivateDataEXT vkGetPrivateDataEXT;
PFN_vkCmdSetFragmentShadingRateEnumNV vkCmdSetFragmentShadingRateEnumNV;
#ifdef VK_USE_PLATFORM_WIN32_KHR
	PFN_vkAcquireWinrtDisplayNV vkAcquireWinrtDisplayNV;
	PFN_vkGetWinrtDisplayNV vkGetWinrtDisplayNV;
#endif
PFN_vkCmdSetVertexInputEXT vkCmdSetVertexInputEXT;
PFN_vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI vkGetDeviceSubpassShadingMaxWorkgroupSizeHUAWEI;
PFN_vkCmdSubpassShadingHUAWEI vkCmdSubpassShadingHUAWEI;
PFN_vkCmdBindInvocationMaskHUAWEI vkCmdBindInvocationMaskHUAWEI;
PFN_vkGetMemoryRemoteAddressNV vkGetMemoryRemoteAddressNV;
PFN_vkCmdSetPatchControlPointsEXT vkCmdSetPatchControlPointsEXT;
PFN_vkCmdSetRasterizerDiscardEnableEXT vkCmdSetRasterizerDiscardEnableEXT;
PFN_vkCmdSetDepthBiasEnableEXT vkCmdSetDepthBiasEnableEXT;
PFN_vkCmdSetLogicOpEXT vkCmdSetLogicOpEXT;
PFN_vkCmdSetPrimitiveRestartEnableEXT vkCmdSetPrimitiveRestartEnableEXT;
PFN_vkCmdSetColorWriteEnableEXT vkCmdSetColorWriteEnableEXT;
PFN_vkCmdDrawMultiEXT vkCmdDrawMultiEXT;
PFN_vkCmdDrawMultiIndexedEXT vkCmdDrawMultiIndexedEXT;
PFN_vkSetDeviceMemoryPriorityEXT vkSetDeviceMemoryPriorityEXT;
PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
PFN_vkCmdBuildAccelerationStructuresIndirectKHR vkCmdBuildAccelerationStructuresIndirectKHR;
PFN_vkBuildAccelerationStructuresKHR vkBuildAccelerationStructuresKHR;
PFN_vkCopyAccelerationStructureKHR vkCopyAccelerationStructureKHR;
PFN_vkCopyAccelerationStructureToMemoryKHR vkCopyAccelerationStructureToMemoryKHR;
PFN_vkCopyMemoryToAccelerationStructureKHR vkCopyMemoryToAccelerationStructureKHR;
PFN_vkWriteAccelerationStructuresPropertiesKHR vkWriteAccelerationStructuresPropertiesKHR;
PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR;
PFN_vkCmdCopyAccelerationStructureToMemoryKHR vkCmdCopyAccelerationStructureToMemoryKHR;
PFN_vkCmdCopyMemoryToAccelerationStructureKHR vkCmdCopyMemoryToAccelerationStructureKHR;
PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR;
PFN_vkGetDeviceAccelerationStructureCompatibilityKHR vkGetDeviceAccelerationStructureCompatibilityKHR;
PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
PFN_vkGetRayTracingCaptureReplayShaderGroupHandlesKHR vkGetRayTracingCaptureReplayShaderGroupHandlesKHR;
PFN_vkCmdTraceRaysIndirectKHR vkCmdTraceRaysIndirectKHR;
PFN_vkGetRayTracingShaderGroupStackSizeKHR vkGetRayTracingShaderGroupStackSizeKHR;
PFN_vkCmdSetRayTracingPipelineStackSizeKHR vkCmdSetRayTracingPipelineStackSizeKHR;

#ifdef VK_USE_PLATFORM_ANDROID_KHR
PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHR;
#endif

#ifdef VK_USE_PLATFORM_ANDROID_KHR
PFN_vkGetAndroidHardwareBufferPropertiesANDROID vkGetAndroidHardwareBufferPropertiesANDROID;
PFN_vkGetMemoryAndroidHardwareBufferANDROID vkGetMemoryAndroidHardwareBufferANDROID;
#endif

#ifdef VK_USE_PLATFORM_FUCHSIA
PFN_vkCreateImagePipeSurfaceFUCHSIA vkCreateImagePipeSurfaceFUCHSIA;
#endif

#ifdef VK_USE_PLATFORM_FUCHSIA
PFN_vkGetMemoryZirconHandleFUCHSIA vkGetMemoryZirconHandleFUCHSIA;
PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA vkGetMemoryZirconHandlePropertiesFUCHSIA;
#endif

#ifdef VK_USE_PLATFORM_FUCHSIA
PFN_vkImportSemaphoreZirconHandleFUCHSIA vkImportSemaphoreZirconHandleFUCHSIA;
PFN_vkGetSemaphoreZirconHandleFUCHSIA vkGetSemaphoreZirconHandleFUCHSIA;
#endif

#ifdef VK_USE_PLATFORM_FUCHSIA
PFN_vkCreateBufferCollectionFUCHSIA vkCreateBufferCollectionFUCHSIA;
PFN_vkSetBufferCollectionImageConstraintsFUCHSIA vkSetBufferCollectionImageConstraintsFUCHSIA;
PFN_vkSetBufferCollectionBufferConstraintsFUCHSIA vkSetBufferCollectionBufferConstraintsFUCHSIA;
PFN_vkDestroyBufferCollectionFUCHSIA vkDestroyBufferCollectionFUCHSIA;
PFN_vkGetBufferCollectionPropertiesFUCHSIA vkGetBufferCollectionPropertiesFUCHSIA;
#endif

#ifdef VK_USE_PLATFORM_IOS_MVK
PFN_vkCreateIOSSurfaceMVK vkCreateIOSSurfaceMVK;
#endif

#ifdef VK_USE_PLATFORM_MACOS_MVK
PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;
#endif

#ifdef VK_USE_PLATFORM_METAL_EXT
PFN_vkCreateMetalSurfaceEXT vkCreateMetalSurfaceEXT;
#endif

#ifdef VK_USE_PLATFORM_VI_NN
PFN_vkCreateViSurfaceNN vkCreateViSurfaceNN;
#endif

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR;
PFN_vkGetPhysicalDeviceWaylandPresentationSupportKHR vkGetPhysicalDeviceWaylandPresentationSupportKHR;
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
PFN_vkCreateWin32SurfaceKHR vkCreateWin32SurfaceKHR;
PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR vkGetPhysicalDeviceWin32PresentationSupportKHR;
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
PFN_vkGetMemoryWin32HandleKHR vkGetMemoryWin32HandleKHR;
PFN_vkGetMemoryWin32HandlePropertiesKHR vkGetMemoryWin32HandlePropertiesKHR;
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
PFN_vkImportSemaphoreWin32HandleKHR vkImportSemaphoreWin32HandleKHR;
PFN_vkGetSemaphoreWin32HandleKHR vkGetSemaphoreWin32HandleKHR;
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
PFN_vkImportFenceWin32HandleKHR vkImportFenceWin32HandleKHR;
PFN_vkGetFenceWin32HandleKHR vkGetFenceWin32HandleKHR;
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
PFN_vkGetMemoryWin32HandleNV vkGetMemoryWin32HandleNV;
#endif

#ifdef VK_USE_PLATFORM_WIN32_KHR
PFN_vkGetPhysicalDeviceSurfacePresentModes2EXT vkGetPhysicalDeviceSurfacePresentModes2EXT;
PFN_vkAcquireFullScreenExclusiveModeEXT vkAcquireFullScreenExclusiveModeEXT;
PFN_vkReleaseFullScreenExclusiveModeEXT vkReleaseFullScreenExclusiveModeEXT;
PFN_vkGetDeviceGroupSurfacePresentModes2EXT vkGetDeviceGroupSurfacePresentModes2EXT;
#endif

#ifdef VK_USE_PLATFORM_XCB_KHR
PFN_vkCreateXcbSurfaceKHR vkCreateXcbSurfaceKHR;
PFN_vkGetPhysicalDeviceXcbPresentationSupportKHR vkGetPhysicalDeviceXcbPresentationSupportKHR;
#endif

#ifdef VK_USE_PLATFORM_XLIB_KHR
PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHR;
PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR vkGetPhysicalDeviceXlibPresentationSupportKHR;
#endif

#ifdef VK_USE_PLATFORM_DIRECTFB_EXT
PFN_vkCreateDirectFBSurfaceEXT vkCreateDirectFBSurfaceEXT;
PFN_vkGetPhysicalDeviceDirectFBPresentationSupportEXT vkGetPhysicalDeviceDirectFBPresentationSupportEXT;
#endif

#ifdef VK_USE_PLATFORM_XLIB_XRANDR_EXT
PFN_vkAcquireXlibDisplayEXT vkAcquireXlibDisplayEXT;
PFN_vkGetRandROutputDisplayEXT vkGetRandROutputDisplayEXT;
#endif

#ifdef VK_USE_PLATFORM_GGP
PFN_vkCreateStreamDescriptorSurfaceGGP vkCreateStreamDescriptorSurfaceGGP;
#endif

#ifdef VK_USE_PLATFORM_SCREEN_QNX
PFN_vkCreateScreenSurfaceQNX vkCreateScreenSurfaceQNX;
PFN_vkGetPhysicalDeviceScreenPresentationSupportQNX vkGetPhysicalDeviceScreenPresentationSupportQNX;
#endif

#ifdef VK_ENABLE_BETA_EXTENSIONS
PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR vkGetPhysicalDeviceVideoCapabilitiesKHR;
PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR vkGetPhysicalDeviceVideoFormatPropertiesKHR;
PFN_vkCreateVideoSessionKHR vkCreateVideoSessionKHR;
PFN_vkDestroyVideoSessionKHR vkDestroyVideoSessionKHR;
PFN_vkGetVideoSessionMemoryRequirementsKHR vkGetVideoSessionMemoryRequirementsKHR;
PFN_vkBindVideoSessionMemoryKHR vkBindVideoSessionMemoryKHR;
PFN_vkCreateVideoSessionParametersKHR vkCreateVideoSessionParametersKHR;
PFN_vkUpdateVideoSessionParametersKHR vkUpdateVideoSessionParametersKHR;
PFN_vkDestroyVideoSessionParametersKHR vkDestroyVideoSessionParametersKHR;
PFN_vkCmdBeginVideoCodingKHR vkCmdBeginVideoCodingKHR;
PFN_vkCmdEndVideoCodingKHR vkCmdEndVideoCodingKHR;
PFN_vkCmdControlVideoCodingKHR vkCmdControlVideoCodingKHR;
#endif

#ifdef VK_ENABLE_BETA_EXTENSIONS
PFN_vkCmdDecodeVideoKHR vkCmdDecodeVideoKHR;
#endif

#ifdef VK_ENABLE_BETA_EXTENSIONS
PFN_vkCmdEncodeVideoKHR vkCmdEncodeVideoKHR;
#endif

#if defined(__APPLE__)
PFN_vkCreateMacOSSurfaceMVK vkCreateMacOSSurfaceMVK;
#endif

#ifdef __cplusplus
}
#endif
