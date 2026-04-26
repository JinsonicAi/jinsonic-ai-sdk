#ifndef _AX_HWDECODER_H_
#define _AX_HWDECODER_H_

#include <signal.h>
#include <stdio.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "IDecoder.hpp"
#include "streamInfo.hpp"

class EXPORT_VISIBILITY HwDecoder {
public:
	HwDecoder(int device_id, int group, int channel, stream_info info);
	~HwDecoder();

	std::shared_ptr<AXVideoFrame> Decode(const uint8_t* nalu, size_t nalu_size);
	bool						  NeedKeyFrameSync() const;
	bool						  NeedReset() const;
	bool                          IsReady() const;
	// int							  sendFrame(const uint8_t* nalu, size_t nalu_size);
	// std::shared_ptr<AXVideoFrame> getFrame();

private:
	stream_info				  info_;
	int						  device_id_;
	int						  group_;
	int						  channel_id_;
	std::shared_ptr<IDecoder> decoder_;
};

#endif
