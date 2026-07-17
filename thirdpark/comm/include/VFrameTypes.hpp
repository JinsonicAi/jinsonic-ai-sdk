#pragma once

#include <cstdint>

enum class VFrameBackend {
	Unknown = 0,
	Host,
	AxLocal,
	Axcl,
	Rk,
};

enum class VFrameMemoryDomain {
	Unknown = 0,
	CpuVirt,
	HostPhy,
	AxLocalPhy,
	AxclDevice,
	RkMpp,
	DmaBuf,
};

enum class VFrameNativeHandleType {
	AxVideoFrame = 0,
	AxVencStream,
	RkMppFrame,
	RkRgaBuffer,
	DmaBuf,
	CpuVirt,
};

struct RkFrameNative {
	uint32_t width{0};
	uint32_t height{0};
	uint32_t stride{0};
	uint32_t vstride{0};
	uint32_t size{0};
	int		 format{0};
	int		 dma_fd{-1};
	uint64_t phy_addr{0};
	void*	 vir_addr{nullptr};
	void*	 mpp_frame{nullptr};
	void*	 mpp_buffer{nullptr};
	void*	 mpp_group{nullptr};
	void*	 rga_buffer{nullptr};
	void*	 user{nullptr};
	uint64_t pts{0};
	uint64_t seq{0};
	bool	 read_only{false};
	bool	 borrowed{true};
};

struct RkFrameCreateOptions {
	uint32_t width{0};
	uint32_t height{0};
	int		 format{0};
	uint32_t align{16};
	uint32_t stride{0};
	uint32_t vstride{0};
	uint32_t size{0};
	bool	 cpu_mapped{true};
	// raw_linear=true 时分配一块 size 字节的线性 dma-buf（不按图像格式计算大小），
	// 用于 H264/H265 码流、JPEG 等任意大小、无对齐要求的数据。
	// 对齐 AX 侧 AXVideoFrame(w,h,device_id,size) 的 AX_FORMAT_BITMAP 语义。
	bool raw_linear{false};
};

inline const char* vframe_backend_name(VFrameBackend backend) {
	switch (backend) {
	case VFrameBackend::Host:
		return "host";
	case VFrameBackend::AxLocal:
		return "ax.local";
	case VFrameBackend::Axcl:
		return "axcl";
	case VFrameBackend::Rk:
		return "rk.local";
	default:
		return "unknown";
	}
}
