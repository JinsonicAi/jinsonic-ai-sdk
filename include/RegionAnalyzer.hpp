#pragma once
/**
 * @file   RegionAnalyzer.hpp
 * @brief  General region analysis engine with production-grade encapsulation
 *
 * Covered scenarios:
 *   1. Intrusion detection (entering a region -> instant/delayed alarm)
 *   2. Absence detection (no target in the region for too long -> alarm)
 *   3. Loitering detection (target stays in the region too long -> alarm)
 *   4. Crowd alarm (target count in the region exceeds threshold -> alarm)
 *   5. Tripwire detection (target crosses a virtual line -> alarm)
 *   6. Region statistics (real-time target count / cumulative in-out count)
 *
 * Usage (plugin side):
 *   RegionAnalyzer analyzer;
 *   analyzer.add_region(region, rule);
 *   analyzer.set_callback([](const RegionEvent& e){ ... });
 *   // Call once per frame
 *   analyzer.update(det);
 */

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include "ax_algorithm_sdk.h"

namespace region {

// ─────────────────── Time Utilities ───────────────────
using Clock    = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

inline int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               Clock::now().time_since_epoch())
        .count();
}

// ─────────────────── Geometry Utilities ───────────────────
/// ax_bbox_t -> cv::Rect (integer conversion)
inline cv::Rect to_cv_rect(const ax_bbox_t& b) noexcept {
    return cv::Rect(static_cast<int>(std::lround(b.x)),
                    static_cast<int>(std::lround(b.y)),
                    static_cast<int>(std::lround(b.w)),
                    static_cast<int>(std::lround(b.h)));
}

/// Center point of a bbox
inline cv::Point2f bbox_center(const ax_bbox_t& b) noexcept {
    return {b.x + b.w * 0.5f, b.y + b.h * 0.5f};
}

/// Whether the center point is inside the polygon
inline bool point_in_polygon(const cv::Point2f& pt,
                             const std::vector<cv::Point>& poly) {
    return cv::pointPolygonTest(poly, pt, false) >= 0;
}

/// Whether the center point is inside any region (empty region list means "full frame")
inline bool in_any_region(const ax_bbox_t& b,
                          const std::vector<std::vector<cv::Point>>& regions) {
    if (regions.empty()) return true;
    auto c = bbox_center(b);
    for (const auto& poly : regions)
        if (point_in_polygon(c, poly)) return true;
    return false;
}

/// Whether the bbox area is smaller than the threshold
inline bool bbox_too_small(const ax_bbox_t& b, float min_size) noexcept {
    return b.w * b.h < min_size * min_size;
}

// ─────────────────── Tripwire Utilities ───────────────────
/// Check whether two line segments intersect (p1-p2 and p3-p4)
bool segments_intersect(const cv::Point2f& p1, const cv::Point2f& p2,
                        const cv::Point2f& p3, const cv::Point2f& p4);

/// Determine crossing direction: +1 forward / -1 reverse / 0 no crossing
int cross_direction(const cv::Point2f& prev_center,
                    const cv::Point2f& curr_center,
                    const cv::Point2f& line_p1,
                    const cv::Point2f& line_p2);

// ─────────────────── Event Definitions ───────────────────
enum class EventType : uint8_t {
    Enter        = 0,   // Target enters the region
    Leave        = 1,   // Target leaves the region
    Loiter       = 2,   // Loitering (stay timeout)
    Absence      = 3,   // Absence (region vacant timeout)
    Crowd        = 4,   // Crowding (count exceeds threshold)
    CrowdRecover = 5,   // Crowd recovered
    LineCrossIn  = 6,   // Tripwire crossed (forward)
    LineCrossOut = 7,   // Tripwire crossed (reverse)
    AbsenceRecover = 8, // Absence recovered
};

struct RegionEvent {
    EventType     type;
    int           region_id      = -1;
    unsigned long track_id       = 0;
    ax_bbox_t     bbox           = {};
    int           label          = 0;
    int64_t       timestamp_ms   = 0;
    int           object_count   = 0;     // Current target count in the region
    float         dwell_sec      = 0.f;   // Time the target has stayed in the region
    float         absence_sec    = 0.f;   // Region vacancy duration
};

using EventCallback = std::function<void(const RegionEvent&)>;

// ─────────────────── Region Rules ───────────────────
struct RegionRule {
    // Basic switches
    bool enable_enter       = true;    // Trigger enter event
    bool enable_leave       = true;    // Trigger leave event

