#pragma once
#include <chrono>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <atomic>
#include <vector>
#include <condition_variable>
#include <deque>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>

#include "HwEncoder.hpp"
#include "TimestampedRingBuffer.hpp"

class Task {
public:
	Task(const std::string& id);
	using TimePoint	 = std::chrono::steady_clock::time_point;
	using RingBuffer = TimestampedRingBuffer<AXVideoFrame>;
	struct ConsumerConfig {
		size_t queue_capacity{3};
		bool prefer_realtime{true};
	};
	std::string																   task_id;
	std::unordered_map<std::string, std::shared_ptr<jdk_nodes::jdk_node_base>> nodes;

	std::shared_ptr<HwEncoder> getEncoder(int device_id, int group, int channel, int rtsp_port,
										  const std::string& plugin_name, size_t buffer_size = 3);

	std::shared_ptr<AXVideoFrame> getFrame(
		std::shared_ptr<AXVideoFrame> input_frame,
		TimePoint&					  last_time,
		const std::string&			  plugin_name);
	std::shared_ptr<AXVideoFrame> getFrame(
		std::shared_ptr<AXVideoFrame> input_frame,
		const std::string&			  plugin_name);
	void unsubscribe(const std::string& plugin_name);

	void add_node(const std::shared_ptr<jdk_nodes::jdk_node_base>& node);
	void start();
	void pause();
	void stop();

private:
	// stop 幂等/防重入：避免 stop 过程中再次 stop 导致重复 detach/deinit 引发死锁或长时间阻塞
	std::atomic<bool> stopping_{false};
	std::atomic<bool> stopped_{false};

	std::shared_ptr<HwEncoder>	encoder_;
	std::shared_ptr<RingBuffer> frame_buffer_;
	std::mutex					encoder_mutex_;
	std::condition_variable		encode_cv_;
	int							encoder_device_id_{std::numeric_limits<int>::min()};
	int							encoder_group_{std::numeric_limits<int>::min()};
	int							encoder_channel_{std::numeric_limits<int>::min()};
	int							encoder_rtsp_port_{std::numeric_limits<int>::min()};
	bool						encoding_in_progress_{false};
	uint64_t					last_encoded_pts_{std::numeric_limits<uint64_t>::max()};
	uint64_t					last_encoded_seq_{std::numeric_limits<uint64_t>::max()};
	uint64_t					seq_{0};

	struct EncodedPacket {
		std::shared_ptr<AXVideoFrame> frame;
		uint64_t pts{0};
		uint64_t seq{0};
		bool keyframe{false};
		TimePoint ts;
	};
	struct SubscriberSlot {
		ConsumerConfig cfg{};
		std::deque<std::shared_ptr<EncodedPacket>> q;
		uint64_t dropped{0};
		uint64_t consumed{0};
		TimePoint last_consume_ts{TimePoint::min()};
	};
	std::unordered_map<std::string, SubscriberSlot> subscribers_;
	std::deque<std::shared_ptr<EncodedPacket>> gop_cache_;
	size_t max_gop_cache_{120};

	void ensure_subscriber_locked(const std::string& plugin_name, ConsumerConfig cfg);
	void ensure_subscriber_locked(const std::string& plugin_name);
	void publish_packet_locked(const std::shared_ptr<EncodedPacket>& pkt);
	static bool is_idr_from_annexb(const uint8_t* data, size_t len);
};

struct TaskEncodeRequest {
	int	   device_id{-1};
	int	   group{1};
	int	   channel{0};
	int	   rtsp_port{8554};
	size_t queue_capacity{3};
};

std::shared_ptr<Task> get_task(const std::string& task_id);
std::shared_ptr<AXVideoFrame> get_task_encoded_frame(
	const std::string&			task_id,
	const std::string&			consumer_id,
	std::shared_ptr<AXVideoFrame> input_frame,
	TaskEncodeRequest			req = TaskEncodeRequest());
void unsubscribe_task_encoder(const std::string& task_id, const std::string& consumer_id);
