#pragma once

#include <cstddef>
#include <memory>

#include "AxVideoFrame.hpp"

namespace jdk_plugin {

inline bool raw_frame_layout_reasonable(const AX_VIDEO_FRAME_T* frame) {
	if (!frame) return false;
	if (frame->u32Width < 2 || frame->u32Height < 2) return false;
	if (frame->u32Width > 8192 || frame->u32Height > 8192) return false;
	if (frame->u32FrameSize == 0 || frame->u32FrameSize > 512u * 1024u * 1024u) return false;
	if (frame->enImgFormat == AX_FORMAT_INVALID || frame->enImgFormat == AX_FORMAT_BITMAP) return false;
	if (frame->u32PicStride[0] != 0 && frame->u32PicStride[0] < frame->u32Width) return false;
	return true;
}

inline bool frame_has_device_memory(const std::shared_ptr<AXVideoFrame>& owner,
									const AX_VIDEO_FRAME_T* frame = nullptr) {
	const AX_VIDEO_FRAME_T* raw = frame ? frame : (owner ? owner->raw() : nullptr);
	if (!raw_frame_layout_reasonable(raw)) return false;

	// AX/AXCL capture and IVPS paths consume device/physical memory. RK MPP/RGA
	// frames are dma-buf backed and normally have phy=0, so dma-fd is the valid
	// device-memory handle for rk.local.
	if (raw->u64PhyAddr[0] != 0) return true;
	if (owner && owner->isRKFrame() && owner->dmaFd() >= 0) return true;
	return false;
}

inline bool frame_cpu_drawable_or_rk_dma(const std::shared_ptr<AXVideoFrame>& owner) {
	if (!owner || !owner->raw()) return false;
	if (owner->isRKFrame() && owner->dmaFd() >= 0) return true;
	return owner->raw()->u64VirAddr[0] != 0;
}

inline bool frame_has_host_memory(const std::shared_ptr<AXVideoFrame>& owner, size_t min_size = 1) {
	if (!owner || !owner->getPviraddr()) return false;
	if (owner->size() < 0) return false;
	return static_cast<size_t>(owner->size()) >= min_size;
}

inline bool same_frame_memory(const std::shared_ptr<AXVideoFrame>& lhs,
							  const std::shared_ptr<AXVideoFrame>& rhs) {
	if (!lhs || !rhs) return false;
	if (lhs.get() == rhs.get()) return true;
	const auto* lraw = lhs->raw();
	const auto* rraw = rhs->raw();
	if (!lraw || !rraw) return false;
	if (lhs->isRKFrame() || rhs->isRKFrame()) {
		const int lfd = lhs->dmaFd();
		const int rfd = rhs->dmaFd();
		if (lfd >= 0 && lfd == rfd) return true;
	}
	return lraw == rraw || (lraw->u64PhyAddr[0] != 0 && lraw->u64PhyAddr[0] == rraw->u64PhyAddr[0]);
}

}  // namespace jdk_plugin
