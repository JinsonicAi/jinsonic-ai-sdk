#pragma once

#include <any>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <opencv2/core.hpp>

#include "ax_global_type.h"

class AXVideoFrame;

namespace jdk_osd {

struct Color {
	uint8_t r{0};
	uint8_t g{255};
	uint8_t b{0};
	uint8_t a{255};

	uint32_t rgb() const {
		return (static_cast<uint32_t>(r) << 16) |
			   (static_cast<uint32_t>(g) << 8) |
			   static_cast<uint32_t>(b);
	}

	cv::Scalar bgra() const {
		return cv::Scalar(b, g, r, a);
	}

	static Color from_rgb(uint32_t rgb, uint8_t alpha = 255) {
		return Color{
			static_cast<uint8_t>((rgb >> 16) & 0xff),
			static_cast<uint8_t>((rgb >> 8) & 0xff),
			static_cast<uint8_t>(rgb & 0xff),
			alpha};
	}
};

struct Point {
	float x{0};
	float y{0};
};

struct Style {
	Color color{};
	int	  thickness{2};
	int	  alpha{255};
	bool  solid{false};
};

struct TextStyle {
	Color fg{255, 255, 255, 255};
	Color bg{0, 0, 0, 128};
	int	  font_size{24};
	int	  bg_alpha{128};
};

struct Box {
	float		x{0};
	float		y{0};
	float		w{0};
	float		h{0};
	std::string label;
	float		score{0.0f};
	uint64_t	track_id{0};
	Style		style{};
	TextStyle	label_style{};
	bool		draw_label{true};
	bool		ghost{false};
	int			priority{0};
};

struct Text {
	float		x{0};
	float		y{0};
	std::string text;
	TextStyle	style{};
	int			priority{0};
};

struct Line {
	std::vector<Point> points;
	Style			   style{};
	int				   priority{0};
};

struct Polygon {
	std::vector<Point> points;
	Style			   style{};
	bool			   closed{true};
	int				   priority{0};
};

struct Keypoint {
	Point p{};
	Style style{};
	int	  radius{3};
	int	  priority{0};
};

struct Mask {
	std::vector<std::vector<Point>> contours;
	Color							color{0, 255, 0, 96};
	int								priority{0};
};

struct Overlay {
	std::vector<Mask>	 masks;
	std::vector<Polygon> polygons;
	std::vector<Line>	 lines;
	std::vector<Box>	 boxes;
	std::vector<Keypoint> keypoints;
	std::vector<Text>	 texts;

	bool empty() const {
		return masks.empty() && polygons.empty() && lines.empty() &&
			   boxes.empty() && keypoints.empty() && texts.empty();
	}

