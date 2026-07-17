#pragma once

// ─── Common "latch-to-red" logic for tripwire / line crossing ───
// All plugins that use tripwires/counting lines (tripwire / wrongway / peopleflow / vehicleflow ...)
// share the same display-latch semantics:
//   1. The instant a target crosses the line (or triggers an alarm) → set the latch latched = true;
//   2. As long as the target box still intersects that line, keep the "alarm display state"
//      (the target box and the line are rendered in red);
//   3. When the target box leaves the line → the latch is cleared automatically and the color returns to normal.
//
// Pipeline usage (per frame, per line):
//   if (line crossed) ls.alarm_latched = true;
//   bool active = tripline::sustain_latch(ls.alarm_latched, line.p1, line.p2, obj_rect);
//   if (active) { obj.alarm_active = true; obj.line_index = li; }
//
// Node rendering usage:
//   target box: if (obj.alarm_active) color = tripline::kAlarmColor;
//   line:       any obj.alarm_active && obj.line_index == li → render that line with kAlarmColor.

#include <cmath>
#include <opencv2/core.hpp>

namespace tripline {

// Alarm display color (RGB)
constexpr unsigned int kAlarmColor = 0xFF0000;

// Whether segment ab intersects rectangle rect (Liang-Barsky clipping)
inline bool segment_intersects_rect(const cv::Point2f& a, const cv::Point2f& b,
                                    const cv::Rect_<float>& rect) {
	// An endpoint inside the rectangle → must intersect (fast path)
	if (rect.contains(a) || rect.contains(b)) return true;

	float x0 = a.x, y0 = a.y, x1 = b.x, y1 = b.y;
	float dx = x1 - x0, dy = y1 - y0;
	float t0 = 0.0f, t1 = 1.0f;
	const float xmin = rect.x, xmax = rect.x + rect.width;
	const float ymin = rect.y, ymax = rect.y + rect.height;

	const float p[4] = {-dx, dx, -dy, dy};
	const float q[4] = {x0 - xmin, xmax - x0, y0 - ymin, ymax - y0};
	for (int i = 0; i < 4; ++i) {
		if (std::fabs(p[i]) < 1e-6f) {
			if (q[i] < 0) return false;  // parallel and outside the boundary
		} else {
			float r = q[i] / p[i];
			if (p[i] < 0) { if (r > t1) return false; if (r > t0) t0 = r; }
			else          { if (r < t0) return false; if (r < t1) t1 = r; }
		}
	}
	return t0 <= t1;
}

// Latch sustain: when latched is set and the box still crosses the line → keep it (return true);
// when the box leaves the line → clear the latch automatically (return false). Returns false directly when latched is not set.
inline bool sustain_latch(bool& latched, const cv::Point2f& p1, const cv::Point2f& p2,
                          const cv::Rect_<float>& obj_rect) {
	if (!latched) return false;
	if (segment_intersects_rect(p1, p2, obj_rect)) return true;
	latched = false;  // box has left the line → restore normal color
	return false;
}

}  // namespace tripline
