// Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
// Sample-owned projection and quad images. Only public OVRTX/OpenXR/Vulkan APIs are used.
#include "../common/sample_support.h"
#include "../common/quad_upload_cache.h"
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <ovrtx/ovrtx.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

extern thread_local std::string sampleError;
namespace
{
static_assert(sizeof(SamplePose) == sizeof(XrPosef), "Sample pose must match OpenXR");
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void xrCheck(XrResult r)
{
    if (XR_FAILED(r)) throw std::runtime_error("Projection OpenXR error " + std::to_string(r));
}
void vkCheck(VkResult r)
{
    if (r != VK_SUCCESS) throw std::runtime_error("Projection Vulkan error " + std::to_string(r));
}
void rtCheck(ovrtx_api_status_t r)
{
    if (r != OVRTX_API_SUCCESS) throw std::runtime_error("Projection OVRTX output error " + std::to_string(r));
}
std::string text(ovx_string_t s) { return s.ptr ? std::string(s.ptr, s.length) : std::string(); }

#define SAMPLE_VK_FUNCTIONS(X) \
    X(GetPhysicalDeviceMemoryProperties) X(CreateBuffer) X(DestroyBuffer) \
    X(GetBufferMemoryRequirements) X(AllocateMemory) X(FreeMemory) X(BindBufferMemory) \
    X(MapMemory) X(UnmapMemory) X(CreateCommandPool) X(DestroyCommandPool) \
    X(AllocateCommandBuffers) X(ResetCommandBuffer) X(BeginCommandBuffer) X(EndCommandBuffer) \
    X(CmdPipelineBarrier) X(CmdCopyBufferToImage) X(CmdCopyImageToBuffer) X(QueueSubmit) X(QueueWaitIdle)

struct Chain
{
    XrSwapchain handle = XR_NULL_HANDLE;
    uint32_t width = 0, height = 0;
    bool depth = false;
    bool readback = false;
    bool srgb = false;
    std::vector<XrSwapchainImageVulkanKHR> images;
};
struct Projection
{
    sample::QuadUploadCache quadUploadCache;
    ovrtx_renderer_t* renderer;
    XrInstance instance;
    XrSession session;
    XrSpace space;
    PFN_xrGetInstanceProcAddr getProc;
    VkInstance vkInstance;
    VkPhysicalDevice physical;
    VkDevice device;
    VkQueue queue;
    void* loader = nullptr;
#define DECLARE_VK(name) PFN_vk##name vk##name = nullptr;
    SAMPLE_VK_FUNCTIONS(DECLARE_VK)
#undef DECLARE_VK
    PFN_xrDestroySwapchain destroySwapchain = nullptr;
    PFN_xrDestroySpace destroySpace = nullptr;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize capacity = 0;
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer command = VK_NULL_HANDLE;
    std::array<Chain, 4> chains; // color/depth pairs, stereo only
    std::array<XrCompositionLayerProjectionView, 2> views{};
    std::array<XrCompositionLayerDepthInfoKHR, 2> depths{};
    XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    XrCompositionLayerQuad quad{XR_TYPE_COMPOSITION_LAYER_QUAD};
    XrSpace viewSpace = XR_NULL_HANDLE;
    std::array<bool, 2> ready{};
    std::vector<uint8_t> colorPixels, depthPixels;

