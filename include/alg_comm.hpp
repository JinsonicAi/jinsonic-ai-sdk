#pragma once
#include <ax_engine_api.h>
#include <ax_ivps_api.h>
#include <ax_sys_api.h>
#include <axcl.h>

#include <any>
#include <cstring>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <optional>
#include <random>

#include "AxVideoFrame.hpp"
#include "HwCapture.hpp"
#include "HwIvps.hpp"
#include "ax_algorithm_sdk.h"
#include "ax_engine_api.h"
#include "axcl.h"
#include "streamInfo.hpp"

#define ALIGN_UP(x, align) (((x) + ((align) - 1)) & ~((align) - 1))

class AxImage {
public:
	AxImage(cv::Mat &mat, int device_id)
		: device_id_(device_id), valid_(false) {
		preprocessImage(mat);
		// cv::imwrite("ax_create_image.jpg", mat);
		if (ax_create_image(mat.cols, mat.rows, mat.cols,
							ax_color_space_rgb, &img_, device_id_) == 0) {
			std::memcpy(img_.pVir, mat.data, img_.nSize);
			valid_ = true;
		}
	}
	void preprocessImage(cv::Mat &image) {
		cv::resize(image, image, cv::Size(ALIGN_UP(image.cols, 128), ALIGN_UP(image.rows, 128)));
		cv::cvtColor(image, image, cv::COLOR_BGR2RGB);
	}
	// copying is prohibited movement is allowed
	AxImage(const AxImage &)			= delete;
	AxImage &operator=(const AxImage &) = delete;

	AxImage(AxImage &&other) noexcept
		: img_(other.img_), device_id_(other.device_id_), valid_(other.valid_) {
		other.valid_ = false;
	}

	AxImage &operator=(AxImage &&other) noexcept {
		if (this != &other) {
			cleanup();
			img_		 = other.img_;
			device_id_	 = other.device_id_;
			valid_		 = other.valid_;
			other.valid_ = false;
		}
		return *this;
	}

	~AxImage() { cleanup(); }

	ax_image_t		 *get() { return &img_; }
	const ax_image_t *get() const { return &img_; }

	bool valid() const { return valid_; }

private:
	void cleanup() {
		if (valid_) {
			ax_release_image(&img_, device_id_);
			valid_ = false;
		}
	}

	ax_image_t img_{};
	int		   device_id_;
	bool	   valid_;
};

class SafeAlgorithm {
public:
	struct Options {
		ax_model_type_e model_type{};
		std::string		model_path;
		int				device_id{-1};

		// NPU configuration（if not filled in it will default AX_ENGINE_VIRTUAL_NPU_STD）
		AX_ENGINE_NPU_ATTR_T npu_attr{};
		bool				 use_default_npu{true};

		// initialize retry strategy
		int						  max_retries{5};
		std::chrono::milliseconds retry_delay{1000};
	};

	// not void：return optional<R>
	template <class F>
	auto call_algo(F &&f)
		-> std::optional<std::invoke_result_t<F &>>
		requires(!std::is_void_v<std::invoke_result_t<F &>>);

	// void：return whether to execute
	template <class F>
	bool call_algo(F &&f)
		requires(std::is_void_v<std::invoke_result_t<F &>>);
	// Complete get_param -> modify -> set_param within a controlled critical section.
	template <class F>
	bool update_params(F &&mutator) {
		auto param = this->get_param();
		std::invoke(std::forward<F>(mutator), param);
		this->set_param(&param);
		return true;
	}

	explicit SafeAlgorithm(const Options &opt);

	SafeAlgorithm(const SafeAlgorithm &)			= delete;
	SafeAlgorithm &operator=(const SafeAlgorithm &) = delete;
	SafeAlgorithm(SafeAlgorithm &&other) noexcept {
		*this = std::move(other);
	}
	SafeAlgorithm &operator=(SafeAlgorithm &&other) noexcept;

	~SafeAlgorithm();

