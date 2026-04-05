#include "renderer/Skybox.h"
#include "core/Logger.h"

#include <fstream>
#include <stdexcept>
#include <cstring>
#include <cmath>
#include <array>

namespace Genesis {

// ─── Helper: read SPIR-V file ───
static std::vector<char> readFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + filepath);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    return buffer;
}

static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }
    return module;
}

// ─── Helper: one-shot command buffer ───
static VkCommandBuffer beginOneShot(VkDevice device, VkCommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool        = pool;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &allocInfo, &cmd);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    return cmd;
}

static void endOneShot(VkDevice device, VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submitInfo{};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmd;
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, pool, 1, &cmd);
}

// ─── Generate procedural sky color for a given direction ───
struct Color3 { uint8_t r, g, b; };

static Color3 skyColor(float x, float y, float z) {
    // Normalize direction
    float len = std::sqrt(x * x + y * y + z * z);
    if (len > 0.0f) { x /= len; y /= len; z /= len; }

    // y = up/down direction
    float t = y * 0.5f + 0.5f; // 0 = down, 1 = up

    // Ground: dark brownish
    // Horizon: warm orange/white
    // Sky: deep blue to light blue

    float r, g, b;
    if (t < 0.48f) {
        // Below horizon — dark ground
        float gt = t / 0.48f;
        r = 0.15f + gt * 0.15f;
        g = 0.12f + gt * 0.13f;
        b = 0.10f + gt * 0.10f;
    } else if (t < 0.52f) {
        // Horizon band — warm glow
        float ht = (t - 0.48f) / 0.04f;
        r = 0.30f + ht * 0.50f;
        g = 0.25f + ht * 0.40f;
        b = 0.20f + ht * 0.30f;
    } else {
        // Sky — gradient from light blue to deep blue
        float st = (t - 0.52f) / 0.48f;
        r = 0.80f - st * 0.55f;
        g = 0.65f - st * 0.30f;
        b = 0.50f + st * 0.45f;
    }

    return {
        static_cast<uint8_t>(std::fmin(r, 1.0f) * 255.0f),
        static_cast<uint8_t>(std::fmin(g, 1.0f) * 255.0f),
        static_cast<uint8_t>(std::fmin(b, 1.0f) * 255.0f)
    };
}

void Skybox::init(VkDevice device, VkPhysicalDevice physicalDevice,
                   VkCommandPool commandPool, VkQueue queue,
                   VkRenderPass renderPass,
                   const std::vector<VkBuffer>& viewProjBuffers,
                   VkDeviceSize viewProjSize, u32 framesInFlight) {
    createCubemap(device, physicalDevice, commandPool, queue);
    createMesh(device, physicalDevice, commandPool, queue);
    createDescriptors(device, framesInFlight, viewProjBuffers, viewProjSize);
    createPipeline(device, renderPass);
    Logger::info("Skybox initialized");
}

void Skybox::shutdown(VkDevice device) {
    if (m_pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, m_pipeline, nullptr);
    if (m_pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
    if (m_descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
    if (m_descriptorLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, m_descriptorLayout, nullptr);
    if (m_cubemapSampler != VK_NULL_HANDLE) vkDestroySampler(device, m_cubemapSampler, nullptr);
    if (m_cubemapView != VK_NULL_HANDLE) vkDestroyImageView(device, m_cubemapView, nullptr);
    if (m_cubemapImage != VK_NULL_HANDLE) vkDestroyImage(device, m_cubemapImage, nullptr);
    if (m_cubemapMemory != VK_NULL_HANDLE) vkFreeMemory(device, m_cubemapMemory, nullptr);
    VulkanBuffer::destroy(device, m_vertexBuffer);
    VulkanBuffer::destroy(device, m_indexBuffer);
    Logger::info("Skybox destroyed");
}

void Skybox::render(VkCommandBuffer cmd, u32 currentFrame) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkDescriptorSet set = m_descriptorSets[currentFrame];
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipelineLayout, 0, 1, &set, 0, nullptr);

    VkBuffer vertexBuffers[] = { m_vertexBuffer.buffer };
    VkDeviceSize offsets[]    = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmd, m_indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);
}

