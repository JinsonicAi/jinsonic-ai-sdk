#pragma once
/**
 * TrajectoryOptimizer.hpp  —  High-performance trajectory point thinning / drawing utility
 *
 * Core strategies:
 *   1. Distance-based thinning: skip points whose distance to the previous kept point
 *      is below a threshold (avoids brute-force rendering of dense clusters).
 *   2. Draw only keypoints (filled circles), no polylines → eliminates cv::polylines overhead.
 *   3. Gradient radius along the tail (newest point largest, oldest smallest) —
 *      visually intuitive and reduces total fill area.
 *   4. Hard cap on the number of thinned points to guard against edge-case overload.
 *
 * Usage:
 *   #include "TrajectoryOptimizer.hpp"
 *   auto thinned = trajectory_opt::thin_points(raw_trajectory, min_dist_sq, max_output);
 *   trajectory_opt::append_keypoints(overlay, thinned, color, base_radius, tail_radius);
 */

#include <algorithm>
#include <cmath>
#include <vector>

#include "JdkOsd.hpp"

namespace trajectory_opt {

/**
 * Distance-based trajectory thinning (linear scan; Douglas-Peucker is too heavy here).
 *
 * @param raw            Raw trajectory points in chronological order.
 * @param min_dist_sq    Minimum squared pixel distance between kept points.
 *                       Recommended: (0.5–1% of frame_max_dim)², e.g. 1080p → ~25 (5px²).
 * @param max_output     Maximum number of output points after thinning; 0 = unlimited.
 * @return               Thinned point set (always includes first and last point).
 */
inline std::vector<jdk_osd::Point> thin_points(
	const std::vector<cv::Point2f>& raw,
	float							min_dist_sq = 25.0f,
	int								max_output	= 32) {
	if (raw.empty()) return {};
	if (raw.size() == 1) {
		return {{raw[0].x, raw[0].y}};
	}

	std::vector<jdk_osd::Point> result;
	result.reserve(std::min(static_cast<int>(raw.size()), max_output > 0 ? max_output : 64));

	// Always keep the first point
	result.push_back({raw[0].x, raw[0].y});
	float last_x = raw[0].x;
	float last_y = raw[0].y;

	for (size_t i = 1; i < raw.size() - 1; ++i) {
		float dx = raw[i].x - last_x;
		float dy = raw[i].y - last_y;
		if (dx * dx + dy * dy < min_dist_sq) continue;
		result.push_back({raw[i].x, raw[i].y});
		last_x = raw[i].x;
		last_y = raw[i].y;
		if (max_output > 0 && static_cast<int>(result.size()) >= max_output - 1) break;
	}

	// Always keep the last point (most recent position)
	const auto& last_pt = raw.back();
	result.push_back({last_pt.x, last_pt.y});
	return result;
}

/**
 * Append thinned trajectory points to an overlay as keypoints (dots only, no lines).
 *
 * Radius gradient: oldest points use base_radius, newest point uses tail_radius,
 * giving the trail a natural sense of direction.
 *
 * @param overlay       Target overlay.
 * @param points        Thinned point set.
 * @param color         Dot color.
 * @param base_radius   Radius for the oldest (head) point.
 * @param tail_radius   Radius for the newest (tail) point.
 * @param priority      Drawing priority.
 */
inline void append_keypoints(
	jdk_osd::Overlay&				   overlay,
	const std::vector<jdk_osd::Point>& points,
	jdk_osd::Color					   color,
	int								   base_radius = 2,
	int								   tail_radius = 5,
	int								   priority	   = 120) {
	if (points.empty()) return;
	const int n = static_cast<int>(points.size());
	for (int i = 0; i < n; ++i) {
		// Linearly interpolate radius: base_radius for the first point, tail_radius for the last
		int radius = base_radius;
		if (n > 1) {
			radius = base_radius + (tail_radius - base_radius) * i / (n - 1);
		}
		overlay.keypoints.push_back(
			jdk_osd::make_keypoint(points[i].x, points[i].y, radius, color, priority));
	}
}

/**
 * All-in-one: thin raw trajectory points and draw them as dots (no polylines).
 *
 * @param overlay       Target overlay.
 * @param raw           Raw trajectory (sequence of cv::Point2f).
 * @param color         Dot color.
 * @param frame_max_dim Largest frame dimension, used to derive adaptive min_dist.
 * @param base_radius   Radius for the oldest point.
 * @param tail_radius   Radius for the newest point.
 * @param max_points    Maximum number of points after thinning.
 * @param priority      Drawing priority.
 */
inline void draw_trajectory_optimized(
	jdk_osd::Overlay&				overlay,
	const std::vector<cv::Point2f>& raw,
	jdk_osd::Color					color,
	int								frame_max_dim = 1080,
	int								base_radius	  = 2,
	int								tail_radius	  = 5,
	int								max_points	  = 24,
	int								priority	  = 120) {
	if (raw.empty()) return;

	// Adaptive min distance: larger frames allow larger minimum spacing.
	// 1080p → min_dist ~5px, 4K → ~10px, 720p → ~4px
	const float min_dist	= std::max(3.0f, frame_max_dim * 0.005f);
	const float min_dist_sq = min_dist * min_dist;

	auto thinned = thin_points(raw, min_dist_sq, max_points);
	append_keypoints(overlay, thinned, color, base_radius, tail_radius, priority);
}

}  // namespace trajectory_opt