	bool ok() const noexcept;
	void close();
	// thread safety for the same instance of detect/track can be serialized multiple instances can be parallelized
	// opt
	ax_algorithm_handle_t getHandle() const noexcept;
	int					  set_affinity(bool random = true);
	int					  set_affinity(ax_npu_affinity_e affinity);
	ax_algorithm_param_t  get_param();
	void				  set_param(ax_algorithm_param_t *param);
	//
	// int detect(std::shared_ptr<AXVideoFrame> frame, ax_result_t &result, ax_color_space_e type = ax_color_space_nv12);
	// int track(std::shared_ptr<AXVideoFrame> frame, ax_result_t &result, ax_color_space_e type = ax_color_space_nv12);

	int detect(std::shared_ptr<AXVideoFrame> frame, ax_result_t &result, std::shared_ptr<HwCapture> Capture, ax_color_space_e type = ax_color_space_nv12);
	int track(std::shared_ptr<AXVideoFrame> frame, ax_result_t &result, std::shared_ptr<HwCapture> Capture, ax_color_space_e type = ax_color_space_nv12);
	// face
	int	  get_face_feature(std::shared_ptr<AXVideoFrame> frame, ax_result_t *result, int idx, float feature[AX_ALGORITHM_FACE_FEATURE_LEN], std::shared_ptr<HwCapture> Capture, ax_color_space_e type = ax_color_space_nv12);
	int	  get_face_feature_2(std::shared_ptr<AXVideoFrame> frame, ax_object_t *obj, float feature[AX_ALGORITHM_FACE_FEATURE_LEN], std::shared_ptr<HwCapture> Capture, ax_color_space_e type = ax_color_space_nv12);
	int	  get_face_feature_2(ax_image_t *image, ax_object_t *obj, float feature[AX_ALGORITHM_FACE_FEATURE_LEN]);
	float face_compare(float a[AX_ALGORITHM_FACE_FEATURE_LEN], float b[AX_ALGORITHM_FACE_FEATURE_LEN]);
	// person
	int get_body_attr(std::shared_ptr<AXVideoFrame> frame, ax_bbox_t *bbox, ax_body_attr_t *body_attr, std::shared_ptr<HwCapture> Capture, ax_color_space_e type = ax_color_space_nv12);
	// plate
	std::string get_plate(ax_image_t *image);

private:
	int				   do_init();
	void			   do_deinit();
	static std::mutex &license_mu();
	static std::string license_path_for(int device_id);
	ax_image_t		   frameToImage(std::shared_ptr<AXVideoFrame> frame, std::shared_ptr<AXVideoFrame> &algFrame,
									std::shared_ptr<HwCapture> Capture, ax_color_space_e type = ax_color_space_nv12);

private:
	static constexpr float clamp01(float v) noexcept {
		return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
	}
	ax_algorithm_handle_t handle_{};
	std::atomic<bool>	  alive_{false};
	std::atomic<int>	  inflight_{0};
	std::mutex			  call_mu_;	 // serialization of underlying library calls
	Options				  opt_{};
	// host tools
	std::shared_ptr<HwIvps>	   hostIvps_;
	std::shared_ptr<HwCapture> hostCapture_;
};

inline ax_npu_affinity_e random_npu1_affinity();

std::string frameToBase64(std::shared_ptr<AXVideoFrame> frame /*jpeg*/);

// hsv to bgr
AX_U32 hsv2bgr(float h, float s, float v);
AX_U32 random_color(int id);

int	 rtsp_snap(const std::string &stream_name, const std::string jpg_path = "./snapshot/rtsp_snapshot.jpg");
int	 getStreamInfo(const std::string &stream_name, stream_info &info);
int	 rtsp_check(const std::string &stream_name, stream_info &info, std::string &msg, int max_packets = 40);
bool isRectInAnyRegion(const cv::Rect &rect, const std::vector<std::vector<cv::Point>> &regionList);

// int		   alg_init(ax_algorithm_handle_t &handle, ax_model_type_e model_type, std::string model_path, int device_id);
// int		   alg_deinit(ax_algorithm_handle_t &handle);
ax_image_t frame2image(std::shared_ptr<AXVideoFrame> frame, ax_color_space_e type = ax_color_space_nv12);
// int		   alg_track(std::shared_ptr<AXVideoFrame> frame, ax_algorithm_handle_t &handle, ax_result_t &result, int device_id);
// int		   alg_detect(std::shared_ptr<AXVideoFrame> frame, ax_algorithm_handle_t &handle, ax_result_t &result, float feature[512], int device_id);