    template<class T> T xrFunction(const char* name)
    {
        PFN_xrVoidFunction value = nullptr;
        xrCheck(getProc(instance, name, &value));
        require(value != nullptr, "Missing projection OpenXR function");
        return reinterpret_cast<T>(value);
    }
    void initialize(uint32_t family)
    {
#ifdef _WIN32
        loader = LoadLibraryW(L"vulkan-1.dll");
        auto getInstanceProc = loader ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            GetProcAddress(static_cast<HMODULE>(loader), "vkGetInstanceProcAddr")) : nullptr;
#else
        loader = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
        auto getInstanceProc = loader ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(loader, "vkGetInstanceProcAddr")) : nullptr;
#endif
        require(getInstanceProc != nullptr, "Vulkan loader unavailable");
#define LOAD_VK(name) vk##name = reinterpret_cast<PFN_vk##name>(getInstanceProc(vkInstance, "vk" #name)); \
        require(vk##name != nullptr, "Missing Vulkan function: vk" #name);
        SAMPLE_VK_FUNCTIONS(LOAD_VK)
#undef LOAD_VK
        destroySwapchain = xrFunction<PFN_xrDestroySwapchain>("xrDestroySwapchain");
        destroySpace = xrFunction<PFN_xrDestroySpace>("xrDestroySpace");
        VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        info.queueFamilyIndex = family;
        vkCheck(vkCreateCommandPool(device, &info, nullptr, &pool));
        VkCommandBufferAllocateInfo allocation{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocation.commandPool = pool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 1;
        vkCheck(vkAllocateCommandBuffers(device, &allocation, &command));
        layer.space = space;
        layer.viewCount = 2;
        layer.views = views.data();
    }
    ~Projection()
    {
        if (vkQueueWaitIdle) vkQueueWaitIdle(queue);
        for (auto& chain : chains) if (chain.handle && destroySwapchain) destroySwapchain(chain.handle);
        if (viewSpace && destroySpace) destroySpace(viewSpace);
        if (buffer) vkDestroyBuffer(device, buffer, nullptr);
        if (memory) vkFreeMemory(device, memory, nullptr);
        if (pool) vkDestroyCommandPool(device, pool, nullptr);
#ifdef _WIN32
        if (loader) FreeLibrary(static_cast<HMODULE>(loader));
#else
        if (loader) dlclose(loader);
#endif
    }
    void staging(VkDeviceSize bytes)
    {
        if (capacity >= bytes) return;
        if (buffer) { vkDestroyBuffer(device, buffer, nullptr); buffer = VK_NULL_HANDLE; }
        if (memory) { vkFreeMemory(device, memory, nullptr); memory = VK_NULL_HANDLE; }
        capacity = 0;
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = bytes;
        info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCheck(vkCreateBuffer(device, &info, nullptr, &buffer));
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, buffer, &requirements);
        VkPhysicalDeviceMemoryProperties properties{};
        vkGetPhysicalDeviceMemoryProperties(physical, &properties);
        uint32_t type = properties.memoryTypeCount;
        constexpr VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
            if ((requirements.memoryTypeBits & (1u << i)) && (properties.memoryTypes[i].propertyFlags & flags) == flags)
            { type = i; break; }
        require(type < properties.memoryTypeCount, "No coherent host staging memory on XR device");
        VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocate.allocationSize = requirements.size;
        allocate.memoryTypeIndex = type;
        vkCheck(vkAllocateMemory(device, &allocate, nullptr, &memory));
        vkCheck(vkBindBufferMemory(device, buffer, memory, 0));
        capacity = bytes;
    }
    void configure(Chain& chain, uint32_t width, uint32_t height, bool depth, bool readback = false, bool srgb = false)
    {
        if (chain.handle && chain.width == width && chain.height == height &&
            chain.depth == depth && chain.readback == readback && chain.srgb == srgb) return;
        if (chain.handle) { xrCheck(destroySwapchain(chain.handle)); chain.handle = XR_NULL_HANDLE; }
        // LdrColor bytes are sRGB. A transfer copy does not decode them.
        // Keep procedural quad bytes in their existing linear UNORM format.
        const int64_t format = depth ? VK_FORMAT_D32_SFLOAT :
            (srgb ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM);
        auto enumerate = xrFunction<PFN_xrEnumerateSwapchainFormats>("xrEnumerateSwapchainFormats");
        uint32_t count = 0;
        xrCheck(enumerate(session, 0, &count, nullptr));
        std::vector<int64_t> formats(count);
        xrCheck(enumerate(session, count, &count, formats.data()));
        require(std::find(formats.begin(), formats.end(), format) != formats.end(), "XR runtime lacks required projection format");
        XrSwapchainCreateInfo info{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        info.usageFlags = XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
            (readback ? XR_SWAPCHAIN_USAGE_TRANSFER_SRC_BIT : 0) |
            (depth ? XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT);
        info.format = format;
        info.sampleCount = info.faceCount = info.arraySize = info.mipCount = 1;
        info.width = width; info.height = height;
        xrCheck(xrFunction<PFN_xrCreateSwapchain>("xrCreateSwapchain")(session, &info, &chain.handle));
        chain.width = width; chain.height = height; chain.depth = depth; chain.readback = readback; chain.srgb = srgb;
        auto images = xrFunction<PFN_xrEnumerateSwapchainImages>("xrEnumerateSwapchainImages");
        xrCheck(images(chain.handle, 0, &count, nullptr));
        chain.images.assign(count, {XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR});
        xrCheck(images(chain.handle, count, &count, reinterpret_cast<XrSwapchainImageBaseHeader*>(chain.images.data())));
    }
    void upload(Chain& chain, const void* pixels, void* readback = nullptr)
    {
        const auto bytes = VkDeviceSize(chain.width) * chain.height * 4;
        staging(bytes * (readback ? 2 : 1));
        void* mapped = nullptr;
        vkCheck(vkMapMemory(device, memory, 0, bytes, 0, &mapped));
        std::memcpy(mapped, pixels, size_t(bytes));
        vkUnmapMemory(device, memory);
        uint32_t index = 0;
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        xrCheck(xrFunction<PFN_xrAcquireSwapchainImage>("xrAcquireSwapchainImage")(chain.handle, &acquire, &index));
        XrSwapchainImageWaitInfo wait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wait.timeout = XR_INFINITE_DURATION;
        xrCheck(xrFunction<PFN_xrWaitSwapchainImage>("xrWaitSwapchainImage")(chain.handle, &wait));
        auto release = xrFunction<PFN_xrReleaseSwapchainImage>("xrReleaseSwapchainImage");
        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        try
        {
            require(index < chain.images.size(), "Invalid acquired projection image index");
            vkCheck(vkResetCommandBuffer(command, 0));
            VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkCheck(vkBeginCommandBuffer(command, &begin));
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Each image is fully overwritten.
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = chain.images[index].image;
            barrier.subresourceRange = {VkImageAspectFlags(chain.depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), 0, 1, 0, 1};
            vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {barrier.subresourceRange.aspectMask, 0, 0, 1};
            copy.imageExtent = {chain.width, chain.height, 1};
            vkCmdCopyBufferToImage(command, buffer, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            if (readback)
            {
                require(chain.readback, "Swapchain was not created for readback");
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    0, 0, nullptr, 0, nullptr, 1, &barrier);
                copy.bufferOffset = bytes; // Keep the upload and download regions disjoint.
                vkCmdCopyImageToBuffer(command, barrier.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
                VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
                host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
                host.srcQueueFamilyIndex = host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                host.buffer = buffer; host.offset = bytes; host.size = bytes;
                vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                    0, 0, nullptr, 1, &host, 0, nullptr);
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            }
            barrier.newLayout = chain.depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barrier.dstAccessMask = 0;
            vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);
            vkCheck(vkEndCommandBuffer(command));
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1; submit.pCommandBuffers = &command;
            vkCheck(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
            vkCheck(vkQueueWaitIdle(queue)); // No borrowed queue work escapes this call.
            if (readback)
            {
                vkCheck(vkMapMemory(device, memory, 0, bytes * 2, 0, &mapped));
                std::memcpy(readback, static_cast<const uint8_t*>(mapped) + bytes, size_t(bytes));
                vkUnmapMemory(device, memory);
            }
        }
        catch (...) { vkQueueWaitIdle(queue); release(chain.handle, &releaseInfo); throw; }
        xrCheck(release(chain.handle, &releaseInfo));
    }
    void copyOutput(ovrtx_render_var_output_handle_t output, bool depth, SampleImage& image)
    {
        ovrtx_map_output_description_t desc{OVRTX_MAP_DEVICE_TYPE_CPU, 0};
        ovrtx_render_var_output_t mapped{};
        const auto status = ovrtx_map_render_var_output(renderer, output, &desc, {~uint64_t(0)}, &mapped).status;
        struct Unmap { ovrtx_renderer_t* renderer; ovrtx_render_var_output_map_handle_t map;
            ~Unmap() { if (map != OVRTX_INVALID_RENDER_VAR_OUTPUT_MAP_HANDLE) ovrtx_unmap_render_var_output(renderer, map, {}); } } unmap{renderer, mapped.map_handle};
        rtCheck(status);
        require(mapped.status == OVRTX_EVENT_COMPLETED && mapped.num_tensors == 1, "Incomplete projection output");
        const auto* tensor = mapped.tensors[0].dl;
        require(tensor && tensor->device.device_type == kDLCPU && tensor->dtype.lanes == 1 &&
            tensor->ndim >= 2 && tensor->ndim <= 3, "Invalid projection tensor");
        require(tensor->shape[0] > 0 && tensor->shape[0] <= 8192 && tensor->shape[1] > 0 && tensor->shape[1] <= 8192,
            "Invalid projection image extent");
        const int channels = depth ? 1 : 4;
        require(tensor->dtype.code == (depth ? kDLFloat : kDLUInt) && tensor->dtype.bits == (depth ? 32 : 8) &&
            (tensor->ndim == 3 ? tensor->shape[2] == channels : depth), "Expected LdrColor RGBA8 or XRDepth float32");
        require(!tensor->strides || (tensor->strides[1] == channels &&
            (tensor->ndim == 2 || tensor->strides[2] == 1) && tensor->strides[0] >= tensor->shape[1] * channels),
            "Unsupported projection output strides");
        image.width = uint32_t(tensor->shape[1]); image.height = uint32_t(tensor->shape[0]); image.pitch = image.width * 4;
        auto& storage = depth ? depthPixels : colorPixels;
        storage.resize(size_t(image.pitch) * image.height);
        const auto* source = static_cast<const uint8_t*>(tensor->data) + tensor->byte_offset;
        const size_t stride = tensor->strides ? size_t(tensor->strides[0]) * (depth ? 4 : 1) : image.pitch;
        for (uint32_t row = 0; row < image.height; ++row)
            std::memcpy(storage.data() + size_t(row) * image.pitch, source + row * stride, image.pitch);
        image.rgba = storage.data();
    }
};
}

