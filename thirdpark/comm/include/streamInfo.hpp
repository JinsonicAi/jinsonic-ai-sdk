/**************************************************************************************************
 *
 * Copyright (c) 2019-2024 Axera Semiconductor Co., Ltd. All Rights Reserved.
 *
 * This source file is the property of Axera Semiconductor Co., Ltd. and
 * may not be copied or distributed in any isomorphic form without the prior
 * written consent of Axera Semiconductor Co., Ltd.
 *
 **************************************************************************************************/

#ifndef __STREAM_INFO_HPP__
#define __STREAM_INFO_HPP__
#include <stdint.h>

#include "ax_global_type.h"
struct video_info {
	AX_PAYLOAD_TYPE_E payload;
	uint32_t		  width;
	uint32_t		  height;
	uint32_t		  fps;
};

struct audio_info {
	AX_PAYLOAD_TYPE_E payload;
	uint32_t		  bps;
	uint32_t		  sample_rate;
	uint32_t		  sample_bits;
};
struct stream_info {
	struct video_info video;
	struct audio_info audio;
};
#endif