#pragma once
/**
 * TrajectoryOptimizer.hpp  —  High-performance trajectory point thinning and rendering utility
 *
 * Core strategy:
 *   1. Distance-based thinning: skip adjacent points closer than the threshold to avoid redundant rendering.
 *   2. Render keypoints only (circles), without connecting lines, to avoid the cost of cv::polylines.
 *   3. Use a trailing radius gradient (newer points larger than older points) for directional visual feedback.
 *   4. Limit the number of thinned points to bound work in extreme cases.
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
 * Distance-based trajectory point thinning using a lightweight linear scan instead of Douglas-Peucker.
 *
 * @param raw            Source trajectory points in chronological order.
 * @param min_dist_sq    Squared minimum pixel distance; adjacent points closer than sqrt(min_dist_sq) are skipped.
 *                       Recommended: the square of 0.5-1% of frame_max_dim, e.g. about 25 for 1080p (5 px squared).
 * @param max_output     Maximum number of output points after thinning; 0 disables the limit.
 * @return               Thinned points, preserving the first, last, and sufficiently distant intermediate points.
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

    // Always preserve the first point.
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

    // Always preserve the last point (the newest position).
    const auto& last_pt = raw.back();
    result.push_back({last_pt.x, last_pt.y});
    return result;
}

/**
 * Append thinned trajectory points to an overlay as keypoints only, without connecting lines.
 *
 * Radius gradient: older points are smaller and newer points are larger to show direction.
 *
 * @param overlay       Destination overlay.
 * @param points        Thinned point collection.
 * @param color         Keypoint color.
 * @param base_radius   Radius of the oldest (first) point.
 * @param tail_radius   Radius of the newest (last) point.
 * @param priority      Rendering priority.
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
        // Linearly interpolate the radius from base_radius to tail_radius.
        int radius = base_radius;
        if (n > 1) {
            radius = base_radius + (tail_radius - base_radius) * i / (n - 1);
        }
        overlay.keypoints.push_back(
            jdk_osd::make_keypoint(points[i].x, points[i].y, radius, color, priority));
    }
}

/**
 * Convenience API that thins a source trajectory and renders keypoints without connecting lines.
 *
 * @param overlay       Destination overlay.
 * @param raw           Source trajectory as a cv::Point2f sequence.
 * @param color         Keypoint color.
 * @param frame_max_dim Largest frame dimension, used to adapt min_dist.
 * @param base_radius   Radius of older points.
 * @param tail_radius   Radius of the newest point.
 * @param max_points    Maximum number of points after thinning.
 * @param priority      Rendering priority.
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

    // Adapt the minimum distance: larger frames permit wider spacing.
    // 1080p → min_dist ~5px, 4K → ~10px, 720p → ~4px
    const float min_dist = std::max(3.0f, frame_max_dim * 0.005f);
    const float min_dist_sq = min_dist * min_dist;

    auto thinned = thin_points(raw, min_dist_sq, max_points);
    append_keypoints(overlay, thinned, color, base_radius, tail_radius, priority);
}

}  // namespace trajectory_opt
