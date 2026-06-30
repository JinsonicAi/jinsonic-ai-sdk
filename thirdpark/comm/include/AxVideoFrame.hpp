#ifndef _HW_AXVIDEOFRAME_HPP_
#define _HW_AXVIDEOFRAME_HPP_

#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>
#define FMT_HEADER_ONLY
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>

#include "AxSysLifecycle.hpp"
#include "VFrameTypes.hpp"
#include "ax_global_type.h"
#ifndef NO_CONTEXT_MANAGER
#include "contextManager.hpp"
#endif
#define VERIFY_PATH "/usr/local/aibox/verify"

class EXPORT_VISIBILITY AXVideoFrame : public std::enable_shared_from_this<AXVideoFrame> {
public:
	// MemCopy source memory domain tags:
	//   -1: host CPU virtual memory, matching the public host device_id convention.
	//   >=0: AXCL device physical address on the specified logical device.
	// Host physical addresses require the explicit SRC_HOST_PHY tag.
	// Keep SRC_CPU_VIRT=-2 for source compatibility with existing callers.
	static constexpr int SRC_CPU_VIRT = -2;	// src_ptr is CPU virtual address (e.g. cv::Mat::data)
	static constexpr int SRC_HOST_PHY = -3;	// src_ptr encodes a local-SoC host physical address
	static constexpr int SRC_RK_MPP   = -4;	// src_ptr is owned by an RK MPP/RGA native frame

	AXVideoFrame(uint32_t width, uint32_t height, int device_id = 0, AX_IMG_FORMAT_E format = AX_FORMAT_YUV420_SEMIPLANAR, AX_U32 nAlign = 16);
	AXVideoFrame(uint32_t width, uint32_t height, int device_id, AX_U32 size);
	AXVideoFrame(const RkFrameNative& rk_frame, std::function<void(const RkFrameNative&)> fin = {});
	explicit AXVideoFrame(const RkFrameCreateOptions& rk_options);
	static std::shared_ptr<AXVideoFrame> createRK(uint32_t width, uint32_t height,
												  AX_IMG_FORMAT_E format = AX_FORMAT_YUV420_SEMIPLANAR,
												  AX_U32 nAlign = 16);
	// Linear buffer (AX_FORMAT_BITMAP semantics): allocates an RK dma-buf of `size` bytes, with no format/alignment constraints.
	// Corresponds to the AX-side AXVideoFrame(w,h,device_id,size); used for H264/H265 bitstreams, JPEG, and other arbitrary-size data.
	static std::shared_ptr<AXVideoFrame> createRK(uint32_t width, uint32_t height, AX_U32 size);
	// fin(vf) will be automatically called upon destruction to perform the corresponding return/release.
	AXVideoFrame(const AX_VIDEO_FRAME_INFO_T&					   vf,
				 std::function<void(const AX_VIDEO_FRAME_INFO_T&)> fin, int device_id);
	AXVideoFrame(uint32_t width, uint32_t height, int device_id, const AX_VENC_STREAM_T& stream,
				 std::function<void(const AX_VENC_STREAM_T&)> fin);

	// move-only, prevents double release
	AXVideoFrame(AXVideoFrame&&) noexcept;
	AXVideoFrame& operator=(AXVideoFrame&&) noexcept;
	AXVideoFrame(const AXVideoFrame&)			 = delete;
	AXVideoFrame& operator=(const AXVideoFrame&) = delete;

	~AXVideoFrame();

	std::shared_ptr<AXVideoFrame> toHost();	 // to host
	std::shared_ptr<AXVideoFrame> clone() const;

	int		 fill(uint8_t value, int x = 0, int y = 0);
	// src_device_id:
	//   >=0: source is device physical memory on that logical device
	//   -1 or SRC_CPU_VIRT(-2): source is a CPU virtual pointer
	//   SRC_HOST_PHY(-3): source is a local-SoC host physical address
	int		 CopyFrom(const uint8_t* src, int size, int offset = 0, int src_device_id = -1);
	// int		 MemCopy(const uint8_t* nalu, int nalu_size, int offset = 0, int src_device_id = -1);
	// Copy frame content out to a CPU buffer. RK frames perform the required
	// dma-buf cache sync internally; AX/AXCL frames keep the existing toHost fallback.
	int		 CopyTo(void* dst, int size, int offset = 0);
	AX_U64	 getPhyaddr();
	AX_VOID* getPviraddr();

	void load_data(const std::string& filename, int length = -1);
	void save_data(const std::string& filename, int length = -1);

	operator AX_VIDEO_FRAME_T&();
	AX_VIDEO_FRAME_T*		raw() noexcept;
	const AX_VIDEO_FRAME_T* raw() const noexcept;
	AX_VIDEO_FRAME_T*		axRaw() noexcept;
	const AX_VIDEO_FRAME_T* axRaw() const noexcept;

	VFrameBackend		backend() const;
	const char*			backendName() const;
	VFrameMemoryDomain memoryDomain() const;
	bool				cpuAccessible() const;
	bool				isAXFrame() const;
	bool				isRKFrame() const;
	int				dmaFd() const;
	bool				syncForCpuRead();
	void*				nativeHandle(VFrameNativeHandleType type);
	const void*			nativeHandle(VFrameNativeHandleType type) const;

	int&	  size();
	uint32_t& width();
	int		  height() const;
	uint32_t& stride();
	int		  device_id() const;
	bool	  isHost() const;
	void*&	  context();

public:	 // encode
	int				  enCodingType{0};
	AX_U64			  pts{0};
	AX_PAYLOAD_TYPE_E enType{PT_BUTT};

private:
	class Impl;
	AXVideoFrame(uint32_t width, uint32_t height, int device_id, AX_IMG_FORMAT_E format, AX_U32 nAlign, bool pool_backed);
	std::unique_ptr<Impl> impl_;
};

using VFrame = AXVideoFrame;

#endif
