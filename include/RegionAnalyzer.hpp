#pragma once
/**
 * @file   RegionAnalyzer.hpp
 * @brief  Generic region analysis engine — commercial-grade wrapper
 *
 * Supported scenarios:
 *   1. Intrusion detection (entry event → instant/delayed alarm)
 *   2. Absence detection (area empty too long → alarm)
 *   3. Loitering detection (target stays too long → alarm)
 *   4. Crowd alarm (target count exceeds threshold → alarm)
 *   5. Line crossing detection (target crosses virtual line → alarm)
 *   6. Region statistics (real-time target count / in/out totals)
 *
 * Usage (plugin side):
 *   RegionAnalyzer analyzer;
 *   analyzer.add_region(region, rule);
 *   analyzer.set_callback([](const RegionEvent& e){ ... });
 *   // called each frame
 *   analyzer.update(det);
 */

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <unordered_map>
#include <vector>

#include "ax_algorithm_sdk.h"

namespace region {

// ─────────────────── Time utilities ───────────────────
using Clock		= std::chrono::steady_clock;
using TimePoint = Clock::time_point;

inline int64_t now_ms() {
	return std::chrono::duration_cast<std::chrono::milliseconds>(
			   Clock::now().time_since_epoch())
		.count();
}

// ─────────────────── Geometry utilities ───────────────────
/// ax_bbox_t → cv::Rect (integer conversion)
inline cv::Rect to_cv_rect(const ax_bbox_t& b) noexcept {
	return cv::Rect(static_cast<int>(std::lround(b.x)),
					static_cast<int>(std::lround(b.y)),
					static_cast<int>(std::lround(b.w)),
					static_cast<int>(std::lround(b.h)));
}

/// bbox center point
inline cv::Point2f bbox_center(const ax_bbox_t& b) noexcept {
	return {b.x + b.w * 0.5f, b.y + b.h * 0.5f};
}

/// whether the center point is inside the polygon
inline bool point_in_polygon(const cv::Point2f&			   pt,
							 const std::vector<cv::Point>& poly) {
	return cv::pointPolygonTest(poly, pt, false) >= 0;
}

/// whether the center point is inside any region (empty region list means full frame)
inline bool in_any_region(const ax_bbox_t&							 b,
						  const std::vector<std::vector<cv::Point>>& regions) {
	if (regions.empty())
		return true;
	auto c = bbox_center(b);
	for (const auto& poly : regions)
		if (point_in_polygon(c, poly))
			return true;
	return false;
}

/// whether bbox area is smaller than the threshold
inline bool bbox_too_small(const ax_bbox_t& b, float min_size) noexcept {
	return b.w * b.h < min_size * min_size;
}

// ─────────────────── Line crossing utilities ───────────────────
/// check whether two segments intersect (p1-p2 with p3-p4)
bool segments_intersect(const cv::Point2f& p1, const cv::Point2f& p2,
						const cv::Point2f& p3, const cv::Point2f& p4);

/// determine crossing direction: +1 forward / -1 backward / 0 no crossing
int cross_direction(const cv::Point2f& prev_center,
					const cv::Point2f& curr_center,
					const cv::Point2f& line_p1,
					const cv::Point2f& line_p2);

// ─────────────────── Event definitions ───────────────────
enum class EventType : uint8_t {
	Enter		   = 0,	 // target entered region
	Leave		   = 1,	 // target left region
	Loiter		   = 2,	 // loitering (dwell timeout)
	Absence		   = 3,	 // absence (region empty timeout)
	Crowd		   = 4,	 // crowd (exceeds threshold)
	CrowdRecover   = 5,	 // crowd recovered
	LineCrossIn	   = 6,	 // line cross (forward)
	LineCrossOut   = 7,	 // line cross (backward)
	AbsenceRecover = 8,	 // absence recovered
};

struct RegionEvent {
	EventType	  type;
	int			  region_id	   = -1;
	unsigned long track_id	   = 0;
	ax_bbox_t	  bbox		   = {};
	int			  label		   = 0;
	int64_t		  timestamp_ms = 0;
	int			  object_count = 0;	   // current target count in region
	float		  dwell_sec	   = 0.f;  // time target has stayed in region
	float		  absence_sec  = 0.f;  // duration region has been empty
};

using EventCallback = std::function<void(const RegionEvent&)>;

// ─────────────────── Region rules ───────────────────
struct RegionRule {
	// basic switches
	bool enable_enter = true;  // trigger enter event
	bool enable_leave = true;  // trigger leave event

