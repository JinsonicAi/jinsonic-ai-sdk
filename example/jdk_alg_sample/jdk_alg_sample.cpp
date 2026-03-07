#include <signal.h>
#include <stdio.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "AxVideoFrame.hpp"
#include "HwCapture.hpp"
#include "HwIvps.hpp"
#include "alg_comm.hpp"

using DecodePair = std::pair<cv::Mat, std::shared_ptr<AXVideoFrame>>;

std::vector<unsigned char> read_all(const std::string& path) {
	std::ifstream ifs(path, std::ios::binary);
	return std::vector<unsigned char>(
		(std::istreambuf_iterator<char>(ifs)),
		std::istreambuf_iterator<char>());
}

std::optional<DecodePair> decodeJpegToCvMat(const std::vector<uchar>& jpegdate, std::shared_ptr<HwCapture> Capture_,
											std::shared_ptr<HwIvps> ivps_) {
	auto nv12Frame = Capture_->decodeJpegToNV12(jpegdate.data(), jpegdate.size());
	if (!nv12Frame) {
		std::cerr << "[decodeJpegToCvMat] decodeJpegToNV12 failed" << std::endl;
		return {};
	}
	fmt::print("nv12Frame: {}x{},stride:{}, size: {}\n", nv12Frame->width(), nv12Frame->height(), nv12Frame->stride(), nv12Frame->size());
	nv12Frame->save_data(fmt::format("decodeJpegToCvMat_{}x{}.nv12", nv12Frame->height(), nv12Frame->width()));
	auto rgbFrame = ivps_->HwIvpsCsc(nv12Frame->raw());
	if (!rgbFrame) {
		std::cerr << "[decodeJpegToCvMat] HwIvpsCsc failed" << std::endl;
		return {};
	}
	fmt::print("rgbFrame: {}x{},stride:{}, size: {}\n", rgbFrame->width(), rgbFrame->height(), rgbFrame->stride(), rgbFrame->size());
	rgbFrame->save_data(fmt::format("decodeJpegToCvMat_{}x{}.rgb", rgbFrame->height(), rgbFrame->width()));

	const int	 w			= rgbFrame->width();
	const int	 h			= rgbFrame->height();
	const size_t stride_px	= rgbFrame->stride();
	const size_t step_bytes = stride_px * 3;
	uint8_t*	 ptr		= static_cast<uint8_t*>(rgbFrame->toHost()->getPviraddr());
	cv::Mat		 mat_rgb(h, w, CV_8UC3, ptr, step_bytes);
	// cv::imwrite("decodeJpegToCvMat_out.jpg", mat_rgb);
	return DecodePair{mat_rgb.clone(),	// stride,
					  nullptr};
}

std::vector<float> extFeature(std::optional<DecodePair> inputImage, std::shared_ptr<SafeAlgorithm> regco) {
	if (!inputImage) return {};
	auto& [image, nv12] = *inputImage;
	if (!regco) return {};
	ax_object_t		   obj = {0};
	std::vector<float> feature(512, 0.0f);
	AxImage			   image_rgb(image, -1);

	if (!image_rgb.valid()) {
		fmt::print("ax_create_image failed!!!\n");
		return {};
	}
	if (int ret = regco->get_face_feature_2(image_rgb.get(), &obj, feature.data()); ret != 0) {
		fmt::print("ax_algorithm_get_face_feature failed ret:{}!!!!\n", ret);
		return {};
	}
	// if (int ret = ax_algorithm_get_face_feature_2(handle, image_rgb.get(), &obj, feature.data()); ret != 0) {
	// 	print("ax_algorithm_get_face_feature failed ret:{}!!!!\n", ret);
	// 	return {};
	// }
	return feature;
}

std::string getPlate(std::optional<DecodePair> inputImage, std::shared_ptr<SafeAlgorithm> regco) {
	if (!inputImage) return {};
	auto& [image, nv12] = *inputImage;
	if (!regco) return {};
	// ax_object_t		   obj = {0};
	// std::vector<float> feature(512, 0.0f);
	AxImage image_rgb(image, -1);

	if (!image_rgb.valid()) {
		fmt::print("ax_create_image failed!!!\n");
		return {};
	}
	auto		t0	  = std::chrono::steady_clock::now();
	std::string plate = regco->get_plate(image_rgb.get());
	auto		t1	  = std::chrono::steady_clock::now();
	auto		ms	  = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

	std::cout << "get_plate cost: " << ms << " ms, plate = " << plate << std::endl;
	// if (int ret = regco->get_face_feature_2(image_rgb.get(), &obj, feature.data()); ret != 0) {
	// 	fmt::print("ax_algorithm_get_face_feature failed ret:{}!!!!\n", ret);
	// 	return {};
	// }
	// if (int ret = ax_algorithm_get_face_feature_2(handle, image_rgb.get(), &obj, feature.data()); ret != 0) {
	// 	print("ax_algorithm_get_face_feature failed ret:{}!!!!\n", ret);
	// 	return {};
	// }
	return plate;
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <image.jpg>\n";
		return 1;
	}
	auto tarGetJpeg = read_all(argv[1]);
	auto Capture_	= std::make_shared<HwCapture>(-1);
	auto ivps_		= std::make_shared<HwIvps>(-1, 0, 0);

	std::shared_ptr<SafeAlgorithm> alg{nullptr};
	SafeAlgorithm::Options		   opt{ax_model_type_lpr, "models/lpr_20241129_npu1.model", -1};
	if (alg = std::make_shared<SafeAlgorithm>(opt); !alg || !alg->ok()) {
		fmt::print(fg(fmt::color::red), "❌ recog algo init failed!\n");
		exit(1);
	}
	// alg->set_affinity(true);
	// alg->update_params([&](auto& p) {
	// 	// fmt::print("--->old det_threshold: {}\n", p.det_threshold);
	// 	p.det_threshold				   = 0.0;
	// 	p.face_param.quality_threshold = 0.0;
	// });

	////
	// std::vector<float> faceFeat;
	// if (auto inputImage = decodeJpegToCvMat(tarGetJpeg, Capture_, ivps_); inputImage) {
	// 	auto& [image, nv12] = *inputImage;
	// 	cv::imwrite("decodeJpegToCvMat.jpg", image);
	// 	faceFeat = extFeature(inputImage, alg);
	// }
	// fmt::print("faceFeat size: {}, first 10 elements:\n", faceFeat.size());
	std::string plate;
	if (auto inputImage = decodeJpegToCvMat(tarGetJpeg, Capture_, ivps_); inputImage) {
		auto& [image, nv12] = *inputImage;
		cv::imwrite("decodeJpegToCvMat.jpg", image);
		plate = getPlate(inputImage, alg);
	}
	fmt::print("plate:{}\n", plate);

	return 0;
}