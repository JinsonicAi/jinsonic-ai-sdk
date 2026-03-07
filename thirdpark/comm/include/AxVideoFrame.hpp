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
#include "ax_global_type.h"
#ifndef NO_CONTEXT_MANAGER
#include "contextManager.hpp"
#endif
#define VERIFY_PATH "/usr/local/aibox/verify"

class EXPORT_VISIBILITY AXVideoFrame : public std::enable_shared_from_this<AXVideoFrame> {
public:
	// MemCopy source memory domain tags
	// Keep backward compatibility with legacy callsites:
	//   -1 -> host physical source
	//   -2 -> CPU virtual source
	static constexpr int SRC_HOST_PHY = -1;	// src_ptr encodes host physical address (reinterpret_cast<const uint8_t*>(phy))
	static constexpr int SRC_CPU_VIRT = -2;	// src_ptr is CPU virtual address (e.g. cv::Mat::data)

	AXVideoFrame(uint32_t width, uint32_t height, int device_id = 0, AX_IMG_FORMAT_E format = AX_FORMAT_YUV420_SEMIPLANAR, AX_U32 nAlign = 16);
	AXVideoFrame(uint32_t width, uint32_t height, int device_id, AX_U32 size);
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
	//   >=0: source is device physical memory on that device
	//   SRC_HOST_PHY(-1): source is host physical address encoded in src_ptr
	//   SRC_CPU_VIRT(-2) or other negative values: source is CPU virtual pointer
	int		 MemCopy(const uint8_t* nalu, int nalu_size, int offset = 0, int src_device_id = 0);
	AX_U64	 getPhyaddr();
	AX_VOID* getPviraddr();

	void load_data(const std::string& filename, int length = -1);
	void save_data(const std::string& filename, int length = -1);

	operator AX_VIDEO_FRAME_T&();
	AX_VIDEO_FRAME_T*		raw() noexcept;
	const AX_VIDEO_FRAME_T* raw() const noexcept;

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
	std::unique_ptr<Impl> impl_;
};

#endif
