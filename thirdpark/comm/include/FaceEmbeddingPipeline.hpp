#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>

#include "AxVideoFrame.hpp"
#include "HwIvps.hpp"

namespace face_embedding {

inline constexpr std::size_t kDimensions = 512;
inline constexpr int kAlignedSize = 112;
inline constexpr float kDetectionThreshold = 0.45f;
inline constexpr const char* kPipelineVersion = "arcface112-gdc-v1";

struct FeatureVariant {
	std::string contract_id;
	std::string runtime_family;
	std::array<float, kDimensions> data{};
};

struct FeatureSet {
	std::vector<FeatureVariant> variants;
	std::size_t primary_index{0};

	const FeatureVariant* primary() const noexcept {
		return primary_index < variants.size() ? &variants[primary_index] : nullptr;
	}
};

inline std::string runtime_family(const std::string& runtime_location,
								  const std::string& infer_type) {
	if (runtime_location == "rk.local" || infer_type == "rk") return "rk";
	return "ax";
}

inline std::string model_fingerprint(const std::string& model_path) {
	namespace fs = std::filesystem;
	struct CacheEntry {
		std::uintmax_t size{0};
		fs::file_time_type mtime{};
		std::string digest;
	};
	static std::mutex cache_mutex;
	static std::unordered_map<std::string, CacheEntry> cache;

	std::error_code ec;
	const auto size = fs::file_size(model_path, ec);
	if (ec) return "model-unavailable";
	const auto mtime = fs::last_write_time(model_path, ec);
	if (ec) return "model-unavailable";
	{
		std::lock_guard<std::mutex> lock(cache_mutex);
		const auto it = cache.find(model_path);
		if (it != cache.end() && it->second.size == size && it->second.mtime == mtime)
			return it->second.digest;
	}

	std::ifstream input(model_path, std::ios::binary);
	if (!input) return "model-unavailable";
	std::uint64_t hash = 1469598103934665603ULL;
	std::array<char, 256 * 1024> buffer{};
	while (input) {
		input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		const auto count = input.gcount();
		for (std::streamsize i = 0; i < count; ++i) {
			hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
			hash *= 1099511628211ULL;
		}
	}
	std::ostringstream out;
	out << std::hex << std::setfill('0') << std::setw(16) << hash;
	const std::string digest = out.str();
	{
		std::lock_guard<std::mutex> lock(cache_mutex);
		cache[model_path] = {size, mtime, digest};
	}
	return digest;
}

inline std::string contract_id(const std::string& model_path,
							  const std::string& runtime_location,
							  const std::string& infer_type) {
	return std::string("facerec/") + runtime_family(runtime_location, infer_type) + "/" +
		model_fingerprint(model_path) + "/" + kPipelineVersion;
}

template <typename FaceBox>
std::shared_ptr<AXVideoFrame> align(std::shared_ptr<AXVideoFrame> frame,
									const FaceBox& face,
									const std::shared_ptr<HwIvps>& ivps,
									std::string* error = nullptr) {
	if (error) error->clear();
	if (!frame || !frame->raw() || !ivps || face.landmarks.size() < 5) {
		if (error) *error = "invalid-face-alignment-input";
		return nullptr;
	}

	static const std::array<cv::Point2f, 5> reference = {{
		{38.2946f, 51.6963f},
		{73.5318f, 51.5014f},
		{56.0252f, 71.7366f},
		{41.5493f, 92.3655f},
		{70.7299f, 92.2041f},
	}};
	std::vector<cv::Point2f> source(5);
	for (std::size_t i = 0; i < source.size(); ++i)
		source[i] = {face.landmarks[i].x, face.landmarks[i].y};
	const std::vector<cv::Point2f> destination(reference.begin(), reference.end());

	const cv::Mat transform = cv::estimateAffinePartial2D(source, destination, cv::noArray(), cv::LMEDS);
	if (transform.empty()) {
		if (error) *error = "face-alignment-estimation-failed";
		return nullptr;
	}
	cv::Mat inverse;
	cv::invertAffineTransform(transform, inverse);
	cv::Mat inverse64;
	inverse.convertTo(inverse64, CV_64F);
	const double matrix[9] = {
		inverse64.at<double>(0, 0), inverse64.at<double>(0, 1), inverse64.at<double>(0, 2),
		inverse64.at<double>(1, 0), inverse64.at<double>(1, 1), inverse64.at<double>(1, 2),
		0.0, 0.0, 1.0,
	};
	return ivps->HwIvpsDewarpMatrix(frame->raw(), {kAlignedSize, kAlignedSize}, matrix);
}

}  // namespace face_embedding
