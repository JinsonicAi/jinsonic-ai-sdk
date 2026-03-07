#ifndef __HDMI_HPP__
#define __HDMI_HPP__

#include "ax_global_type.h"

bool HdmiSendFrame(int chn, std::string name, AX_VIDEO_FRAME_T& frame);

#endif	// __HDMI_HPP__