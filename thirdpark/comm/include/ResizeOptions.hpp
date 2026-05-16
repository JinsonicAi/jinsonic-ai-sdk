#ifndef _RESIZE_OPTIONS_HPP_
#define _RESIZE_OPTIONS_HPP_

#include <cstdint>

enum class ResizeMode {
	Stretch,           // Stretch to target size
	LetterboxTopLeft,  // Letterbox with top-left alignment, fill empty space with background color
	LetterboxCenter,   // Letterbox with center alignment, fill empty space with background color
};

struct ResizeOptions {
	ResizeOptions() = default;
	ResizeOptions(ResizeMode resizeMode, uint32_t color = 0x808080, bool enablePrefill = true)
		: mode(resizeMode), bgColor(color), prefill(enablePrefill) {}
	// ResizeOptions(bool keepAspectRatio)
	// 	: mode(keepAspectRatio ? ResizeMode::LetterboxCenter : ResizeMode::Stretch) {}

	ResizeMode mode{ResizeMode::LetterboxCenter};
	uint32_t       bgColor{0x808080};
	bool           prefill{true};
};

using IvpsCropResizeOptions = ResizeOptions;

#endif
