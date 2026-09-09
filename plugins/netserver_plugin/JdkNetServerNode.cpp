#include "JdkNetServerNode.hpp"

#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
#include <utility>

#include "PluginFrameUtils.hpp"
#include "post_node_info.h"

namespace jdk_nodes {
#define MAX_FRAME_SIZE (2 * 1024 * 1024)
static uint32_t getTimestamp();

namespace {
void configure_encode_worker_priority(const std::string& task_id) {
	// The SDK's VENC workers run at a higher realtime priority. This userspace
	// feeder only performs bounded queue work plus blocking SendFrame/GetStream;
	// giving it a lower RR priority prevents hundreds of best-effort graph and
	// inference threads from delaying the realtime video path under mixed loads.
	// If the deployment lacks CAP_SYS_NICE, retain a useful best-effort fallback.
	sched_param param{};
	param.sched_priority = 30;
	const int sched_ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
	if (sched_ret == 0) {
		fmt::print("[NetServerAsync] encode worker realtime task={} policy=RR priority={}\n",
			task_id, param.sched_priority);
		return;
	}

	const pid_t tid = static_cast<pid_t>(::syscall(SYS_gettid));
	errno = 0;
	const int nice_ret = ::setpriority(PRIO_PROCESS, static_cast<id_t>(tid), -5);
	fmt::print("[NetServerAsync] encode worker realtime unavailable task={} err={} {}; "
		"nice_fallback={}\n",
		task_id, sched_ret, std::strerror(sched_ret), nice_ret == 0 ? -5 : 0);
}

void configure_output_worker_priority(const std::string& task_id) {
	// Packetization and bounded socket writes must drain encoded frames before
	// the output queue back-pressures VENC. Keep this below the encoder feeder's
	// RR priority, while still protecting it from best-effort inference/post
	// processing bursts. poll()/sendmsg() remain non-blocking and bounded.
	sched_param param{};
	param.sched_priority = 20;
	const int sched_ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
	if (sched_ret == 0) {
		fmt::print("[NetServerAsync] output worker realtime task={} policy=RR priority={}\n",
			task_id, param.sched_priority);
		return;
	}

	const pid_t tid = static_cast<pid_t>(::syscall(SYS_gettid));
	errno = 0;
	const int nice_ret = ::setpriority(PRIO_PROCESS, static_cast<id_t>(tid), -3);
	fmt::print("[NetServerAsync] output worker realtime unavailable task={} err={} {}; "
		"nice_fallback={}\n",
		task_id, sched_ret, std::strerror(sched_ret), nice_ret == 0 ? -3 : 0);
}
}  // namespace

NetServerNode::NetServerNode(std::string node_name, PluginRuntime runtime, int channel_id, bool rtsp_enable, int rtsp_port,
							 std::string user, std::string pass, std::string task_id,
							 size_t encode_queue_capacity)
	: device_id_(runtime.runtime_device_id),
	  channel_id_(channel_id),
	  rtsp_enable_(rtsp_enable),
	  rtsp_port_(rtsp_port),
	  user_(std::move(user)),
	  pass_(std::move(pass)),
	  task_id_(std::move(task_id)),
	  encode_queue_capacity_(std::max<size_t>(1, encode_queue_capacity)),
	  runtime_(std::move(runtime)) {
	// The queue contains compact encoded access units, not 1080P raw frames.
	// Twelve default slots absorb roughly 400 ms of scheduler/client-attach
	// jitter without materially increasing memory. Raw input remains latest-only.
	output_queue_capacity_ = std::clamp<size_t>(encode_queue_capacity_ * 4, 8, 24);
	consumer_id_ = node_name;
	if (rtsp_enable_) {
		rtsp_		= std::make_shared<RTSPServer>("ch1", device_id_, rtsp_port_, VideoCodecType::H264, user_, pass_);
		rtsp_ready_ = rtsp_ && rtsp_->is_ready();
		if (!rtsp_ready_) {
			if (rtsp_) {
				rtsp_->deinit();
				rtsp_.reset();
			}
			throw std::runtime_error(
				"RTSP server failed to listen on allocated port " + std::to_string(rtsp_port_));
		}
	}
	refresh_rtsp_urls(true);
	start_encode_worker();
	start_output_worker();
	printf("NetServerNode constructed! task=%s node=%s rtsp_enable=%d rtsp_port=%d rtsp_url=%s BUILD TIME: %s %s\n",
		   task_id_.c_str(), consumer_id_.c_str(), rtsp_enable_, rtsp_port_, rtsp_url_.c_str(), __DATE__, __TIME__);
	fmt::print("✅ NetServerNode constructed! BUILD TIME: {} {}\n", __DATE__, __TIME__);
}

NetServerNode::~NetServerNode() {
	stop();
	fmt::print("✅ NetServerNode destructed!\n");
}

void NetServerNode::stop() {
	std::lock_guard<std::mutex> lk(mutex_);
	set_alive(false);
	unregister_task_rtsp_output(task_id_.c_str(), consumer_id_.c_str());
	stop_encode_worker();
	stop_output_worker();
	fmt::print("rtsp_ stop ...\n");
	rtsp_ready_				= false;
	rtsp_send_error_logged_ = false;
	rtsp_url_.clear();
	rtsp_urls_.clear();
	if (rtsp_) {
		rtsp_->deinit();
		rtsp_.reset();
	}
	fmt::print("task encoder unsubscribe ...\n");
	unsubscribe_task_encoder(task_id_, consumer_id_);
	fmt::print("✅ NetServerNode stop ok!\n");
}

void NetServerNode::start_encode_worker() {
	bool expected = false;
	if (!encode_running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		return;
	// The worker always selects the newest frame and clears every older entry in
	// encode_loop().  Retaining more than one raw frame therefore cannot improve
	// throughput or absorb jitter: those entries are guaranteed to be discarded.
	// On AX650N every retained frame also borrows one persistent-IVPS output block;
	// across 16 encoded channels the former 2-4 entry queue could pin dozens of
	// 1080P blocks and back-pressure otherwise healthy realtime groups.  Use a
	// strict latest-only slot so replacement releases the stale block immediately.
	encode_input_capacity_ = 1;
	encode_thread_ = std::thread([this]() { encode_loop(); });
}

void NetServerNode::stop_encode_worker() {
	encode_running_.store(false, std::memory_order_release);
	encode_cv_.notify_all();
	if (encode_thread_.joinable())
		encode_thread_.join();
	std::lock_guard<std::mutex> queue_lock(encode_mutex_);
	encode_input_queue_.clear();
}

void NetServerNode::enqueue_encode(std::shared_ptr<AXVideoFrame> frame) {
	if (!frame || !encode_running_.load(std::memory_order_acquire))
		return;
	{
		std::lock_guard<std::mutex> queue_lock(encode_mutex_);
		if (!encode_running_.load(std::memory_order_relaxed))
			return;
		while (encode_input_queue_.size() >= encode_input_capacity_) {
			encode_input_queue_.pop_front();
			encode_input_dropped_.fetch_add(1, std::memory_order_relaxed);
		}
		encode_input_queue_.push_back(PendingEncodeFrame{std::move(frame)});
	}
	encode_cv_.notify_one();
}

void NetServerNode::drain_encoded_output(std::shared_ptr<AXVideoFrame> encoded_frame) {
	TaskEncodeRequest req;
	req.device_id = device_id_;
	req.group = channel_id_;
	req.channel = channel_id_;
	req.rtsp_port = rtsp_port_;
	req.queue_capacity = encode_queue_capacity_;
	req.runtime_location = runtime_.location;
	req.producer_priority = TASK_ENCODER_PRODUCER_REALTIME;

	const size_t max_drain = std::clamp<size_t>(encode_queue_capacity_ + 1, 2, 16);
	size_t delivered = 0;
	while (encoded_frame && delivered < max_drain) {
		const bool encoded_on_axcl = encoded_frame->backend() == VFrameBackend::Axcl ||
									 encoded_frame->memoryDomain() == VFrameMemoryDomain::AxclDevice;
		auto dump_frame = (!encoded_on_axcl && encoded_frame->cpuAccessible())
			? encoded_frame
			: encoded_frame->toHost();
		if (!jdk_plugin::frame_has_host_memory(dump_frame)) {
			fprintf(stderr, "❌ dump_frame is nullptr or getPviraddr() failed!");
			break;
		}

		const size_t sz = dump_frame->size();
		if (sz > 0 && sz < MAX_FRAME_SIZE) {
			enqueue_output(std::move(dump_frame), encoded_frame->pts);
		} else {
			fprintf(stderr, "❌ Encoded frame size %zu exceeds MAX_FRAME_SIZE %d, dropping frame!\n", sz, MAX_FRAME_SIZE);
		}

		++delivered;
		encoded_frame = get_task_encoded_frame(task_id_, consumer_id_, nullptr, req);
	}
}

void NetServerNode::encode_loop() {
	pthread_setname_np(pthread_self(), "netserver-enc");
	configure_encode_worker_priority(task_id_);
	while (true) {
		PendingEncodeFrame pending;
		{
			std::unique_lock<std::mutex> queue_lock(encode_mutex_);
			encode_cv_.wait(queue_lock, [this]() {
				return !encode_running_.load(std::memory_order_acquire) || !encode_input_queue_.empty();
			});
			if (!encode_running_.load(std::memory_order_acquire)) {
				encode_input_queue_.clear();
				break;
			}
			// Realtime output always prefers the newest source frame. If the worker
			// was descheduled briefly, discard stale work before entering VENC.
			pending = std::move(encode_input_queue_.back());
			if (encode_input_queue_.size() > 1) {
				encode_input_dropped_.fetch_add(
					static_cast<uint64_t>(encode_input_queue_.size() - 1),
					std::memory_order_relaxed);
			}
			encode_input_queue_.clear();
		}

		TaskEncodeRequest req;
		req.device_id = device_id_;
		req.group = channel_id_;
		req.channel = channel_id_;
		req.rtsp_port = rtsp_port_;
		req.queue_capacity = encode_queue_capacity_;
		req.runtime_location = runtime_.location;
		req.producer_priority = TASK_ENCODER_PRODUCER_REALTIME;
		auto encoded = get_task_encoded_frame(task_id_, consumer_id_, std::move(pending.frame), req);
		drain_encoded_output(std::move(encoded));
	}
}

void NetServerNode::start_output_worker() {
	bool expected = false;
	if (!output_running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
		return;
	output_thread_ = std::thread([this]() { output_loop(); });
}

void NetServerNode::stop_output_worker() {
	output_running_.store(false, std::memory_order_release);
	output_cv_.notify_all();
	output_space_cv_.notify_all();
	if (output_thread_.joinable())
		output_thread_.join();
	std::lock_guard<std::mutex> queue_lock(output_mutex_);
	output_queue_.clear();
}

void NetServerNode::enqueue_output(std::shared_ptr<AXVideoFrame> frame, AX_U64 pts) {
	if (!frame || !output_running_.load(std::memory_order_acquire))
		return;
	{
		std::unique_lock<std::mutex> queue_lock(output_mutex_);
		// Never discard an already encoded H.264 P-frame: later frames reference
		// it and clients will decode a corrupted GOP. Back-pressure the encoder
		// worker instead. Its raw input is a strict latest-only slot, so overload
		// is shed safely before encoding while memory stays bounded.
		output_space_cv_.wait(queue_lock, [this]() {
			return !output_running_.load(std::memory_order_acquire) ||
				   output_queue_.size() < output_queue_capacity_;
		});
		if (!output_running_.load(std::memory_order_relaxed))
			return;
		output_queue_.push_back(PendingOutputFrame{std::move(frame), pts});
		output_enqueued_.fetch_add(1, std::memory_order_relaxed);
	}
	output_cv_.notify_one();
}

void NetServerNode::output_loop() {
	pthread_setname_np(pthread_self(), "netserver-out");
	configure_output_worker_priority(task_id_);
	auto last_log = std::chrono::steady_clock::now();
	uint64_t last_enqueued = 0;
	uint64_t last_sent = 0;
	uint64_t last_dropped = 0;
	uint64_t last_failed = 0;
	uint64_t last_encode_input_dropped = 0;
	while (true) {
		PendingOutputFrame pending;
		{
			std::unique_lock<std::mutex> queue_lock(output_mutex_);
			output_cv_.wait(queue_lock, [this]() {
				return !output_running_.load(std::memory_order_acquire) || !output_queue_.empty();
			});
			if (!output_running_.load(std::memory_order_acquire)) {
				output_queue_.clear();
				output_space_cv_.notify_all();
				break;
			}
			pending = std::move(output_queue_.front());
			output_queue_.pop_front();
		}
		output_space_cv_.notify_one();

		if (!pending.frame || !jdk_plugin::frame_has_host_memory(pending.frame)) {
			output_send_failed_.fetch_add(1, std::memory_order_relaxed);
			continue;
		}
		const size_t sz = pending.frame->size();
		auto* data = reinterpret_cast<uint8_t*>(pending.frame->getPviraddr());
		if (sz == 0 || sz >= MAX_FRAME_SIZE || data == nullptr) {
			output_send_failed_.fetch_add(1, std::memory_order_relaxed);
			continue;
		}

		bool rtsp_ok = true;
		if (rtsp_enable_ && rtsp_ready_ && rtsp_ && rtsp_->is_ready())
			rtsp_ok = rtsp_->send_nalu(data, static_cast<int>(sz), pending.pts);

		SdkFrame sdk_frame{};
		sdk_frame.size = sz;
		sdk_frame.frameData = data;
		sdk_frame.presentationTs = pending.pts != 0
			? static_cast<uint32_t>(pending.pts)
			: getTimestamp();
		SdkWriteVideoFrame(task_id_.c_str(), &sdk_frame);
		if (rtsp_ok) {
			output_sent_.fetch_add(1, std::memory_order_relaxed);
		} else {
			output_send_failed_.fetch_add(1, std::memory_order_relaxed);
		}

		const auto now = std::chrono::steady_clock::now();
		if (now - last_log >= std::chrono::seconds(5)) {
			const uint64_t enqueued = output_enqueued_.load(std::memory_order_relaxed);
			const uint64_t sent = output_sent_.load(std::memory_order_relaxed);
			const uint64_t dropped = output_dropped_.load(std::memory_order_relaxed);
			const uint64_t failed = output_send_failed_.load(std::memory_order_relaxed);
			const uint64_t encode_input_dropped = encode_input_dropped_.load(std::memory_order_relaxed);
			double elapsed = std::chrono::duration<double>(now - last_log).count();
			size_t queued = 0;
			{
				std::lock_guard<std::mutex> queue_lock(output_mutex_);
				queued = output_queue_.size();
			}
			const double enqueue_fps = (enqueued - last_enqueued) / elapsed;
			const double send_fps = (sent - last_sent) / elapsed;
			output_delivery_fps_.store(
				static_cast<uint32_t>(std::max<long>(0, std::lround(send_fps))),
				std::memory_order_relaxed);
			std::cout << "[NetServerAsync] task=" << task_id_
				<< " enqueue_fps=" << enqueue_fps
				<< " send_fps=" << send_fps
				<< " dropped=" << (dropped - last_dropped)
				<< " encode_input_dropped=" << (encode_input_dropped - last_encode_input_dropped)
				<< " failed=" << (failed - last_failed)
				<< " queued=" << queued << "\n";
			last_enqueued = enqueued;
			last_sent = sent;
			last_dropped = dropped;
			last_failed = failed;
			last_encode_input_dropped = encode_input_dropped;
			last_log = now;
		}
	}
}

void NetServerNode::refresh_rtsp_urls(bool force) {
	const auto now = std::chrono::steady_clock::now();
	if (!force && last_rtsp_url_refresh_.time_since_epoch().count() != 0 &&
		now - last_rtsp_url_refresh_ < std::chrono::seconds(5)) {
		return;
	}
	last_rtsp_url_refresh_ = now;

	std::vector<std::string> urls;
	if (rtsp_enable_ && rtsp_ready_ && rtsp_ && rtsp_->is_ready())
		urls = rtsp_->rtsp_urls();
	const std::string primary = urls.empty() ? std::string() : urls.front();
	if (!force && urls == rtsp_urls_ && primary == rtsp_url_)
		return;

	rtsp_urls_ = std::move(urls);
	rtsp_url_  = primary;
	register_task_rtsp_output(task_id_.c_str(), consumer_id_.c_str(),
							  rtsp_url_.c_str(), "H264", rtsp_port_);
	const std::string urls_json = nlohmann::json(rtsp_urls_).dump();
	register_task_rtsp_output_urls(task_id_.c_str(), consumer_id_.c_str(),
								   urls_json.c_str());
	reporter_.set_output_rtsp_config({task_id_,
									  PLUGIN_NODE_NAME,
									  rtsp_url_.empty() ? "N/A" : rtsp_url_,
									  "H264"});
}
static uint32_t getTimestamp() {
	struct timeval tv = {0};
	gettimeofday(&tv, NULL);
	uint32_t ts = ((tv.tv_sec * 1000) + ((tv.tv_usec + 500) / 1000)) * 90;	// clockRate/1000;
	return ts;
}

// handle frame meta one by one
std::shared_ptr<jdk_objects::jdk_meta> NetServerNode::handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) {
	// fmt::print("NetServerNode handle_frame_meta, meta create_time: {}, frame pts: {}\n", meta->create_time, meta->frame_ ? meta->frame_->pts : -1);
	if (!meta || (!meta->frame_)) {
		std::cerr << "❌ meta is null or contains no valid frame.\n";
		return jdk_node_base::handle_frame_meta(meta);
	}
	std::shared_ptr<AXVideoFrame> frame =
		meta->dump_frame_ ? meta->dump_frame_  // use decorated frame if we have one
						  : meta->frame_;	   // otherwise fall back to raw frame

	if (!frame) {  // extra safety‑guard
		fprintf(stderr, "❌ NetServerNode received a nullptr frame, skipping\n");
		return jdk_node_base::handle_frame_meta(meta);
	}
	// Automatic timing, write the time spent to reporter_ at the end of the function.
	MetricsReporter::ScopedTimer timer(reporter_);
	// auto						 last_time = std::chrono::system_clock::now();
	std::lock_guard<std::mutex> lk(mutex_);
	if (!is_alive()) {
		fmt::print("NetServerNode is not alive, skipping frame_meta handling.\n");
		return nullptr;
	}
	refresh_rtsp_urls(false);

	// VENC and encoded-frame host copies are intentionally kept off the JDK graph
	// worker. jdk_node uses a bounded DropOldest input queue; doing synchronous
	// encode here made the graph discard otherwise healthy realtime frames under
	// 16-channel load. The dedicated worker still uses TaskEncoderHub, so Record
	// and NetServer share exactly one encoder and one encoded access unit.
	enqueue_encode(frame);

	std::string Resolution	   = std::to_string(frame->width()) + "x" + std::to_string(frame->height());
	const uint32_t delivered_fps = output_delivery_fps_.load(std::memory_order_relaxed);
	const int fps_for_report = delivered_fps > 0
		? static_cast<int>(delivered_fps)
		: std::max(0, jdk_node_base::node_fps());
	reporter_.report_output_rtsp(Resolution, fps_for_report, meta->create_time);
	// fmt::print("NetServerNode handle_frame_meta done, meta create_time: {}, frame pts: {}, resolution: {}, fps: {}\n",
	//            meta->create_time, frame->pts, Resolution, fps_for_report);
	return jdk_node_base::handle_frame_meta(meta);
}

void NetServerNode::handle_frame_meta(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch) {
	const auto& frame_meta_with_batch = meta_with_batch;
	// run_infer_combinations(frame_meta_with_batch);
}

std::shared_ptr<jdk_objects::jdk_meta> NetServerNode::handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) {
	if (!meta)
		return nullptr;
	std::cout << "[NetServerNode] handle_control_meta: control_type = " << meta->control_type << std::endl;
	if (meta->control_type == jdk_objects::jdk_control_type::SPEAK) {
		stop();
		fmt::print("✅ NetServerNode stop ok!\n");
	}
	return jdk_node_base::handle_control_meta(meta);
}

}  // namespace jdk_nodes
