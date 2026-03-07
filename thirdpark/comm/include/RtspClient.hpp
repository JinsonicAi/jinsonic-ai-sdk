#pragma once
#include <atomic>
#include <cstdint>
#include <fstream>
#include <functional>
#include <functional>  // bind_front
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
}

/** single channel instance configuratio n(time unit:millisecond) */
struct RtspPullOptions {
	std::string url;							   // RTSP URL
	std::string dump_path;						   // debugging disk writing(leave blank to close)，the document will be Annex-B
	bool		prefer_tcp			 = true;	   // RTSP over TCP
	bool		low_latency			 = true;	   // nobuffer / reorder_queue_size=0
	int			open_timeout_ms		 = 3000;	   // open timeout
	int			rw_timeout_ms		 = 3000;	   // read and write timeout
	int			max_delay_ms		 = 200;		   // demux maximum cache
	int			reconnect_interval	 = 1500;	   // reconnection interval
	int			analyzeduration_ms	 = 100;		   // flow analysis time
	int			probesize_bytes		 = 32 * 1024;  // detect size
	bool		inject_ps_before_idr = true;	   // optional: inject the last seen in idr before VPS/SPS/PPS(as your hardware decoding requires)
};

/** encoded frame information（undecoded） */
struct EncodedAU {
	const uint8_t *data		= nullptr;	// valid only during the callback period
	int			   size		= 0;
	bool		   key		= false;  // is it a key frame such as idr
	int			   codec_id = 0;	  // AV_CODEC_ID_HEVC / H264 ...
	int			   width	= 0;	  // from stream
	int			   height	= 0;
	int64_t		   pts		= AV_NOPTS_VALUE;
	AVRational	   time_base{1, 1000};
};

/** streaming type only outputs complete au encoded frames does not decode */
class RtspPull {
public:
	using AUCallback = std::function<void(const EncodedAU &au)>;

	explicit RtspPull(RtspPullOptions opt) noexcept;
	~RtspPull() noexcept;

	RtspPull(const RtspPull &)			  = delete;
	RtspPull &operator=(const RtspPull &) = delete;

	bool start() noexcept;	// non throwable exception
	void stop() noexcept;

	void setCallback(AUCallback cb) noexcept;

	bool isRunning() const noexcept { return running_; }

private:
	void	   runLoop(std::stop_token, std::atomic<bool> &) noexcept;
	static int interruptThunk(void *opaque) noexcept;
	int		   interruptCb() noexcept;

	// resources
	void closeAll() noexcept;
	bool openBSF() noexcept;  // Annex-B Standardization
	void closeBSF() noexcept;

	// VPS/SPS/PPS capture & injection optional
	void scanAndCachePS(const uint8_t *data, int size, int codec_id) noexcept;
	bool needInjectPS(bool key) const noexcept { return opt_.inject_ps_before_idr && key; }
	void makeWithPS(const uint8_t *data, int size, int codec_id, std::vector<uint8_t> &out) noexcept;

private:
	RtspPullOptions	  opt_;
	std::jthread	  th_;
	std::atomic<bool> running_{false};
	std::atomic<bool> stopping_{false};

	// callback
	std::mutex cb_mtx_;
	AUCallback cb_;
	std::mutex res_mtx_;

	AVFormatContext *fmt_	 = nullptr;
	int				 vindex_ = -1;
	AVBSFContext	*bsf_	 = nullptr;	 // unified transfer Annex-B
	std::ofstream	 dump_;

	// io can be interrupted
	std::atomic<long long> io_deadline_ms_{0};

	// recently seen vps sps pps simple cache
	std::vector<uint8_t> vps_, sps_, pps_;
};

/** multi instance manager dynamic creation/deletion */
class RtspPullManager {
public:
	using Id = uint64_t;
	Id	 create(const RtspPullOptions &opt, RtspPull::AUCallback cb = {}) noexcept;
	void destroy(Id id) noexcept;

private:
	std::mutex										  mtx_;
	Id												  next_{1};
	std::unordered_map<Id, std::unique_ptr<RtspPull>> map_;
};