	void append(const Overlay& rhs) {
		masks.insert(masks.end(), rhs.masks.begin(), rhs.masks.end());
		polygons.insert(polygons.end(), rhs.polygons.begin(), rhs.polygons.end());
		lines.insert(lines.end(), rhs.lines.begin(), rhs.lines.end());
		boxes.insert(boxes.end(), rhs.boxes.begin(), rhs.boxes.end());
		keypoints.insert(keypoints.end(), rhs.keypoints.begin(), rhs.keypoints.end());
		texts.insert(texts.end(), rhs.texts.begin(), rhs.texts.end());
	}
};

struct OverlayResult {
	std::shared_ptr<std::any> payload;
	Overlay					 overlay;
};

inline const Overlay* overlay_from_any(const std::any& value) {
	if (auto p = std::any_cast<Overlay>(&value)) {
		return p;
	}
	if (auto p = std::any_cast<std::shared_ptr<Overlay>>(&value)) {
		return p->get();
	}
	if (auto p = std::any_cast<OverlayResult>(&value)) {
		return &p->overlay;
	}
	if (auto p = std::any_cast<std::shared_ptr<OverlayResult>>(&value)) {
		return (*p) ? &(*p)->overlay : nullptr;
	}
	return nullptr;
}

inline const std::any& payload_from_any(const std::any& value) {
	if (auto p = std::any_cast<OverlayResult>(&value)) {
		if (p->payload) return *p->payload;
	}
	if (auto p = std::any_cast<std::shared_ptr<OverlayResult>>(&value)) {
		if (*p && (*p)->payload) return *(*p)->payload;
	}
	return value;
}

inline std::shared_ptr<std::any> make_overlay_result(std::shared_ptr<std::any> payload,
													 Overlay overlay) {
	return std::make_shared<std::any>(
		OverlayResult{std::move(payload), std::move(overlay)});
}

inline TextStyle make_text_style(int font_size = 24,
								 Color fg = {255, 255, 255, 255},
								 Color bg = {0, 0, 0, 128}) {
	TextStyle style;
	style.font_size = font_size;
	style.fg = fg;
	style.bg = bg;
	style.bg_alpha = bg.a;
	return style;
}

inline Text make_text(float x,
					  float y,
					  std::string value,
					  int font_size = 24,
					  Color fg = {255, 255, 255, 255},
					  Color bg = {0, 0, 0, 128},
					  int priority = 0) {
	return Text{x, y, std::move(value), make_text_style(font_size, fg, bg), priority};
}

inline Box make_box(float x,
					float y,
					float w,
					float h,
					Color color = {0, 255, 0, 255},
					int thickness = 0,
					std::string label = {},
					int label_font_size = 22,
					int priority = 0) {
	Box box;
	box.x = x;
	box.y = y;
	box.w = w;
	box.h = h;
	box.label = std::move(label);
	box.draw_label = !box.label.empty();
	box.style.color = color;
	box.style.alpha = color.a;
	box.style.thickness = thickness;
	box.label_style = make_text_style(label_font_size, {255, 255, 255, 255}, color);
	box.priority = priority;
	return box;
}

inline Line make_line(std::vector<Point> points,
					  Color color = {0, 255, 0, 255},
					  int thickness = 2,
					  int priority = 0) {
	Line line;
	line.points = std::move(points);
	line.style.color = color;
	line.style.alpha = color.a;
	line.style.thickness = thickness;
	line.priority = priority;
	return line;
}

inline Polygon make_polygon(std::vector<Point> points,
							Color color = {0, 255, 0, 255},
							int thickness = 2,
							bool closed = true,
							bool filled = false,
							int priority = 0) {
	Polygon polygon;
	polygon.points = std::move(points);
	polygon.style.color = color;
	polygon.style.alpha = color.a;
	polygon.style.thickness = thickness;
	polygon.style.solid = filled;
	polygon.closed = closed;
	polygon.priority = priority;
	return polygon;
}

inline Keypoint make_keypoint(float x,
							  float y,
							  int radius = 3,
							  Color color = {0, 210, 255, 255},
							  int priority = 0) {
	Keypoint keypoint;
	keypoint.p = {x, y};
	keypoint.radius = radius;
	keypoint.style.color = color;
	keypoint.style.alpha = color.a;
	keypoint.priority = priority;
	return keypoint;
}

inline Mask make_mask(std::vector<std::vector<Point>> contours,
					  Color color = {0, 255, 0, 96},
					  int priority = 0) {
	Mask mask;
	mask.contours = std::move(contours);
	mask.color = color;
	mask.priority = priority;
	return mask;
}

class OverlayBuilder {
public:
	OverlayBuilder& append(const Overlay& overlay) {
		overlay_.append(overlay);
		return *this;
	}

	OverlayBuilder& text(float x,
						 float y,
						 std::string value,
						 int font_size = 24,
						 Color fg = {255, 255, 255, 255},
						 Color bg = {0, 0, 0, 128},
						 int priority = 0) {
		overlay_.texts.push_back(make_text(x, y, std::move(value), font_size, fg, bg, priority));
		return *this;
	}

	OverlayBuilder& box(float x,
						float y,
						float w,
						float h,
						Color color = {0, 255, 0, 255},
						int thickness = 0,
						std::string label = {},
						int label_font_size = 22,
						int priority = 0) {
		overlay_.boxes.push_back(make_box(x, y, w, h, color, thickness, std::move(label), label_font_size, priority));
		return *this;
	}

	OverlayBuilder& line(std::vector<Point> points,
						 Color color = {0, 255, 0, 255},
						 int thickness = 2,
						 int priority = 0) {
		overlay_.lines.push_back(make_line(std::move(points), color, thickness, priority));
		return *this;
	}

	OverlayBuilder& polygon(std::vector<Point> points,
							Color color = {0, 255, 0, 255},
							int thickness = 2,
							bool closed = true,
							bool filled = false,
							int priority = 0) {
		overlay_.polygons.push_back(make_polygon(std::move(points), color, thickness, closed, filled, priority));
		return *this;
	}

	OverlayBuilder& keypoint(float x,
							 float y,
							 int radius = 3,
							 Color color = {0, 210, 255, 255},
							 int priority = 0) {
		overlay_.keypoints.push_back(make_keypoint(x, y, radius, color, priority));
		return *this;
	}

	OverlayBuilder& mask(std::vector<std::vector<Point>> contours,
						 Color color = {0, 255, 0, 96},
						 int priority = 0) {
		overlay_.masks.push_back(make_mask(std::move(contours), color, priority));
		return *this;
	}

	const Overlay& overlay() const {
		return overlay_;
	}

	Overlay build() const {
		return overlay_;
	}

