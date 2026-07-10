#pragma once

// ─── Shared tripwire "latched red" behavior ───
// All plugins that use tripwires or counting lines (tripwire / wrongway / peopleflow / vehicleflow ...)
// share the same display-latch semantics:
//   1. When an object crosses a line or triggers an alarm, set latched = true.
//   2. Keep the alarm display active while the object box still intersects the line; render both in red.
//   3. Automatically clear the latch and restore the normal color after the object box leaves the line.
//
// Pipeline usage (for every line in every frame):
//   if (a line crossing is triggered) ls.alarm_latched = true;
//   bool active = tripline::sustain_latch(ls.alarm_latched, line.p1, line.p2, obj_rect);
//   if (active) { obj.alarm_active = true; obj.line_index = li; }
//
// Node rendering usage:
//   Object box: if (obj.alarm_active) color = tripline::kAlarmColor;
//   Tripwire: when any obj.alarm_active && obj.line_index == li, use kAlarmColor for that line.

#include <cmath>
#include <opencv2/core.hpp>

namespace tripline {

// Alarm display color (RGB).
constexpr unsigned int kAlarmColor = 0xFF0000;

// Whether line segment ab intersects rect, using Liang-Barsky clipping.
inline bool segment_intersects_rect(const cv::Point2f& a, const cv::Point2f& b,
                                    const cv::Rect_<float>& rect) {
	// An endpoint inside the rectangle guarantees an intersection (fast path).
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
			if (q[i] < 0) return false;  // Parallel and outside the boundary.
		} else {
			float r = q[i] / p[i];
			if (p[i] < 0) { if (r > t1) return false; if (r > t0) t0 = r; }
			else          { if (r < t0) return false; if (r < t1) t1 = r; }
		}
	}
	return t0 <= t1;
}

// Sustain a latched alarm while the box intersects the line, returning true.
// Clear the latch and return false when the box leaves the line; return false immediately when not latched.
inline bool sustain_latch(bool& latched, const cv::Point2f& p1, const cv::Point2f& p2,
                          const cv::Rect_<float>& obj_rect) {
	if (!latched) return false;
	if (segment_intersects_rect(p1, p2, obj_rect)) return true;
	latched = false;  // The box has left the line; restore the normal color.
	return false;
}

}  // namespace tripline
