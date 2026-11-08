// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// NVIDIA DLSS (through NVIDIA Streamline) in place of a game's DLSS
// evaluation. NVIDIA's DLLs come from the user's PC; see vulkan_common/streamline.h.

#pragma once

#include <array>
#include <atomic>

#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class MemoryAllocator;
class Scheduler;

class StreamlineDlssUpscaler {
public:
    struct Image {
        VkImageView view;
        VkImage image;
        VkFormat format;
        VkImageSubresourceRange range;     ///< The view's level and layer, with the view's aspect.
        VkImageAspectFlags barrier_aspect; ///< Every aspect of the image's format.
    };

    struct Inputs {
        Image color;          ///< Jittered colour at the render size.
        Image depth;          ///< Device depth (reversed) at the render size.
        Image motion_vectors; ///< Motion to the previous frame at the render size.
        VkExtent2D render_size;
        VkExtent2D upscale_size;
        /// This frame's sample offset in render pixels, as the game hands its own DLSS.
        std::array<f32, 2> jitter;
        /// Turns a motion vector into render pixels (the render size for UV offsets).
        std::array<f32, 2> motion_vector_scale;
        bool reset;
        bool depth_inverted = true;
        bool hdr = true; ///< Linear HDR colour. False for tonemapped, perceptually encoded colour.
    };

    /// Throws vk::Exception when Streamline has no DLSS for this device.
    explicit StreamlineDlssUpscaler(const Device& device, Scheduler& scheduler,
                                    MemoryAllocator& memory_allocator);
    ~StreamlineDlssUpscaler();

    /// Records one evaluation into the output image (RGBA16F at the upscale size, GENERAL
    /// layout). Returns false when DLSS can't run, so the caller can fall back to FSR 3.1.
    bool Upscale(const Inputs& inputs);

    [[nodiscard]] bool Failed() const noexcept {
        return failed.load();
    }

    [[nodiscard]] VkImage OutputImage() const noexcept {
        return *output_image;
    }
    [[nodiscard]] VkImageView OutputView() const noexcept {
        return *output_view;
    }
    /// Nearest-neighbour clamp sampler (normalized coordinates), for reading the output.
    [[nodiscard]] VkSampler PointSampler() const noexcept {
        return *point_sampler;
    }

private:
    void CreateOutput(VkExtent2D upscale_size);
    /// The DLSS mode (sl::DLSSMode) for a render size: one whose render size range NVIDIA DLSS
    /// reports for this output takes it, nearest its optimal size; else by the ratio.
    [[nodiscard]] u32 PickMode(VkExtent2D render, VkExtent2D upscale);

    const Device& device;
    Scheduler& scheduler;
    MemoryAllocator& memory_allocator;
    void* set_options{}; ///< slDLSSSetOptions
    void* get_optimal_settings{}; ///< slDLSSGetOptimalSettings (optional)
    struct ModeRange {
        u32 mode{};
        u32 optimal_width{};
        u32 min_width{};
        u32 min_height{};
        u32 max_width{};
        u32 max_height{};
    };
    std::array<ModeRange, 5> mode_ranges{};
    size_t num_mode_ranges{};
    VkExtent2D ranges_output{}; ///< the output size mode_ranges belong to
    VkExtent2D render_extent{};
    VkExtent2D upscale_extent{};
    bool hdr_input = true;
    u32 applied_preset = ~0U; ///< sl::DLSSPreset of the last options
    u32 frame_index{};
    vk::Image output_image;
    vk::ImageView output_view;
    vk::Sampler point_sampler;
    std::atomic<bool> failed{};
};

} // namespace Vulkan