    // Loitering detection
    bool  enable_loiter     = false;
    float loiter_sec        = 10.f;    // Trigger when stay duration exceeds this value
    float loiter_cooldown   = 30.f;    // Cooldown before the same track can trigger again

    // Absence detection
    bool  enable_absence    = false;
    float absence_sec       = 30.f;    // Trigger when region vacancy exceeds this value
    float absence_cooldown  = 60.f;    // Cooldown before triggering again

    // Crowd detection
    bool enable_crowd       = false;
    int  crowd_threshold    = 10;      // Trigger when count exceeds this number
    int  crowd_recover_gap  = 2;       // Recover when count falls back to threshold - gap

    // Tripwire detection
    bool enable_line_cross  = false;

    // Filtering
    float min_bbox_size     = 0.f;     // Minimum bbox side length (pixels)
    int   label_filter      = -1;      // -1 = no filtering; >=0 = only track this label
};

// ─────────────────── Region Definition ───────────────────
struct Region {
    int                    id   = 0;
    std::string            name;
    std::vector<cv::Point> polygon;    // Polygon vertices (>= 3)
};

// ─────────────────── Tripwire Definition ───────────────────
struct Tripwire {
    int        id = 0;
    std::string name;
    cv::Point2f p1, p2;               // Two endpoints of the line segment
};

// ─────────────────── Region Statistics ───────────────────
struct RegionStats {
    int  current_count    = 0;   // Current target count in the region
    int  total_enter      = 0;   // Total entries
    int  total_leave      = 0;   // Total exits
    bool crowd_active     = false;
    bool absence_active   = false;
};

// ─────────────────── Tripwire Statistics ───────────────────
struct TripwireStats {
    int count_in  = 0;   // Forward crossing count
    int count_out = 0;   // Reverse crossing count
};

// ─────────────────── Core Engine ───────────────────
class RegionAnalyzer {
public:
    RegionAnalyzer()  = default;
    ~RegionAnalyzer() = default;

    // ── Configuration ──
    void add_region(Region region, RegionRule rule = {});
    void add_tripwire(Tripwire tw, RegionRule rule = {});
    void set_callback(EventCallback cb);
    void set_track_timeout(float sec);   // Default: 5s

    // ── Per-frame Update ──
    /// Pass in the current frame detection results; the engine handles all region logic automatically
    void update(const ax_result_t& det, int64_t ts_ms = 0);

    // ── Query ──
    int          count_in_region(int region_id) const;
    RegionStats  get_region_stats(int region_id) const;
    TripwireStats get_tripwire_stats(int tw_id) const;

    // ── Management ──
    void reset();                        // Clear all states
    void remove_region(int region_id);
    void remove_tripwire(int tw_id);

private:
    // ── Internal Track State ──
    struct TrackState {
        ax_bbox_t   bbox{};
        cv::Point2f prev_center{};
        cv::Point2f curr_center{};
        int         label = 0;
        TimePoint   first_seen;
        TimePoint   last_seen;
        // Per-region state
        struct PerRegion {
            bool      inside          = false;
            TimePoint enter_time      = {};
            TimePoint last_loiter_fire = {};
        };
        std::unordered_map<int, PerRegion> region_states;
    };

    // ── Internal Region State ──
    struct RegionState {
        Region      def;
        RegionRule  rule;
        RegionStats stats;
        // Absence
        TimePoint   last_occupied_time = Clock::now();
        TimePoint   last_absence_fire  = {};
        bool        crowd_fired        = false;
    };

    struct TripwireState {
        Tripwire      def;
        RegionRule    rule;
        TripwireStats stats;
    };

    // ── Data ──
    mutable std::mutex                          mu_;
    std::unordered_map<unsigned long, TrackState> tracks_;
    std::unordered_map<int, RegionState>         regions_;
    std::unordered_map<int, TripwireState>       tripwires_;
    EventCallback                                callback_;
    float                                        track_timeout_sec_ = 5.f;

    // ── Internal Methods ──
    void fire(const std::vector<RegionEvent>& events, const EventCallback& cb);
    void process_track(unsigned long tid, TrackState& ts, int64_t ts_ms,
                       std::vector<RegionEvent>& events);
    void process_regions_post(int64_t ts_ms, std::vector<RegionEvent>& events);
    void cleanup_tracks(TimePoint now, int64_t ts_ms, std::vector<RegionEvent>& events);
};

}  // namespace region
