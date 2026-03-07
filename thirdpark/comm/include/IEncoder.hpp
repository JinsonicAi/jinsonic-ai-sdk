#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>

#include "AxVideoFrame.hpp"
#include "ax_global_type.h"

class IEncoder {
public:
	virtual ~IEncoder()																														  = default;
	virtual int							  Init()																							  = 0;
	virtual void						  Release()																							  = 0;
	virtual std::shared_ptr<AXVideoFrame> Encode(AX_VIDEO_FRAME_T* srcFrame, const uint8_t* nalu, size_t nalu_size, AX_PAYLOAD_TYPE_E enType) = 0;
};
