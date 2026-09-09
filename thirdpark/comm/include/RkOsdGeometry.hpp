#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

// Private, synchronous hand-off between JdkOsd and the RK RGA backend.
//
// The command is deliberately carried in AX_OSD_BMP_ATTR_T so the public
// IIvps/HwIvps ABI stays unchanged.  It is emitted only for an RK frame and is
// accepted only when the exact RKGB tag, payload magic, version and count all
// match.  AX/AXCL therefore retain their established bitmap/IVPS path.
namespace aibox::rk_osd_geometry {

inline constexpr std::uint64_t kTagMask = 0xffffffff00000000ULL;
inline constexpr std::uint64_t kTag = 0x524b474200000000ULL;  // "RKGB"
inline constexpr std::uint32_t kMagic = 0x524b4742U;
inline constexpr std::uint16_t kVersion = 6;
inline constexpr std::uint32_t kMaxBoxes = 256;
inline constexpr std::uint32_t kMaxLabels = 256;
inline constexpr std::uint32_t kMaxBaseBitmaps = 32;

struct BoxCommand {
	std::int32_t x{0};
	std::int32_t y{0};
	std::int32_t width{0};
	std::int32_t height{0};
	std::uint32_t rgb{0};  // 0xRRGGBB
	std::uint16_t thickness{0};
	std::uint16_t reserved{0};
};

// A cached label whose pixels may already live in immutable RK DMA memory.
// dma_fd > 0 selects the zero-copy path; bgra remains a bounded CPU fallback.
// The producer retains both owners for the complete synchronous HwDrawOsd call.
// Keeping content immutable while x/y change every video frame is the useful
// static/dynamic split: glyph pixels are uploaded once, but tracking positions
// are never cached or rate-limited.
struct LabelCommand {
	std::int32_t x{0};
	std::int32_t y{0};
	std::uint32_t width{0};
	std::uint32_t height{0};
	std::uint32_t stride_bytes{0};
	std::int32_t dma_fd{-1};
	std::uint32_t format{0};
	const std::uint8_t* bgra{nullptr};
};

// A direct RK DMA bitmap that must be composited below the realtime geometry.
// Grouping these inputs and the geometry canvas in one RGA job removes the
// second per-frame sync/lane acquisition without copying a full 1080p layer on
// the CPU.
struct BaseBitmapCommand {
	std::int32_t dma_fd{-1};
	std::int32_t x{0};
	std::int32_t y{0};
	std::uint32_t width{0};
	std::uint32_t height{0};
	std::uint32_t format{0};
	std::uint32_t reserved{0};
};

// Optional RK source frame for copy-on-write OSD.  Shared decode owns this
// dma-buf and the task owns a separate destination canvas.  Passing the source
// here lets RGA copy source -> destination and apply every bitmap in one
// hardware job, instead of issuing a separate synchronous full-frame copy
// before the in-place OSD pass.  The producer retains the source frame for the
// complete synchronous HwDrawOsd call.
struct SourceFrameCommand {
	std::int32_t dma_fd{-1};
	std::uint32_t width{0};
	std::uint32_t height{0};
	std::uint32_t format{0};
	std::uint32_t stride0{0};
	std::uint32_t stride1{0};
	// MPP commonly aligns a 1920x1080 NV12 frame to 1088 rows.  Height is the
	// visible image height; vstride is the physical Y-plane row count and must
	// survive this private hand-off so RGA locates the UV plane correctly.
	std::uint32_t vstride{0};
	std::uint32_t frame_size{0};
};

struct Batch {
	std::uint32_t magic{kMagic};
	std::uint16_t version{kVersion};
	std::uint16_t reserved{0};
	std::uint32_t count{0};
	std::uint32_t frame_width{0};
	std::uint32_t frame_height{0};
	const BoxCommand* boxes{nullptr};
	std::uint32_t label_count{0};
	std::uint32_t reserved2{0};
	const LabelCommand* labels{nullptr};
	std::uint32_t base_count{0};
	std::uint32_t reserved3{0};
	const BaseBitmapCommand* bases{nullptr};
	SourceFrameCommand source{};
};

static_assert(std::is_standard_layout_v<BoxCommand>);
static_assert(std::is_trivially_copyable_v<BoxCommand>);
static_assert(std::is_standard_layout_v<LabelCommand>);
static_assert(std::is_trivially_copyable_v<LabelCommand>);
static_assert(std::is_standard_layout_v<BaseBitmapCommand>);
static_assert(std::is_trivially_copyable_v<BaseBitmapCommand>);
static_assert(std::is_standard_layout_v<SourceFrameCommand>);
static_assert(std::is_trivially_copyable_v<SourceFrameCommand>);
static_assert(std::is_standard_layout_v<Batch>);
static_assert(std::is_trivially_copyable_v<Batch>);

inline std::uint64_t encode_count(std::uint32_t count) noexcept {
	return count > 0 && count <= kMaxBoxes ? (kTag | count) : 0;
}

inline std::uint32_t decode_count(std::uint64_t value) noexcept {
	if ((value & kTagMask) != kTag) return 0;
	const std::uint32_t count = static_cast<std::uint32_t>(value);
	return count > 0 && count <= kMaxBoxes ? count : 0;
}

inline bool is_tagged(std::uint64_t value) noexcept {
	return (value & kTagMask) == kTag;
}

}  // namespace aibox::rk_osd_geometry
