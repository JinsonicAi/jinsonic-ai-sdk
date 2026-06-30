#ifndef _IIVPS_HPP_
#define _IIVPS_HPP_

#include <signal.h>
#include <stdio.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "ax_pool_type.h"
//
#include "AxVideoFrame.hpp"
#include "HwColor.hpp"
#include "HwCover.hpp"
#include "ResizeOptions.hpp"
#include "ax_global_type.h"
#include "ax_ivps_api.h"
#include "ax_ivps_type.h"
#include "sample_ivps_sync_api.h"

class IIvps {
public:
	virtual ~IIvps() = default;

	virtual int	 Init()	   = 0;
	virtual void Release() = 0;

	virtual int							  HwDrawRect(const AX_VIDEO_FRAME_T *ptSrcFrame, AX_IVPS_RECT_T tRect, AX_U32 nColor = RED, AX_BOOL bSolid = AX_FALSE)												= 0;
	virtual int							  HwDrawLine(const AX_VIDEO_FRAME_T *ptSrcFrame, const AX_IVPS_POINT_T points[], AX_U32 pointNum, AX_U32 nColor = RED, AX_U16 thickness = 2, AX_U16 alpha = 255)	= 0;
	virtual int							  HwDrawPolygon(const AX_VIDEO_FRAME_T *ptSrcFrame, const AX_IVPS_POINT_T points[], AX_U32 pointNum, AX_U32 nColor = RED, AX_U16 thickness = 2, AX_U16 alpha = 255, AX_BOOL bSolid = AX_FALSE) = 0;
	virtual int							  HwDrawOsd(AX_VIDEO_FRAME_T *ptSrcFrame, const AX_OSD_BMP_ATTR_T arrBmp[], AX_U32 nNum)																			= 0;
	virtual int							  HwDrawOsd(AX_VIDEO_FRAME_T *ptSrcFrame, std::pair<int, int> dst, const std::string &utf8_text, int font_size, cv::Scalar font_color, int bg_alpha = 0 /*0~255 */) = 0;
	virtual std::shared_ptr<AXVideoFrame> HwIvpsCsc(const AX_VIDEO_FRAME_T *ptSrcFrame, AX_IMG_FORMAT_E enImgFormat = AX_FORMAT_RGB888, IVPS_ENGINE_ID_E eEngineId = IVPS_ENGINE_ID_VPP)					= 0;
	virtual std::shared_ptr<AXVideoFrame> HwIvpsDewarp(const AX_VIDEO_FRAME_T *ptSrcFrame, std::pair<int, int> Size, TransformMatrices &tm,
													 const ResizeOptions &options = ResizeOptions{})														= 0;	// 通用透视/仿射变换：直接传入外部已算好的 3x3 逆映射矩阵(行优先, dst->src, 浮点)，
	// 内部转定点(×1e6)喂给 GDC，跳过 CalculateTransformMatrices。matrix[6..8] 仿射时为 {0,0,1}。
	virtual std::shared_ptr<AXVideoFrame> HwIvpsDewarpMatrix(const AX_VIDEO_FRAME_T *ptSrcFrame, std::pair<int, int> Size, const double (&matrix)[9],
																   const ResizeOptions &options = ResizeOptions{})									= 0;	virtual std::shared_ptr<AXVideoFrame> HwIvpsCropResize(AX_VIDEO_FRAME_T *ptSrcFrame, std::pair<int, int> Size, AX_IVPS_RECT_T crop, IVPS_ENGINE_ID_E eEngineId = IVPS_ENGINE_ID_VPP,
														 const ResizeOptions &options = ResizeOptions{})												= 0;
};
#endif