	// loitering detection
	bool  enable_loiter	  = false;
	float loiter_sec	  = 10.f;  // trigger when dwell time exceeds this
	float loiter_cooldown = 30.f;  // cooldown before same track can trigger again

	// absence detection
	bool  enable_absence   = false;
	float absence_sec	   = 30.f;	// trigger when region empty longer than this
	float absence_cooldown = 60.f;	// cooldown before next trigger

	// crowd detection
	bool enable_crowd	   = false;
	int	 crowd_threshold   = 10;  // trigger when count exceeds this
	int	 crowd_recover_gap = 2;	  // recover when count drops to threshold - gap

	// line crossing detection
	bool enable_line_cross = false;

	// filtering
	float min_bbox_size = 0.f;	// minimum bbox side length (pixels)
	int	  label_filter	= -1;	// -1 = no filtering; >=0 only track this label
};

// ─────────────────── Region definition ───────────────────
struct Region {
	int					   id = 0;
	std::string			   name;
	std::vector<cv::Point> polygon;	 // polygon vertices (≥3)
};

// ─────────────────── Tripwire definition ───────────────────
struct Tripwire {
	int			id = 0;
	std::string name;
	cv::Point2f p1, p2;	 // line segment endpoints
};

// ─────────────────── Region statistics ───────────────────
struct RegionStats {
	int	 current_count	= 0;  // current target count in region
	int	 total_enter	= 0;  // total entered
	int	 total_leave	= 0;  // total left
	bool crowd_active	= false;
	bool absence_active = false;
};

// ─────────────────── Tripwire statistics ───────────────────
struct TripwireStats {
	int count_in  = 0;	// forward crossings count
	int count_out = 0;	// backward crossings count
};

// ─────────────────── 核心引擎 ───────────────────
class RegionAnalyzer {
public:
	RegionAnalyzer()  = default;
	~RegionAnalyzer() = default;

	// ── Configuration ──
	void add_region(Region region, RegionRule rule = {});
	void add_tripwire(Tripwire tw, RegionRule rule = {});
	void set_callback(EventCallback cb);
	void set_track_timeout(float sec);	// default 5s

	// ── Per-frame update ──
	/// pass current frame detections; engine handles all region logic
	void update(const ax_result_t& det, int64_t ts_ms = 0);

	// ── Queries ──
	int			  count_in_region(int region_id) const;
	RegionStats	  get_region_stats(int region_id) const;
	TripwireStats get_tripwire_stats(int tw_id) const;

	// ── Management ──
	void reset();  // clear all state
	void remove_region(int region_id);
	void remove_tripwire(int tw_id);

private:
	// ── Internal track state ──
	struct TrackState {
		ax_bbox_t	bbox{};
		cv::Point2f prev_center{};
		cv::Point2f curr_center{};
		int			label = 0;
		TimePoint	first_seen;
		TimePoint	last_seen;
		// per-region state
		struct PerRegion {
			bool	  inside		   = false;
			TimePoint enter_time	   = {};
			TimePoint last_loiter_fire = {};
		};
		std::unordered_map<int, PerRegion> region_states;
	};

	// ── Internal region state ──
	struct RegionState {
		Region		def;
		RegionRule	rule;
		RegionStats stats;
		// absence
		TimePoint last_occupied_time = Clock::now();
		TimePoint last_absence_fire	 = {};
		bool	  crowd_fired		 = false;
	};

	struct TripwireState {
		Tripwire	  def;
		RegionRule	  rule;
		TripwireStats stats;
	};

	// ── 数据 ──
	mutable std::mutex							  mu_;
	std::unordered_map<unsigned long, TrackState> tracks_;
	std::unordered_map<int, RegionState>		  regions_;
	std::unordered_map<int, TripwireState>		  tripwires_;
	EventCallback								  callback_;
	float										  track_timeout_sec_ = 5.f;

	// ── 内部方法 ──
	void fire(const std::vector<RegionEvent>& events, const EventCallback& cb);
	void process_track(unsigned long tid, TrackState& ts, int64_t ts_ms,
					   std::vector<RegionEvent>& events);
	void process_regions_post(int64_t ts_ms, std::vector<RegionEvent>& events);
	void cleanup_tracks(TimePoint now, int64_t ts_ms, std::vector<RegionEvent>& events);
};

}  // namespace region
