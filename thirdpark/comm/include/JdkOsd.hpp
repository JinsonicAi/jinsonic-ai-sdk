#pragma once

#include <algorithm>
#include <any>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <opencv2/core.hpp>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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

	// Uniformly "vivify" a color: keep the original hue and alpha while pushing
	// saturation and value into a high range, so OSD elements such as text, points,
	// lines, boxes, polygons and masks always stay bright and eye-catching.
	// Hue-less colors like black/white/gray are left unchanged, to avoid tinting
	// white text or black outlines by mistake.
	// min_s / min_v are the lower bounds for saturation and value (0~1), pushed high by default.
	Color vivid(float min_s = 0.75f, float min_v = 1.0f) const {
		const uint8_t max_u8   = std::max({r, g, b});
		const uint8_t min_u8   = std::min({r, g, b});
		const int	  delta_u8 = static_cast<int>(max_u8) - static_cast<int>(min_u8);

		// Return as-is when nearly hue-less (grayscale).
		if (delta_u8 == 0) return *this;

		const float max_c	  = max_u8 / 255.0f;
		const float delta	  = delta_u8 / 255.0f;
		const float current_s = delta_u8 / static_cast<float>(max_u8);
		// An overlay may cross multiple shared boundaries. Return already-vivid colors
		// directly to avoid redundant HSV conversions.
		if (current_s >= min_s && max_c >= min_v) return *this;

		const float rf = r / 255.0f;
		const float gf = g / 255.0f;
		const float bf = b / 255.0f;

		// Compute hue H (0~6).
		float h = 0.0f;
		if (max_c == rf) {
			h = std::fmod((gf - bf) / delta, 6.0f);
		} else if (max_c == gf) {
			h = (bf - rf) / delta + 2.0f;
		} else {
			h = (rf - gf) / delta + 4.0f;
		}
		if (h < 0.0f) h += 6.0f;

		// Boost saturation and value.
		const float s = std::max(min_s, current_s);
		const float v = std::max(min_v, max_c);

		const int	h_i = static_cast<int>(h);
		const float f	= h - h_i;
		const float p	= v * (1.0f - s);
		const float q	= v * (1.0f - f * s);
		const float t	= v * (1.0f - (1.0f - f) * s);
		float		nr = v, ng = v, nb = v;
		switch (h_i) {
		case 0:
			nr = v;
			ng = t;
			nb = p;
			break;
		case 1:
			nr = q;
			ng = v;
			nb = p;
			break;
		case 2:
			nr = p;
			ng = v;
			nb = t;
			break;
		case 3:
			nr = p;
			ng = q;
			nb = v;
			break;
		case 4:
			nr = t;
			ng = p;
			nb = v;
			break;
		default:
			nr = v;
			ng = p;
			nb = q;
			break;
		}

		return Color{
			static_cast<uint8_t>(std::lround(nr * 255.0f)),
			static_cast<uint8_t>(std::lround(ng * 255.0f)),
			static_cast<uint8_t>(std::lround(nb * 255.0f)),
			a};
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

struct BitmapMask {
	cv::Mat	 mask;
	cv::Rect roi{};
	Color	 color{0, 255, 0, 96};
	int		 priority{0};
	uint8_t	 threshold{1};
};

struct Overlay {
	std::vector<Mask>		masks;
	std::vector<BitmapMask> bitmap_masks;
	std::vector<Polygon>	polygons;
	std::vector<Line>		lines;
	std::vector<Box>		boxes;
	std::vector<Keypoint>	keypoints;
	std::vector<Text>		texts;

	bool empty() const {
		return masks.empty() && bitmap_masks.empty() && polygons.empty() && lines.empty() &&
			   boxes.empty() && keypoints.empty() && texts.empty();
	}

	// Vivify geometry elements only; plain text foreground/background keep their
	// business configuration semantics.
	// This runs per element, does not enter the mask pixel loop, and does not change
	// the number of bitmap uploads or hardware submissions.
	void normalize_geometry_colors() {
		for (auto& mask : masks) mask.color = mask.color.vivid();
		for (auto& mask : bitmap_masks) mask.color = mask.color.vivid();
		for (auto& polygon : polygons) {
			polygon.style.color = polygon.style.color.vivid();
			polygon.style.alpha = polygon.style.color.a;
		}
		for (auto& line : lines) {
			line.style.color = line.style.color.vivid();
			line.style.alpha = line.style.color.a;
		}
		for (auto& box : boxes) {
			box.style.color = box.style.color.vivid();
			box.style.alpha = box.style.color.a;
		}
		for (auto& keypoint : keypoints) {
			keypoint.style.color = keypoint.style.color.vivid();
			keypoint.style.alpha = keypoint.style.color.a;
		}
	}

	void append(const Overlay& rhs) {
		masks.insert(masks.end(), rhs.masks.begin(), rhs.masks.end());
		bitmap_masks.insert(bitmap_masks.end(), rhs.bitmap_masks.begin(), rhs.bitmap_masks.end());
		polygons.insert(polygons.end(), rhs.polygons.begin(), rhs.polygons.end());
		lines.insert(lines.end(), rhs.lines.begin(), rhs.lines.end());
		boxes.insert(boxes.end(), rhs.boxes.begin(), rhs.boxes.end());
		keypoints.insert(keypoints.end(), rhs.keypoints.begin(), rhs.keypoints.end());
		texts.insert(texts.end(), rhs.texts.begin(), rhs.texts.end());
		// append() is also used by callers that construct geometry structs directly.
		// Normalize only the newly appended elements; text colors keep their business semantics.
		auto normalize_range = [](auto& items, size_t begin) {
			for (size_t i = begin; i < items.size(); ++i) {
				using Item = std::decay_t<decltype(items[i])>;
				if constexpr (std::is_same_v<Item, Mask> || std::is_same_v<Item, BitmapMask>) {
					items[i].color = items[i].color.vivid();
				} else {
					items[i].style.color = items[i].style.color.vivid();
					items[i].style.alpha = items[i].style.color.a;
				}
			}
		};
		normalize_range(masks, masks.size() - rhs.masks.size());
		normalize_range(bitmap_masks, bitmap_masks.size() - rhs.bitmap_masks.size());
		normalize_range(polygons, polygons.size() - rhs.polygons.size());
		normalize_range(lines, lines.size() - rhs.lines.size());
		normalize_range(boxes, boxes.size() - rhs.boxes.size());
		normalize_range(keypoints, keypoints.size() - rhs.keypoints.size());
	}
};

struct OverlayResult {
	std::shared_ptr<std::any> payload;
	Overlay					  overlay;
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
													 Overlay				   overlay) {
	overlay.normalize_geometry_colors();
	return std::make_shared<std::any>(
		OverlayResult{std::move(payload), std::move(overlay)});
}

inline TextStyle make_text_style(int   font_size = 24,
								 Color fg		 = {255, 255, 255, 255},
								 Color bg		 = {0, 0, 0, 128}) {
	TextStyle style;
	style.font_size = font_size;
	style.fg		= fg.vivid();
	style.bg		= bg.vivid();
	style.bg_alpha	= bg.a;
	return style;
}

inline Text make_text(float		  x,
					  float		  y,
					  std::string value,
					  int		  font_size = 24,
					  Color		  fg		= {255, 255, 255, 255},
					  Color		  bg		= {0, 0, 0, 128},
					  int		  priority	= 0) {
	return Text{x, y, std::move(value), make_text_style(font_size, fg, bg), priority};
}

inline Box make_box(float		x,
					float		y,
					float		w,
					float		h,
					Color		color			= {0, 255, 0, 255},
					int			thickness		= 0,
					std::string label			= {},
					int			label_font_size = 22,
					int			priority		= 0) {
	Box box;
	box.x				= x;
	box.y				= y;
	box.w				= w;
	box.h				= h;
	box.label			= std::move(label);
	box.draw_label		= !box.label.empty();
	box.style.color		= color.vivid();
	box.style.alpha		= color.a;
	box.style.thickness = thickness;
	box.label_style		= make_text_style(label_font_size, {255, 255, 255, 255}, box.style.color);
	box.priority		= priority;
	return box;
}

inline Line make_line(std::vector<Point> points,
					  Color				 color	   = {0, 255, 0, 255},
					  int				 thickness = 2,
					  int				 priority  = 0) {
	Line line;
	line.points			 = std::move(points);
	line.style.color	 = color.vivid();
	line.style.alpha	 = color.a;
	line.style.thickness = thickness;
	line.priority		 = priority;
	return line;
}

inline Polygon make_polygon(std::vector<Point> points,
							Color			   color	 = {0, 255, 0, 255},
							int				   thickness = 2,
							bool			   closed	 = true,
							bool			   filled	 = false,
							int				   priority	 = 0) {
	Polygon polygon;
	polygon.points			= std::move(points);
	polygon.style.color		= color.vivid();
	polygon.style.alpha		= color.a;
	polygon.style.thickness = thickness;
	polygon.style.solid		= filled;
	polygon.closed			= closed;
	polygon.priority		= priority;
	return polygon;
}

inline Keypoint make_keypoint(float x,
							  float y,
							  int	radius	 = 3,
							  Color color	 = {0, 210, 255, 255},
							  int	priority = 0) {
	Keypoint keypoint;
	keypoint.p			 = {x, y};
	keypoint.radius		 = radius;
	keypoint.style.color = color.vivid();
	keypoint.style.alpha = color.a;
	keypoint.priority	 = priority;
	return keypoint;
}

inline Mask make_mask(std::vector<std::vector<Point>> contours,
					  Color							  color	   = {0, 255, 0, 96},
					  int							  priority = 0) {
	Mask mask;
	mask.contours = std::move(contours);
	mask.color	  = color.vivid();
	mask.priority = priority;
	return mask;
}

inline BitmapMask make_bitmap_mask(cv::Mat	mask,
								   cv::Rect roi		  = {},
								   Color	color	  = {0, 255, 0, 96},
								   uint8_t	threshold = 1,
								   int		priority  = 0) {
	BitmapMask item;
	item.mask	   = std::move(mask);
	item.roi	   = roi;
	item.color	   = color.vivid();
	item.threshold = threshold;
	item.priority  = priority;
	return item;
}

class OverlayBuilder {
public:
	OverlayBuilder& append(const Overlay& overlay) {
		overlay_.append(overlay);
		return *this;
	}

	OverlayBuilder& text(float		 x,
						 float		 y,
						 std::string value,
						 int		 font_size = 24,
						 Color		 fg		   = {255, 255, 255, 255},
						 Color		 bg		   = {0, 0, 0, 128},
						 int		 priority  = 0) {
		overlay_.texts.push_back(make_text(x, y, std::move(value), font_size, fg, bg, priority));
		return *this;
	}

	OverlayBuilder& box(float		x,
						float		y,
						float		w,
						float		h,
						Color		color			= {0, 255, 0, 255},
						int			thickness		= 0,
						std::string label			= {},
						int			label_font_size = 22,
						int			priority		= 0) {
		overlay_.boxes.push_back(make_box(x, y, w, h, color, thickness, std::move(label), label_font_size, priority));
		return *this;
	}

	OverlayBuilder& line(std::vector<Point> points,
						 Color				color	  = {0, 255, 0, 255},
						 int				thickness = 2,
						 int				priority  = 0) {
		overlay_.lines.push_back(make_line(std::move(points), color, thickness, priority));
		return *this;
	}

	OverlayBuilder& polygon(std::vector<Point> points,
							Color			   color	 = {0, 255, 0, 255},
							int				   thickness = 2,
							bool			   closed	 = true,
							bool			   filled	 = false,
							int				   priority	 = 0) {
		overlay_.polygons.push_back(make_polygon(std::move(points), color, thickness, closed, filled, priority));
		return *this;
	}

	OverlayBuilder& keypoint(float x,
							 float y,
							 int   radius	= 3,
							 Color color	= {0, 210, 255, 255},
							 int   priority = 0) {
		overlay_.keypoints.push_back(make_keypoint(x, y, radius, color, priority));
		return *this;
	}

	OverlayBuilder& mask(std::vector<std::vector<Point>> contours,
						 Color							 color	  = {0, 255, 0, 96},
						 int							 priority = 0) {
		overlay_.masks.push_back(make_mask(std::move(contours), color, priority));
		return *this;
	}

	OverlayBuilder& bitmap_mask(cv::Mat	 mask,
								cv::Rect roi	   = {},
								Color	 color	   = {0, 255, 0, 96},
								uint8_t	 threshold = 1,
								int		 priority  = 0) {
		overlay_.bitmap_masks.push_back(make_bitmap_mask(std::move(mask), roi, color, threshold, priority));
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
	size_t max_lines{256};
	size_t max_polygons{16};
	size_t max_keypoints{512};
	size_t osd_batch_size{32};
};

inline constexpr AX_U16 kOsdOpaqueAlpha = 255;

void copy_bgra_to_ax_argb8888(void* dst, const cv::Mat& src);

class OsdComposer {
public:
	struct PreparedBitmap {
		AX_OSD_BMP_ATTR_T			  attr{};
		std::shared_ptr<AXVideoFrame> holder;

		explicit operator bool() const {
			return holder && attr.pBitmap && attr.u32BmpWidth > 0 && attr.u32BmpHeight > 0;
		}
	};

	explicit OsdComposer(int device_id, ComposerOptions options = {});
	~OsdComposer();

	OsdComposer(const OsdComposer&)			   = delete;
	OsdComposer& operator=(const OsdComposer&) = delete;
	OsdComposer(OsdComposer&&) noexcept;
	OsdComposer& operator=(OsdComposer&&) noexcept;

	OsdComposer& clear();
	OsdComposer& text(float		  x,
					  float		  y,
					  std::string value,
					  int		  font_size = 24,
					  Color		  fg		= {255, 255, 255, 255},
					  Color		  bg		= {0, 0, 0, 128});
	OsdComposer& box(float		 x,
					 float		 y,
					 float		 w,
					 float		 h,
					 Color		 color			 = {0, 255, 0, 255},
					 int		 thickness		 = 0,
					 std::string label			 = {},
					 int		 label_font_size = 22);
	OsdComposer& line(std::vector<Point> points,
					  Color				 color	   = {0, 255, 0, 255},
					  int				 thickness = 2);
	OsdComposer& polygon(std::vector<Point> points,
						 Color				color	  = {0, 255, 0, 255},
						 int				thickness = 2,
						 bool				closed	  = true,
						 bool				filled	  = false);
	OsdComposer& keypoint(float x,
						  float y,
						  int	radius = 3,
						  Color color  = {0, 210, 255, 255});
	OsdComposer& mask(std::vector<std::vector<Point>> contours,
					  Color							  color = {0, 255, 0, 96});
	OsdComposer& bitmap_mask(cv::Mat  mask,
							 cv::Rect roi		= {},
							 Color	  color		= {0, 255, 0, 96},
							 uint8_t  threshold = 1);

	int	 flush(AX_VIDEO_FRAME_T* frame);
	int	 draw(AX_VIDEO_FRAME_T* frame, const Overlay& overlay);
	bool prepare_attr(AX_VIDEO_FRAME_T* frame,
					  const Overlay&	overlay,
					  PreparedBitmap&	out,
					  bool				reuse_scratch = true);

	// Generate a compact ARGB bitmap for each bitmap_mask covering only its own bbox.
	// This way masks are no longer composited with boxes/text into one large bitmap
	// spanning the union ROI of the whole frame; the amount of data uploaded/drawn
	// shrinks with the target bbox area, and boxes/text can still use hardware
	// low-bandwidth direct drawing.
	// The generated attrs are batch-submitted to HwDrawOsd together with the
	// static/dynamic attrs and blended independently one by one.
	// Internally uses a cross-frame reusable bitmap pool to avoid repeatedly
	// allocating DMA frames every frame.
	bool prepare_mask_attrs(AX_VIDEO_FRAME_T*			 frame,
							const Overlay&				 overlay,
							std::vector<PreparedBitmap>& out);

	static int DrawText(AX_VIDEO_FRAME_T*  frame,
						int				   device_id,
						const std::string& text,
						int				   x,
						int				   y,
						int				   font_size = 24,
						Color			   fg		 = {255, 255, 255, 255},
						Color			   bg		 = {0, 0, 0, 128});
	static int DrawBox(AX_VIDEO_FRAME_T*  frame,
					   int				  device_id,
					   float			  x,
					   float			  y,
					   float			  w,
					   float			  h,
					   Color			  color		= {0, 255, 0, 255},
					   int				  thickness = 0,
					   const std::string& label		= {});

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

int DrawOverlay(AX_VIDEO_FRAME_T* frame,
				int				  device_id,
				const Overlay&	  overlay,
				ComposerOptions	  options = {});

class OsdRenderer {
public:
	using Config = OsdRendererConfig;

	explicit OsdRenderer(int device_id);
	OsdRenderer(int device_id, Config cfg);
	~OsdRenderer();

	OsdRenderer(const OsdRenderer&)			   = delete;
	OsdRenderer& operator=(const OsdRenderer&) = delete;
	OsdRenderer(OsdRenderer&&) noexcept;
	OsdRenderer& operator=(OsdRenderer&&) noexcept;

	void reset();
	bool render(const std::shared_ptr<AXVideoFrame>& frame, const Overlay& overlay);
	bool render_layers(const std::shared_ptr<AXVideoFrame>& frame,
					   const Overlay&						static_overlay,
					   const Overlay&						dynamic_overlay,
					   bool									static_dirty = false);

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

bool RenderOverlay(const std::shared_ptr<AXVideoFrame>& frame,
				   int									device_id,
				   const Overlay&						overlay,
				   OsdRendererConfig					cfg = {});

bool RenderOverlayLayers(const std::shared_ptr<AXVideoFrame>& frame,
						 int								  device_id,
						 const Overlay&						  static_overlay,
						 const Overlay&						  dynamic_overlay,
						 bool								  static_dirty = false,
						 OsdRendererConfig					  cfg		   = {});

}  // namespace jdk_osd
