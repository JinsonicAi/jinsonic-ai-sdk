#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>  // For thread safety
#include <unordered_map>
#include <vector>

class DumpWriter {
public:
	explicit DumpWriter(const char* path);
	~DumpWriter();
	void write(const void* p, size_t n);

private:
	FILE* fp_{nullptr};
};

class H26xDepayAssembler {
public:
	enum class Codec { Unknown = 0,
					   H264	   = 1,
					   H265	   = 2 };

	struct Frame {
		std::vector<uint8_t> data;
		bool				 is_idr{false};
		uint32_t			 ts{0};
		Codec				 codec{Codec::Unknown};
	};

	using FrameReadyCB = std::function<void(const Frame&)>;

	explicit H26xDepayAssembler(Codec codec = Codec::Unknown, FrameReadyCB cb = nullptr);
	void  set_callback(FrameReadyCB cb);
	void  set_codec(Codec c);
	Codec codec() const;

	// Thread-safe push method for data processing
	void push(const uint8_t* buf, int len, uint32_t ts, uint16_t seq);
	void flush();

private:
	struct Packet {
		std::vector<uint8_t> bytes;
		uint16_t			 seq;
		bool				 parsed = false;
	};

	struct FuState {
		std::vector<uint8_t> buf;
		bool				 started = false;
	};

	struct Bucket {
		std::map<uint16_t, Packet>		  pkts;
		std::vector<std::vector<uint8_t>> nalus;
		std::unordered_map<int, FuState>  fuStates;
		bool							  saw_aud = false, saw_slice = false, saw_idr = false, incomplete = false;
	};

	Codec								 codec_{Codec::Unknown};
	FrameReadyCB						 on_frame_;
	std::unordered_map<uint32_t, Bucket> buckets_;
	uint32_t							 active_ts_{0};
	bool								 has_active_ts_{false};
	std::vector<uint8_t>				 vps_, sps_, pps_;

	std::mutex push_mutex_;	 // Mutex for thread safety during data push operations

	static inline void	  pushStartCode(std::vector<uint8_t>& out);
	static inline int	  annexb_prefix_len(const uint8_t* p, int len);
	static inline bool	  has_start_code(const uint8_t* p, int len);
	static inline uint8_t h264_type(const uint8_t* p);
	static inline bool	  h264_is_fu_a(uint8_t t);
	static inline bool	  h264_is_stap_a(uint8_t t);
	static inline bool	  h264_is_slice(uint8_t t);
	static inline bool	  h264_is_idr(uint8_t t);
	static inline bool	  h264_is_aud(uint8_t t);
	static inline bool	  h264_is_sps(uint8_t t);
	static inline bool	  h264_is_pps(uint8_t t);
	static inline uint8_t h265_type(const uint8_t* p);
	static inline bool	  h265_is_fu(uint8_t t);
	static inline bool	  h265_is_ap(uint8_t t);
	static inline bool	  h265_is_slice_type(uint8_t t);
	static inline bool	  h265_is_idr(uint8_t t);
	static inline bool	  h265_is_aud(uint8_t t);
	static inline bool	  h265_is_vps(uint8_t t);
	static inline bool	  h265_is_sps(uint8_t t);
	static inline bool	  h265_is_pps(uint8_t t);

	void	   resetBucketFor(uint32_t ts);
	void	   parsePacketsForTs(uint32_t ts);
	Codec	   detect_codec_from_nalu(const uint8_t* n, int nlen);
	void	   parseAnnexB(const uint8_t* buf, int len, Bucket& b);
	static int findStartCode(const uint8_t* p, int len, int pos);
	void	   parseRtpH264(const uint8_t* p, int len, Bucket& b);
	void	   parseRtpH265(const uint8_t* p, int len, Bucket& b);
	void	   handleNALU_H264(const uint8_t* n, int nlen, Bucket& b);
	void	   handleNALU_H265(const uint8_t* n, int nlen, Bucket& b);
	void	   finalizeActiveIfComplete();
};
