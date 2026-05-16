#ifndef __HW_IVPS_HPP__
#define __HW_IVPS_HPP__
#pragma once

#include <signal.h>
#include <stdio.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "AxVideoFrame.hpp"
#include "HwCover.hpp"
#include "IIvps.hpp"
#include "ax_global_type.h"
#include "ax_ivps_type.h"
#include "ax_pool_type.h"
#include "sample_ivps_sync_api.h"

class HwIvps {
public:
	HwIvps(int device_id, int group, int channel);
	~HwIvps();

	int							  HwDrawRect(const AX_VIDEO_FRAME_T *ptSrcFrame, AX_IVPS_RECT_T tRect, AX_U32 nColor = RED);
	int							  HwDrawOsd(AX_VIDEO_FRAME_T *ptSrcFrame, const AX_OSD_BMP_ATTR_T arrBmp[], AX_U32 nNum);
	int							  HwDrawOsd(AX_VIDEO_FRAME_T *ptSrcFrame, std::pair<int, int> dst, const std::string &utf8_text, int font_size, cv::Scalar font_color, int bg_alpha = 0 /*0~255 */);
	std::shared_ptr<AXVideoFrame> HwIvpsCsc(const AX_VIDEO_FRAME_T *ptSrcFrame, AX_IMG_FORMAT_E enImgFormat = AX_FORMAT_RGB888, IVPS_ENGINE_ID_E eEngineId = IVPS_ENGINE_ID_VPP);
	std::shared_ptr<AXVideoFrame> HwIvpsDewarp(const AX_VIDEO_FRAME_T *ptSrcFrame, std::pair<int, int> Size, TransformMatrices &tm,
										 const ResizeOptions &options = ResizeOptions{});
	std::shared_ptr<AXVideoFrame> HwIvpsCropResize(AX_VIDEO_FRAME_T *ptSrcFrame, std::pair<int, int> Size, AX_IVPS_RECT_T crop, IVPS_ENGINE_ID_E eEngineId = IVPS_ENGINE_ID_VPP,
											 const ResizeOptions &options = ResizeOptions{});

private:
	int					   device_id_;
	int					   group_;
	int					   channel_id_;
	std::shared_ptr<IIvps> ivps_;
};
#endif
