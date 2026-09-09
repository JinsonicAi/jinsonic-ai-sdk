#pragma once

#include <cstdint>
#include <limits>

// Private hand-off between the RK OSD composer and RKIvps.  AX/AXCL physical
// addresses retain their original meaning; only an exact RKOS tag is decoded
// as a dma-buf descriptor.  Keeping the tag in the existing attribute avoids
// changing the public AX_OSD_BMP_ATTR_T ABI.
namespace aibox::rk_osd_dma {

inline constexpr std::uint64_t kTagMask = 0xffffffff00000000ULL;
inline constexpr std::uint64_t kTag = 0x524b4f5300000000ULL;  // "RKOS"

inline std::uint64_t encode_fd(int fd) noexcept {
	return fd > 0 ? (kTag | static_cast<std::uint32_t>(fd)) : 0;
}

inline int decode_fd(std::uint64_t value) noexcept {
	if ((value & kTagMask) != kTag) return -1;
	const std::uint32_t raw = static_cast<std::uint32_t>(value);
	if (raw == 0 || raw > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
		return -1;
	}
	return static_cast<int>(raw);
}

}  // namespace aibox::rk_osd_dma