void* sample_projection_create(uint64_t renderer, uint64_t instance, uint64_t session, uint64_t space,
    uint64_t getProc, uint64_t vkInstance, uint64_t physical, uint64_t device, uint64_t queue, uint32_t family)
{
    try
    {
        require(renderer && instance && session && space && getProc && vkInstance && physical && device && queue,
            "Projection requires live borrowed renderer, XR and Vulkan handles");
        std::unique_ptr<Projection> p(new Projection{});
        p->renderer = reinterpret_cast<ovrtx_renderer_t*>(renderer);
        p->instance = reinterpret_cast<XrInstance>(instance); p->session = reinterpret_cast<XrSession>(session);
        p->space = reinterpret_cast<XrSpace>(space); p->getProc = reinterpret_cast<PFN_xrGetInstanceProcAddr>(getProc);
        p->vkInstance = reinterpret_cast<VkInstance>(vkInstance); p->physical = reinterpret_cast<VkPhysicalDevice>(physical);
        p->device = reinterpret_cast<VkDevice>(device); p->queue = reinterpret_cast<VkQueue>(queue);
        p->initialize(family);
        return p.release();
    }
    catch (const std::exception& e) { sampleError = e.what(); return nullptr; }
}
int sample_projection_set_views(void* handle, const SampleProjectionView* views, uint32_t count)
{
    try
    {
        require(handle && views && count == 2, "Projection requires two rendered views");
        auto& p = *static_cast<Projection*>(handle);
        p.ready = {};
        for (size_t i = 0; i < 2; ++i)
        {
            p.views[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            std::memcpy(&p.views[i].pose, &views[i].pose, sizeof(SamplePose));
            std::memcpy(&p.views[i].fov, views[i].fov, sizeof(views[i].fov));
        }
        return 0;
    }
    catch (const std::exception& e) { sampleError = e.what(); return -1; }
}
int sample_projection_read(void* handle, ovrtx_step_result_handle_t step, const char* product, SampleImage* color, SampleImage* depth)
{
    try
    {
        require(handle && step != OVRTX_INVALID_STEP_RESULT_HANDLE && product && color && depth, "Invalid borrowed frame output arguments");
        auto& p = *static_cast<Projection*>(handle);
        *color = {}; *depth = {};
        ovrtx_render_product_set_outputs_t outputs{};
        rtCheck(ovrtx_fetch_results(p.renderer, step, {~uint64_t(0)}, &outputs).status);
        require(outputs.status == OVRTX_EVENT_COMPLETED, "Bound frame outputs are incomplete");
        for (size_t i = 0; i < outputs.output_count; ++i)
        {
            const auto& output = outputs.outputs[i];
            if (text(output.render_product_path) != product || !output.output_frame_count) continue;
            const auto& frame = output.output_frames[output.output_frame_count - 1];
            for (size_t j = 0; j < frame.render_var_count; ++j)
            {
                const auto& var = frame.output_render_vars[j];
                if (text(var.source_name) == "LdrColor") p.copyOutput(var.output_handle, false, *color);
                if (text(var.source_name) == "XRDepth") p.copyOutput(var.output_handle, true, *depth);
            }
        }
        require(color->rgba && depth->rgba, "Bound eye is missing LdrColor or XRDepth");
        return 0;
    }
    catch (const std::exception& e) { sampleError = e.what(); return -1; }
}
int sample_projection_upload(void* handle, uint32_t eye, const void* rgba, const void* depth,
    uint32_t width, uint32_t height, float nearZ, float farZ)
{
    try
    {
        require(handle && eye < 2 && rgba && depth && width && height && width <= 8192 && height <= 8192 &&
            nearZ > 0 && farZ > nearZ, "Invalid projection upload arguments");
        auto& p = *static_cast<Projection*>(handle);
        p.configure(p.chains[eye * 2], width, height, false, false, true);
        p.configure(p.chains[eye * 2 + 1], width, height, true);
        p.upload(p.chains[eye * 2], rgba); p.upload(p.chains[eye * 2 + 1], depth);
        p.views[eye].subImage = {p.chains[eye * 2].handle, {{0, 0}, {int32_t(width), int32_t(height)}}, 0};
        auto& d = p.depths[eye];
        d = {XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR};
        d.subImage = p.views[eye].subImage; d.subImage.swapchain = p.chains[eye * 2 + 1].handle;
        d.minDepth = 0; d.maxDepth = 1; d.nearZ = farZ; d.farZ = nearZ; // reverse Z
        p.views[eye].next = &d; p.ready[eye] = true;
        return 0;
    }
    catch (const std::exception& e) { sampleError = e.what(); return -1; }
}
const void* sample_projection_layer(void* handle)
{
    if (!handle) return nullptr;
    auto& p = *static_cast<Projection*>(handle);
    return p.ready[0] && p.ready[1] ? &p.layer : nullptr;
}
void sample_projection_destroy(void* handle) { delete static_cast<Projection*>(handle); }

const void* sample_quad_upload(void* handle, const void* rgba, void* readback,
    uint32_t width, uint32_t height, uint32_t headLocked)
{
    try
    {
        require(handle && rgba && width && height && width <= 8192 && height <= 8192 && headLocked <= 1,
            "Invalid quad upload arguments");
        auto& p = *static_cast<Projection*>(handle);
        if (headLocked && !p.viewSpace)
        {
            XrReferenceSpaceCreateInfo info{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
            info.poseInReferenceSpace.orientation.w = 1;
            xrCheck(p.xrFunction<PFN_xrCreateReferenceSpace>("xrCreateReferenceSpace")(p.session, &info, &p.viewSpace));
        }
        // OpenXR reuses the last released image until its contents change.
        // Explicit readback verification keeps releasing every probe frame,
        // including after the test compositor arms a composed capture.
        if (!p.chains[0].handle || p.quadUploadCache.needsUpload(rgba, width, height, readback != nullptr))
        {
            p.configure(p.chains[0], width, height, false, readback != nullptr);
            p.upload(p.chains[0], rgba, readback);
            p.quadUploadCache.record(rgba, width, height);
        }
        p.quad = {XR_TYPE_COMPOSITION_LAYER_QUAD};
        p.quad.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT |
            XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
        p.quad.space = headLocked ? p.viewSpace : p.space;
        p.quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
        p.quad.subImage = {p.chains[0].handle, {{0, 0}, {int32_t(width), int32_t(height)}}, 0};
        p.quad.pose.orientation.w = 1;
        p.quad.pose.position = {0, headLocked ? -0.2f : 1.5f, -1.0f};
        p.quad.size = {0.8f, 0.8f * float(height) / float(width)};
        return &p.quad;
    }
    catch (const std::exception& e) { sampleError = e.what(); return nullptr; }
}