void Skybox::createCubemap(VkDevice device, VkPhysicalDevice physicalDevice,
                            VkCommandPool commandPool, VkQueue queue) {
    const u32 faceSize = 128;
    const u32 pixelsPerFace = faceSize * faceSize;
    const VkDeviceSize faceBytes = pixelsPerFace * 4;
    const VkDeviceSize totalBytes = faceBytes * 6;

    // Generate 6 face textures
    // Cubemap face order: +X, -X, +Y, -Y, +Z, -Z
    std::vector<uint8_t> pixels(totalBytes);

    for (u32 face = 0; face < 6; face++) {
        for (u32 y = 0; y < faceSize; y++) {
            for (u32 x = 0; x < faceSize; x++) {
                // Map pixel (x,y) to a 3D direction for this face
                float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(faceSize) * 2.0f - 1.0f;
                float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(faceSize) * 2.0f - 1.0f;

                float dx, dy, dz;
                switch (face) {
                    case 0: dx =  1.0f; dy = -v;    dz = -u;    break; // +X
                    case 1: dx = -1.0f; dy = -v;    dz =  u;    break; // -X
                    case 2: dx =  u;    dy =  1.0f; dz =  v;    break; // +Y
                    case 3: dx =  u;    dy = -1.0f; dz = -v;    break; // -Y
                    case 4: dx =  u;    dy = -v;    dz =  1.0f; break; // +Z
                    default: dx = -u;   dy = -v;    dz = -1.0f; break; // -Z
                }

                Color3 c = skyColor(dx, dy, dz);
                u32 idx = (face * pixelsPerFace + y * faceSize + x) * 4;
                pixels[idx + 0] = c.r;
                pixels[idx + 1] = c.g;
                pixels[idx + 2] = c.b;
                pixels[idx + 3] = 255;
            }
        }
    }

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VulkanBuffer::createBuffer(device, physicalDevice, totalBytes,
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               stagingBuffer, stagingMemory);

    void* data;
    vkMapMemory(device, stagingMemory, 0, totalBytes, 0, &data);
    memcpy(data, pixels.data(), static_cast<size_t>(totalBytes));
    vkUnmapMemory(device, stagingMemory);

    // Create cubemap image
    VkImageCreateInfo imageInfo{};
    imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType     = VK_IMAGE_TYPE_2D;
    imageInfo.extent        = { faceSize, faceSize, 1 };
    imageInfo.mipLevels     = 1;
    imageInfo.arrayLayers   = 6;
    imageInfo.format        = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &m_cubemapImage) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create cubemap image");
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, m_cubemapImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = VulkanBuffer::findMemoryType(
        physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &m_cubemapMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate cubemap memory");
    }
    vkBindImageMemory(device, m_cubemapImage, m_cubemapMemory, 0);

    // Transition to TRANSFER_DST
    {
        VkCommandBuffer cmd = beginOneShot(device, commandPool);

        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = m_cubemapImage;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 6;
        barrier.srcAccessMask                   = 0;
        barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        endOneShot(device, commandPool, queue, cmd);
    }

    // Copy each face from staging buffer to cubemap layer
    {
        VkCommandBuffer cmd = beginOneShot(device, commandPool);

        std::array<VkBufferImageCopy, 6> regions{};
        for (u32 face = 0; face < 6; face++) {
            regions[face].bufferOffset                    = face * faceBytes;
            regions[face].bufferRowLength                 = 0;
            regions[face].bufferImageHeight               = 0;
            regions[face].imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            regions[face].imageSubresource.mipLevel       = 0;
            regions[face].imageSubresource.baseArrayLayer = face;
            regions[face].imageSubresource.layerCount     = 1;
            regions[face].imageOffset                     = { 0, 0, 0 };
            regions[face].imageExtent                     = { faceSize, faceSize, 1 };
        }

        vkCmdCopyBufferToImage(cmd, stagingBuffer, m_cubemapImage,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               6, regions.data());

        endOneShot(device, commandPool, queue, cmd);
    }

    // Transition to SHADER_READ_ONLY
    {
        VkCommandBuffer cmd = beginOneShot(device, commandPool);

        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = m_cubemapImage;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 6;
        barrier.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        endOneShot(device, commandPool, queue, cmd);
    }

    // Clean up staging
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    // Create cubemap image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image                           = m_cubemapImage;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format                          = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 6;

    if (vkCreateImageView(device, &viewInfo, nullptr, &m_cubemapView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create cubemap image view");
    }

    // Create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter    = VK_FILTER_LINEAR;
    samplerInfo.minFilter    = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(device, &samplerInfo, nullptr, &m_cubemapSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create cubemap sampler");
    }

    Logger::info("Cubemap created (6 x {}x{} faces)", faceSize, faceSize);
}

