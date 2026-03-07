#pragma once

#include <cstddef>
#include <cstdint>

#include "ax_global_type.h"
#include "streamInfo.hpp"
//
#include "AxVideoFrame.hpp"

class IDecoder {
public:
	virtual ~IDecoder()																	= default;
	virtual int							  Init()										= 0;
	virtual void						  Release()										= 0;
	virtual std::shared_ptr<AXVideoFrame> Decode(const uint8_t* nalu, size_t nalu_size) = 0;
	// virtual int							  sendFrame(const uint8_t* nalu, size_t nalu_size) = 0;
	// virtual std::shared_ptr<AXVideoFrame> getFrame()									   = 0;
};
