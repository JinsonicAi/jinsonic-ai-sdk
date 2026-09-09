#pragma once
/**
 * TrajectoryOptimizer.hpp — High-performance trajectory point thinning/drawing tool
 *
 * Core strategy:
 *   1. Distance thinning: skip when adjacent point distance is less than threshold (avoid dense point rendering)
 *   2. Only draw key points (circles), no connecting lines → saves cv::polylines overhead
 *   3. Tail gradient radius (newest point large, old point small), both aesthetic and saves fill area
 *   4. Upper limit on thinned point count to prevent extreme cases with too many points
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
 * Distance-based trajectory point thinning (Douglas-Peucker is too heavy, use simple linear scan here)
 *
 * @param raw            Raw trajectory points (sorted by time)
 * @param min_dist_sq    Minimum pixel distance squared (skip if adjacent point distance < sqrt(min_dist_sq))
 *                       Recommended value: square of 0.5~1% of frame_max_dim, e.g. 1080p → ~25 (5px²)
 * @param max_output     Maximum output points after thinning, 0 means no limit
 * @return               Thinned point set (keeps first/last + middle points with sufficient distance)
 */
inline std::vector<jdk_osd::Point> thin_points(
    const std::vector<cv::Point2f>& raw,
    float min_dist_sq = 25.0f,
    int max_output = 32)
{
    if (raw.empty()) return {};
    if (raw.size() == 1) {
        return {{raw[0].x, raw[0].y}};
    }

    std::vector<jdk_osd::Point> result;
    result.reserve(std::min(static_cast<int>(raw.size()), max_output > 0 ? max_output : 64));

    // Always keep first point
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

    // Always keep last point (newest position)
    const auto& last_pt = raw.back();
    result.push_back({last_pt.x, last_pt.y});
    return result;
}

/**
 * Append thinned trajectory points to overlay in keypoint mode (only draw points, no lines)
 *
 * Gradient radius: old points have small radius, new points have large radius, visually directional.
 *
 * @param overlay       Target overlay
 * @param points        Thinned point set
 * @param color         Point color
 * @param base_radius   Old point (head) radius
 * @param tail_radius   New point (tail/newest) radius
 * @param priority      Drawing priority
 */
inline void append_keypoints(
    jdk_osd::Overlay& overlay,
    const std::vector<jdk_osd::Point>& points,
    jdk_osd::Color color,
    int base_radius = 2,
    int tail_radius = 5,
    int priority = 120)
{
    if (points.empty()) return;
    const int n = static_cast<int>(points.size());
    for (int i = 0; i < n; ++i) {
        // Linear interpolation radius: first point uses base_radius, last uses tail_radius
        int radius = base_radius;
        if (n > 1) {
            radius = base_radius + (tail_radius - base_radius) * i / (n - 1);
        }
        overlay.keypoints.push_back(
            jdk_osd::make_keypoint(points[i].x, points[i].y, radius, color, priority));
    }
}

/**
 * Integrated interface: thin raw trajectory + only draw points (no connecting lines)
 *
 * @param overlay       Target overlay
 * @param raw           Raw trajectory (cv::Point2f sequence)
 * @param color         Point color
 * @param frame_max_dim Frame max dimension (for adaptive min_dist)
 * @param base_radius   Old point radius
 * @param tail_radius   Newest point radius
 * @param max_points    Max points after thinning
 * @param priority      Drawing priority
 */
inline void draw_trajectory_optimized(
    jdk_osd::Overlay& overlay,
    const std::vector<cv::Point2f>& raw,
    jdk_osd::Color color,
    int frame_max_dim = 1080,
    int base_radius = 2,
    int tail_radius = 5,
    int max_points = 24,
    int priority = 120)
{
    if (raw.empty()) return;

    // Adaptive minimum distance: the larger the frame, the larger the allowed minimum spacing
    // 1080p → min_dist ~5px, 4K → ~10px, 720p → ~4px
    const float min_dist = std::max(3.0f, frame_max_dim * 0.005f);
    const float min_dist_sq = min_dist * min_dist;

    auto thinned = thin_points(raw, min_dist_sq, max_points);
    append_keypoints(overlay, thinned, color, base_radius, tail_radius, priority);
}

}  // namespace trajectory_opt