	Overlay take() {
		return std::move(overlay_);
	}

private:
	Overlay overlay_{};
};

struct ComposerOptions {
	bool  tight_roi{true};
	float max_single_roi_ratio{0.65f};
	int	  min_bitmap_width_align{16};
	int	  min_bitmap_height_align{2};
	int	  allocation_width_align{64};
	int	  allocation_height_align{32};
};

struct OsdRendererConfig {
	bool   enable_labels{true};
	bool   enable_masks_realtime{false};
	bool   enable_color_probe{false};
	bool   enable_single_bitmap_composer{true};
	bool   enable_low_bandwidth_dynamic{true};
	float  composer_max_single_roi_ratio{0.65f};
	size_t max_boxes{64};
	size_t max_labels{16};
	size_t max_lines{32};
	size_t max_polygons{16};
	size_t max_keypoints{128};
	size_t osd_batch_size{32};
};

inline constexpr AX_U16 kOsdOpaqueAlpha = 255;

void copy_bgra_to_ax_argb8888(void* dst, const cv::Mat& src);

class OsdComposer {
public:
	struct PreparedBitmap {
		AX_OSD_BMP_ATTR_T attr{};
		std::shared_ptr<AXVideoFrame> holder;

		explicit operator bool() const {
			return holder && attr.pBitmap && attr.u32BmpWidth > 0 && attr.u32BmpHeight > 0;
		}
	};

	explicit OsdComposer(int device_id, ComposerOptions options = {});
	~OsdComposer();

	OsdComposer(const OsdComposer&) = delete;
	OsdComposer& operator=(const OsdComposer&) = delete;
	OsdComposer(OsdComposer&&) noexcept;
	OsdComposer& operator=(OsdComposer&&) noexcept;

	OsdComposer& clear();
	OsdComposer& text(float x,
					  float y,
					  std::string value,
					  int font_size = 24,
					  Color fg = {255, 255, 255, 255},
					  Color bg = {0, 0, 0, 128});
	OsdComposer& box(float x,
					 float y,
					 float w,
					 float h,
					 Color color = {0, 255, 0, 255},
					 int thickness = 0,
					 std::string label = {},
					 int label_font_size = 22);
	OsdComposer& line(std::vector<Point> points,
					  Color color = {0, 255, 0, 255},
					  int thickness = 2);
	OsdComposer& polygon(std::vector<Point> points,
						 Color color = {0, 255, 0, 255},
						 int thickness = 2,
						 bool closed = true,
						 bool filled = false);
	OsdComposer& keypoint(float x,
						  float y,
						  int radius = 3,
						  Color color = {0, 210, 255, 255});
	OsdComposer& mask(std::vector<std::vector<Point>> contours,
					  Color color = {0, 255, 0, 96});

	int flush(AX_VIDEO_FRAME_T* frame);
	int draw(AX_VIDEO_FRAME_T* frame, const Overlay& overlay);
	bool prepare_attr(AX_VIDEO_FRAME_T* frame,
					  const Overlay& overlay,
					  PreparedBitmap& out,
					  bool reuse_scratch = true);

	static int DrawText(AX_VIDEO_FRAME_T* frame,
						int device_id,
						const std::string& text,
						int x,
						int y,
						int font_size = 24,
						Color fg = {255, 255, 255, 255},
						Color bg = {0, 0, 0, 128});
	static int DrawBox(AX_VIDEO_FRAME_T* frame,
					   int device_id,
					   float x,
					   float y,
					   float w,
					   float h,
					   Color color = {0, 255, 0, 255},
					   int thickness = 0,
					   const std::string& label = {});

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

int DrawOverlay(AX_VIDEO_FRAME_T* frame,
				int device_id,
				const Overlay& overlay,
				ComposerOptions options = {});

class OsdRenderer {
public:
	using Config = OsdRendererConfig;

	explicit OsdRenderer(int device_id);
	OsdRenderer(int device_id, Config cfg);
	~OsdRenderer();

	OsdRenderer(const OsdRenderer&) = delete;
	OsdRenderer& operator=(const OsdRenderer&) = delete;
	OsdRenderer(OsdRenderer&&) noexcept;
	OsdRenderer& operator=(OsdRenderer&&) noexcept;

	void reset();
	bool render(const std::shared_ptr<AXVideoFrame>& frame, const Overlay& overlay);
	bool render_layers(const std::shared_ptr<AXVideoFrame>& frame,
					   const Overlay& static_overlay,
					   const Overlay& dynamic_overlay,
					   bool static_dirty = false);

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

bool RenderOverlay(const std::shared_ptr<AXVideoFrame>& frame,
				   int device_id,
				   const Overlay& overlay,
				   OsdRendererConfig cfg = {});

bool RenderOverlayLayers(const std::shared_ptr<AXVideoFrame>& frame,
						 int device_id,
						 const Overlay& static_overlay,
						 const Overlay& dynamic_overlay,
						 bool static_dirty = false,
						 OsdRendererConfig cfg = {});

}  // namespace jdk_osd
