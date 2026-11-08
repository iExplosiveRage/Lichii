// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <bit>

#include "common/common_types.h"

// The Legend of Zelda: Ocarina of Time renders its 3D at a scale its dynamic resolution
// controller picks; the built-in Max Resolution patch takes the controller's "disabled" path,
// which returns 1.0 (main+0x806420, mov w9, #0x3f800000). The built-in render scale patch sends
// that path through a cave that reads the scale from an unused word at the end of the game's
// BSS (0 keeps 1.0), so the emulator can pick a lower scale while the game runs, and NVIDIA DLSS
// upscales the scene's sub-rectangle back to the game's image.
// Offsets belong to the game's main with build id E0F3CCFAF50065AE4EA01EB5AEB13B1A45DA3348.
namespace Core::OcarinaRenderScale {

constexpr u64 ProgramId = 0x0400486024DEC000ULL;
constexpr u64 SiteOffset = 0x806420;  ///< mov w9, #0x3f800000 in the "disabled" path
constexpr u64 CaveOffset = 0x3C46300; ///< executable padding after .text
constexpr u64 StateOffset = 0x4E9FE20; ///< BSS word, no code references its page

/// b CaveOffset (at SiteOffset).
constexpr std::array<u8, 4> SiteBranch{0xB8, 0xFF, 0xD0, 0x14};
constexpr std::array<u8, 4> SiteOriginal{0x09, 0xF0, 0xA7, 0x52};
/// adrp x9, state page; ldr w9, [x9, #0xe20]; cbz w9, +12; mov x8, #0; b main+0x806000;
/// mov w9, #0x3f800000; b main+0x806000. With a scale set it also returns mode 0 (what the
/// dynamic resolution controller returns), so the game applies the scale every frame.
constexpr std::array<u8, 28> Cave{0xC9, 0x92, 0x00, 0xB0, 0x29, 0x21, 0x4E, 0xB9, 0x69, 0x00,
                                  0x00, 0x34, 0x08, 0x00, 0x80, 0xD2, 0x3C, 0xFF, 0x2E, 0x17,
                                  0x09, 0xF0, 0xA7, 0x52, 0x3A, 0xFF, 0x2E, 0x17};

[[nodiscard]] constexpr u32 SiteBranchWord() {
    return u32{SiteBranch[0]} | (u32{SiteBranch[1]} << 8) | (u32{SiteBranch[2]} << 16) |
           (u32{SiteBranch[3]} << 24);
}

/// The word to store for a scale. 0 leaves the game's own path (full scale, applied only when
/// its renderer starts); a scale, 1.0 included, is applied from the next frame.
[[nodiscard]] inline u32 StateWord(float scale) {
    if (scale <= 0.0f) {
        return 0;
    }
    return std::bit_cast<u32>(scale >= 1.0f ? 1.0f : scale);
}

} // namespace Core::OcarinaRenderScale