void Skybox::createMesh(VkDevice device, VkPhysicalDevice physicalDevice,
                         VkCommandPool commandPool, VkQueue queue) {
    // Simple cube positions (no normals/UVs needed for skybox)
    float s = 1.0f;
    std::vector<float> vertices = {
        -s, -s,  s,    s, -s,  s,    s,  s,  s,   -s,  s,  s,  // front
         s, -s, -s,   -s, -s, -s,   -s,  s, -s,    s,  s, -s,  // back
        -s,  s,  s,    s,  s,  s,    s,  s, -s,   -s,  s, -s,  // top
        -s, -s, -s,    s, -s, -s,    s, -s,  s,   -s, -s,  s,  // bottom
         s, -s,  s,    s, -s, -s,    s,  s, -s,    s,  s,  s,  // right
        -s, -s, -s,   -s, -s,  s,   -s,  s,  s,   -s,  s, -s,  // left
    };

    std::vector<u32> indices;
    for (u32 face = 0; face < 6; face++) {
        u32 base = face * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }
    m_indexCount = static_cast<u32>(indices.size());

    m_vertexBuffer = VulkanBuffer::createVertexBuffer(
        device, physicalDevice, commandPool, queue,
        vertices.data(), sizeof(float) * vertices.size()
    );
    m_indexBuffer = VulkanBuffer::createIndexBuffer(
        device, physicalDevice, commandPool, queue,
        indices.data(), sizeof(u32) * indices.size()
    );
}

void Skybox::createDescriptors(VkDevice device, u32 framesInFlight,
                                const std::vector<VkBuffer>& viewProjBuffers,
                                VkDeviceSize viewProjSize) {
    // Layout: binding 0 = VP UBO (vertex), binding 1 = cubemap sampler (fragment)
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    bindings[0].binding         = 0;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    bindings[1].binding         = 1;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<u32>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_descriptorLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create skybox descriptor set layout");
    }

    // Pool
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight };
    poolSizes[1] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, framesInFlight };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = framesInFlight;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create skybox descriptor pool");
    }

    // Allocate sets
    std::vector<VkDescriptorSetLayout> layouts(framesInFlight, m_descriptorLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_descriptorPool;
    allocInfo.descriptorSetCount = framesInFlight;
    allocInfo.pSetLayouts        = layouts.data();

    m_descriptorSets.resize(framesInFlight);
    if (vkAllocateDescriptorSets(device, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate skybox descriptor sets");
    }

    // Update sets
    for (u32 i = 0; i < framesInFlight; i++) {
        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = viewProjBuffers[i];
        bufInfo.offset = 0;
        bufInfo.range  = viewProjSize;

        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfo.imageView   = m_cubemapView;
        imgInfo.sampler     = m_cubemapSampler;

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = m_descriptorSets[i];
        writes[0].dstBinding      = 0;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo     = &bufInfo;

        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = m_descriptorSets[i];
        writes[1].dstBinding      = 1;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].descriptorCount = 1;
        writes[1].pImageInfo      = &imgInfo;

        vkUpdateDescriptorSets(device, static_cast<u32>(writes.size()), writes.data(), 0, nullptr);
    }
}

void Skybox::createPipeline(VkDevice device, VkRenderPass renderPass) {
    auto vertCode = readFile("skybox.vert.spv");
    auto fragCode = readFile("skybox.frag.spv");
    VkShaderModule vertModule = createShaderModule(device, vertCode);
    VkShaderModule fragModule = createShaderModule(device, fragCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertModule;
    stages[0].pName  = "main";
    stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragModule;
    stages[1].pName  = "main";

    // Vertex input: just vec3 position (no normal, no UV)
    VkVertexInputBindingDescription binding{};
    binding.binding   = 0;
    binding.stride    = sizeof(float) * 3;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attr{};
    attr.binding  = 0;
    attr.location = 0;
    attr.format   = VK_FORMAT_R32G32B32_SFLOAT;
    attr.offset   = 0;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount    = 1;
    vertexInput.pVertexBindingDescriptions       = &binding;
    vertexInput.vertexAttributeDescriptionCount  = 1;
    vertexInput.pVertexAttributeDescriptions     = &attr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    // Rasterizer: render front faces (we're inside the cube)
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth   = 1.0f;
    rasterizer.cullMode    = VK_CULL_MODE_NONE; // Don't cull — we're inside the cube
    rasterizer.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Depth: test but don't write (skybox is always behind everything)
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable  = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE; // Never write to depth buffer
    depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL; // Pass at depth = 1.0

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                        | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments    = &colorBlendAttachment;

    // Pipeline layout
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts    = &m_descriptorLayout;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create skybox pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = stages;
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = m_pipelineLayout;
    pipelineInfo.renderPass          = renderPass;
    pipelineInfo.subpass             = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create skybox pipeline");
    }

    Logger::info("Skybox pipeline created");

    vkDestroyShaderModule(device, fragModule, nullptr);
    vkDestroyShaderModule(device, vertModule, nullptr);
}

} // namespace Genesis
