#pragma once

#include <any>
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <unordered_map>
#include <vector>

#include "AxVideoFrame.hpp"
#include "jdk_meta.hpp"
#include "json.hpp"
/*
 * ##########################################
 * how does frame meta work?
 * ##########################################
 * frame meta, holding all data(targets/elements/...) of current frame in the video scene. frame meta are independent and don't know about each other, neither its previous frames nor next frames.
 * the data in frame meta is just telling us what **current frame** is so we can not get something like 'state-switch' from a single frame meta.
 * if you need know when the 'state-switch' happen, for example, you want to notify to cloud via restful api if state changed(ignore if it's keeping),
 * you need cache previous frame meta(maybe partial data) in your custom node first and then compare with each other to figure out if it has changed.
 *
 * frame meta works like our eyes, by taking a glance at the frame in video we can see what the picture is and how many targets are there.
 * but if you want to  know something like state-switch, for example, a person was walking and then stop or it stop for a while and then start to walk, you have to see(cache) more frames.
 *
 * see more implementation of 'jdk_track_node' and 'jdk_message_broker_node' which saved history frame meta data and then work based on them.
 * 1. jdk_track_node          : save previous locations of targets and then do tracking based on them, we need see more frames to track targets in video.
 * 2. jdk_message_broker_node : save previous ba_flags and then do notifying based on them, we need see more frames to check if state-switch has happened.
 * ##########################################
 */
namespace jdk_objects {
enum class MetaType {
	CamFrame,
	ImageFrame,
	FileFrame,
	InferFrame,
	// other types...
};

struct ResultEntry {
	using RenderFn = std::function<void(std::shared_ptr<AXVideoFrame>&,
										const std::any&, const std::any&)>;
	using AlarmFn  = std::function<
		 std::pair<nlohmann::json, std::vector<std::function<std::shared_ptr<AXVideoFrame>()>>>(const std::any&)>;

	std::shared_ptr<std::any> result;
	RenderFn				  render_fn;
	AlarmFn					  alarm_fn;
	bool					  push_enabled{false};
	int						  push_interval_ms{0};

	ResultEntry(std::shared_ptr<std::any> r,
				RenderFn				  render,
				AlarmFn					  alarm,
				bool					  push,
				int						  interval) noexcept
		: result(std::move(r)),
		  render_fn(std::move(render)),
		  alarm_fn(std::move(alarm)),
		  push_enabled(push),
		  push_interval_ms(interval) {}

	/* values are forbidden to copy to prevent destruction errors after shallow copying */
	ResultEntry(const ResultEntry&)			   = delete;
	ResultEntry& operator=(const ResultEntry&) = delete;
	/*movement is allowed */
	ResultEntry(ResultEntry&&)			  = default;
	ResultEntry& operator=(ResultEntry&&) = default;
};

template <class T>
class AtomicPtr {
	std::atomic<std::shared_ptr<T>> p_{};

public:
	AtomicPtr() noexcept = default;
	/* allows direct construction assignment with shared ptr */
	AtomicPtr(std::shared_ptr<T> sp) noexcept { store(std::move(sp)); }

	/*copy move just atomic load/store */
	AtomicPtr(const AtomicPtr& other) noexcept { store(other.load()); }
	AtomicPtr(AtomicPtr&& other) noexcept { store(other.exchange({})); }
	AtomicPtr& operator=(const AtomicPtr& other) noexcept {
		store(other.load());
		return *this;
	}
	AtomicPtr& operator=(AtomicPtr&& other) noexcept {
		store(other.exchange({}));
		return *this;
	}

	/* basic operations */
	std::shared_ptr<T> load(std::memory_order o = std::memory_order_acquire) const noexcept {
		return p_.load(o);
	}
	void store(std::shared_ptr<T> sp,
			   std::memory_order  o = std::memory_order_release) noexcept {
		p_.store(std::move(sp), o);
	}
	std::shared_ptr<T> exchange(std::shared_ptr<T> sp,
								std::memory_order  o = std::memory_order_acq_rel) noexcept {
		return p_.exchange(std::move(sp), o);
	}

	/* convenient：implicit conversion is allowed shared_ptr */
	operator std::shared_ptr<T>() const noexcept { return load(); }
};

using result_map_t = std::unordered_map<std::string, AtomicPtr<ResultEntry>>;
// frame meta, which contains frame-related data. it is kind of important meta in pipeline.
class jdk_frame_meta : public jdk_meta {
private:
	/* data */
	MetaType meta_type_;

public:
	jdk_frame_meta(/*cv::Mat frame*/ std::shared_ptr<AXVideoFrame> frame, int frame_index = -1, int channel_index = -1, int original_width = 0, int original_height = 0, int fps = 0, uint64_t pts = 0, MetaType type = MetaType::FileFrame);
	~jdk_frame_meta();

	// define copy constructor since we need deep copy operation.
	// jdk_frame_meta(const jdk_frame_meta& meta);

	// frame the meta belongs to, filled by src nodes.
	int frame_index;

	// fps for current video.
	int fps;

	// orignal frame width, fiiled by src nodes.
	int original_width;
	// original frame height, filled by src nodes.
	int original_height;

	// image data the meta holds, filled by src nodes.
	// deep copy needed here for this member.

	std::shared_ptr<AXVideoFrame> frame_;
	result_map_t				  result_map_;
	//  used to protect result map and other fields that require serial access
	std::shared_ptr<std::mutex>	  mtx_ = std::make_shared<std::mutex>();  //
	uint64_t					  timestamp_;							  //  microseconds
	std::shared_ptr<AXVideoFrame> dump_frame_;
	// get target ptrs by target ids in current frame, ONLY supports jdk_frame_target
	// std::vector<std::shared_ptr<jdk_frame_target>> get_targets_by_ids(const std::vector<int>& ids);

	// copy myself
	virtual std::shared_ptr<jdk_meta> clone() override;

	void set_type(MetaType type);

	MetaType type() const;
};

}  // namespace jdk_objects