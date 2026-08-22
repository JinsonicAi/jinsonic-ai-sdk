// #include <opencv2/opencv.hpp>
#include <array>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <typeinfo>
#include <vector>

// #include "IConver.hpp"
#include "IEngine.hpp"
#include "IPlugin.hpp"
#include "Tensor.hpp"
// #include "onnxruntime_plugin.hpp"
#include "AxVideoFrame.hpp"
#include "HwIvps.hpp"
#include "PluginFrameUtils.hpp"
#include "YOLOV5FACE.hpp"

using namespace std;
using namespace YOLOV5FACE;
#define OjbName YOLOV5FACE
#define OjbInfer IMPLINFER(OjbName, Impl)

class OjbInfer : public InferenceEngine, public Infer, public Params {
public:
	OjbInfer() : InferenceEngine() {}
	virtual ~OjbInfer() {
		std::cout << "Destructor of OjbInfer called" << std::endl;
	}
	virtual bool startup(const std::string& file, int batch_size,
						 const std::string type, const std::string& model_name, int device_id) {
		device_id_ = device_id;
		infer_type_ = type;
		if (!InferenceEngine::startup(make_tuple(file, type), device_id, model_name)) {
			return false;
		}
		// startup() completes before the Infer is published. Constructing HwIvps
		// here avoids racing lazy initialization when several tasks submit to the
		// same Infer concurrently.
		ivps_ = std::make_shared<HwIvps>(
			device_id_, 0, 0, infer_type_ == "rk" ? "rk.local" : "");
		return true;
	}
	virtual bool	 pre_process(Job& job, const std::any& input) override;
	virtual std::any post_process(const Job& job) override;

	virtual std::shared_future<std::any> commit(const std::any& image) override {
		return InferenceEngine::commit(image);
	}
	virtual std::vector<std::shared_future<std::any>> commits(const std::vector<std::any>& images) override {
		return InferenceEngine::commits(std::vector<std::any>(images.begin(), images.end()));
	}

	virtual int paramsSet(const Params params) override {
		*(Params*)this = params;
		return 0;
	}
	virtual int paramsGet(Params& params) override {
		params = *this;
		return 0;
	}

private:
	// AffineMatrix<FaceBox>	affine;
	std::shared_ptr<HwIvps> ivps_   = nullptr;
	int						device_id_ = -1;
	std::string					infer_type_{};

	bool isHost() {
		return device_id_ < 0;
	}
	// auto					ipvs = std::make_shared<HwIvps>(device_id, 0, 0);
};

/*-----------------------------------------------------------------*/

bool OjbInfer::pre_process(Job& job, const std::any& input) {
	auto tensor = engine->inputs[0];
	auto attach_identity_transform = [&job, &tensor]() {
		TransformMatrices transform{};
		const int width = tensor->GetWidth();
		const int height = tensor->GetHeight();
		CalculateTransformMatrices(width, height, width, height, transform,
			ResizeOptions{ResizeMode::Stretch});
		job.output = transform;
	};

	// STG/时序等非图像模型直接提交连续 FP32 特征。input_attr.type 来自
	// .rknn 模型本身且只读；vector<float> 则声明本次 rknn_input.type 为
	// RKNN_TENSOR_FLOAT32，Runtime 负责转换到模型 native input 类型。
	if (input.type() == typeid(TensorInput)) {
		auto tensor_input = std::any_cast<TensorInput>(input);
		if (tensor_input.type != ::Fp32 ||
			tensor_input.bytes != static_cast<size_t>(tensor->GetElementNum()) * sizeof(float)) {
			std::cerr << "[FP32] TensorInput does not match YOLO input shape/type" << std::endl;
			return false;
		}
		job.input = std::move(tensor_input);
		attach_identity_transform();
		return true;
	}
	if (input.type() == typeid(std::vector<float>)) {
		auto values = std::any_cast<std::vector<float>>(input);
		if (values.size() != static_cast<size_t>(tensor->GetElementNum())) {
			std::cerr << "[FP32] input element count does not match model input" << std::endl;
			return false;
		}
		job.input = TensorInput::fromVector(std::move(values), TensorLayout::Model, false);
		attach_identity_transform();
		return true;
	}
	int width = tensor->GetWidth();
	int height = tensor->GetHeight();
	auto frame = std::any_cast<std::shared_ptr<AXVideoFrame>>(input);
	if (!frame) {
		std::cerr << "[FaceDet] input is not an AXVideoFrame\n";
		return false;
	}

	const auto prepared = ivps_->PreprocessForInference(frame, {
		{width, height},
		AX_FORMAT_RGB888,
		ResizeOptions{},
		IVPS_ENGINE_ID_AUTO,
		IVPS_ENGINE_ID_AUTO,
	});
	if (!prepared) {
		std::cerr << "[FaceDet] common IVPS preprocessing failed\n";
		return false;
	}
	const auto& RgbFrame = prepared.frame;
	const auto& transform = prepared.transform;
	if (std::getenv("AIBOX_DUMP_PREPROCESS")) {
		static std::atomic_flag dumped = ATOMIC_FLAG_INIT;
		if (!dumped.test_and_set(std::memory_order_relaxed)) {
			RgbFrame->save_data("frame_416x416_rgb.rgb");
			if (infer_type_ == "rk") {
				auto host_rgb = RgbFrame->toHost();
				if (jdk_plugin::frame_has_host_memory(host_rgb, static_cast<size_t>(width) * height * 3)) {
					cv::Mat rgb(height, width, CV_8UC3, host_rgb->getPviraddr());
					cv::Mat bgr;
					cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
					cv::imwrite("frame_416x416_rgb.jpg", bgr);
				}
			}
		}
	}
	if (infer_type_ == "rk" && std::getenv("AIBOX_YOLO_FP32_INPUT")) {
		// Direct-staging example: no job.input is needed. prepareInput records
		// FLOAT32/NHWC for RkPlugin and allocates enough host staging memory.
		if (!tensor->prepareInput({::Fp32, TensorLayout::NHWC, false})) {
			std::cerr << "[FP32] unable to prepare RKNN FP32/NHWC input staging" << std::endl;
			return false;
		}
		auto* input_data = tensor->host<float>();
		if (!input_data) return false;
		auto host_rgb = RgbFrame->toHost();
		if (!jdk_plugin::frame_has_host_memory(host_rgb, static_cast<size_t>(width) * height * 3)) {
			std::cerr << "[FP32] unable to map RKNN RGB input to host memory" << std::endl;
			return false;
		}
		cv::Mat rgb(height, width, CV_8UC3, host_rgb->getPviraddr());
		cv::Mat fp32(height, width, CV_32FC3, input_data);
		rgb.convertTo(fp32, CV_32FC3);  // values remain 0..255
		job.input.reset();
		job.output = transform;
		return true;
	}
	if (infer_type_ == "rk") {
		if (!jdk_plugin::frame_has_device_memory(RgbFrame) || RgbFrame->size() < tensor->bytes()) {
			std::cerr << "[FaceDet] invalid RKNN RGB dma-buf, fd=" << RgbFrame->dmaFd()
					  << ", frame_bytes=" << RgbFrame->size()
					  << ", tensor_bytes=" << tensor->bytes() << std::endl;
			return false;
		}
		job.input = RgbFrame;
		job.output = transform;
		return true;
	}

	// This is a numeric CPU staging tensor, not a video frame. Keep it in
	// job-owned host memory so its lifetime is independent of AX media pools
	// across task stop/start. TensorInput remains valid until CopyToDevice has
	// consumed the job and works identically for AX, AXCL and RK backends.
	std::vector<float> input_values(static_cast<size_t>(tensor->GetElementNum()));
	if (input_values.size() * sizeof(float) != static_cast<size_t>(tensor->bytes())) {
		std::cerr << "Invalid input tensor buffer, elements=" << input_values.size()
				  << ", tensor_bytes=" << tensor->bytes() << std::endl;
		return false;
	}
	cv::Mat Fp32Image;
	auto host_rgb = RgbFrame->toHost();
	if (!jdk_plugin::frame_has_host_memory(host_rgb, static_cast<size_t>(width) * height * 3)) {
		std::cerr << "[FaceDet] unable to map RGB input to host memory" << std::endl;
		return false;
	}
	cv::Mat(height, width, CV_8UC3, host_rgb->getPviraddr()).convertTo(Fp32Image, CV_32FC3, 1.0 / 255.0);

	std::vector<cv::Mat> channels(Fp32Image.channels());
	for (int i = 0; i < Fp32Image.channels(); ++i) {
		auto index = 2 - i;
		channels[i] = cv::Mat(height, width, CV_32F,
			input_values.data() + index * Fp32Image.total());
	}
	cv::split(Fp32Image, channels);
	job.input = TensorInput::fromVector(std::move(input_values), TensorLayout::NCHW, false);
	job.output = transform;
	return true;
}

///////////////////////
float desigmoid(float x) {
	return -log(1.0f / x - 1.0f);
}

float sigmoid(float x) {
	return 1.0f / (1.0f + exp(-x));
}
inline float fast_sigmoid(float x) {
	return x / (1.0f + fabsf(x));
}
void calculate_box(float** ptrs, int index, int stride, int j, int i, float anchor_w, float anchor_h, cv::Rect_<float>& rect) {
	float pb_cx = (sigmoid(ptrs[0][index]) * 2.f - 0.5f + j) * stride;
	float pb_cy = (sigmoid(ptrs[1][index]) * 2.f - 0.5f + i) * stride;
	float pb_w	= pow(sigmoid(ptrs[2][index]) * 2.f, 2) * anchor_w;
	float pb_h	= pow(sigmoid(ptrs[3][index]) * 2.f, 2) * anchor_h;
	rect		= cv::Rect_<float>(pb_cx - pb_w * 0.5f, pb_cy - pb_h * 0.5f, pb_w, pb_h);
}

void calculate_landmarks(float** ptrs, int index, int stride, int j, int i, float anchor_w, float anchor_h, YPoint* landmarks) {
	for (int k = 0; k < 5; k++) {
		landmarks[k].x = ptrs[5 + 2 * k][index] * anchor_w + j * stride;
		landmarks[k].y = ptrs[6 + 2 * k][index] * anchor_h + i * stride;
	}
}

void decode(float* ptr, int fea_w, int fea_h, int stride,
			const std::array<int, 6>& anchor, Objects& prebox, float threshold) {
	float dthreshold   = desigmoid(threshold);
	int	  spacial_size = fea_w * fea_h;
	for (int c = 0; c < anchor.size() / 2; c++) {
		float  anchor_w = float(anchor[c * 2 + 0]);
		float  anchor_h = float(anchor[c * 2 + 1]);
		float* ptrs[16];
		for (int i = 0; i < 16; i++) {
			ptrs[i] = ptr + spacial_size * (c * 16 + i);
		}

		for (int i = 0; i < fea_h; i++) {
			for (int j = 0; j < fea_w; j++) {
				int	  index = i * fea_w + j;
				float score = ptrs[4][index];
				if (score > dthreshold) {
					FaceBox obj;
					obj.prob = fast_sigmoid(score);
					calculate_box(ptrs, index, stride, j, i, anchor_w, anchor_h, obj.rect);

					calculate_landmarks(ptrs, index, stride, j, i, anchor_w, anchor_h, &obj.landmarks[0]);
					prebox.push_back(obj);
				}
			}
		}
	}
}

std::any OjbInfer::post_process(const Job& job) {
	static constexpr std::array<std::array<int, 6>, 3> anchors{{
		{{4, 5, 8, 10, 13, 16}},
		{{23, 29, 43, 55, 73, 105}},
		{{146, 217, 231, 300, 335, 433}},
	}};
	const auto* transform = std::any_cast<TransformMatrices>(&job.output);
	if (!transform) {
		throw std::runtime_error("[FaceDet] missing per-job transform metadata");
	}

	Objects bboxes;
	for (auto& tensor : engine->outputs) {
		int input_height = engine->inputs[0]->GetWidth();
		int batch		 = tensor->GetBatch();
		int channel		 = tensor->GetChannel();
		int height		 = tensor->GetHeight();
		int width		 = tensor->GetWidth();
		int stride		 = input_height / height;
		// printf("[%d,%d,%d,%d],stride:%d\r\n", batch, channel, height, width, stride);
		const size_t output_index = static_cast<size_t>(&tensor - &engine->outputs[0]);
		if (output_index >= anchors.size()) {
			throw std::runtime_error("[FaceDet] unexpected number of output heads");
		}
		decode(tensor->host<float>(), width, height, stride,
			anchors[output_index], bboxes, confThreshold_);
	}
	const bool dump_boxes = std::getenv("AIBOX_FACEDET_DUMP_BOXES") != nullptr;
	if (dump_boxes) {
		std::printf("[FaceDet][raw] count=%zu\n", bboxes.size());
	}
	auto fast_mns = [&](Objects& src_box, Objects& box_result, float threshold) {
		std::sort(src_box.begin(), src_box.end(),
				  [](FaceBox& a, FaceBox& b) { return a.prob > b.prob; });
		std::vector<bool> remove_flags(src_box.size());

		// BoxArray box_result;
		if (0 < src_box.size()) {
			box_result.reserve(src_box.size());

			auto iou = [](const FaceBox& a, const FaceBox& b) {
				float cross_left   = std::max(a.rect.tl().x, b.rect.tl().x);
				float cross_top	   = std::max(a.rect.tl().y, b.rect.tl().y);
				float cross_right  = std::min(a.rect.br().x, b.rect.br().x);
				float cross_bottom = std::min(a.rect.br().y, b.rect.br().y);

				float cross_area = std::max(0.0f, cross_right - cross_left) *
								   std::max(0.0f, cross_bottom - cross_top);
				float union_area =
					std::max(0.0f, a.rect.width) * std::max(0.0f, a.rect.height) +
					std::max(0.0f, b.rect.width) * std::max(0.0f, b.rect.height) -
					cross_area;
				if (cross_area == 0 || union_area == 0)
					return 0.0f;
				return cross_area / union_area;
			};

			for (int i = 0; i < src_box.size(); ++i) {
				if (remove_flags[i])
					continue;
				auto& ibox = src_box[i];
				// box_result.emplace_back(affine.inv(ibox));
				box_result.emplace_back(transform->inv(ibox));
				for (int j = i + 1; j < src_box.size(); ++j) {
					if (remove_flags[j])
						continue;

					auto& jbox = src_box[j];
					if (ibox.label == jbox.label) {
						// class matched
						if (iou(ibox, jbox) >= threshold)
							remove_flags[j] = true;
					}
				}
			}
		}
	};
	Objects box_result;
	fast_mns(bboxes, box_result, nmsThreshold_);
	if (dump_boxes) {
		std::printf("[FaceDet][final] count=%zu\n", box_result.size());
		for (size_t index = 0; index < box_result.size(); ++index) {
			const auto& box = box_result[index];
			std::printf("[FaceDet][final] #%zu score=%.6f rect=(%.3f,%.3f,%.3f,%.3f)",
						index, box.prob, box.rect.x, box.rect.y, box.rect.width, box.rect.height);
			for (const auto& point : box.landmarks) {
				std::printf(" kp=(%.3f,%.3f)", point.x, point.y);
			}
			std::printf("\n");
		}
	}
	// for (auto &box : box_result) {
	// 	for (auto &landmark : box.landmarks) {
	// 		printf("==>x,y [%d,%d]\r\n", int(landmark.x), int(landmark.y));
	// 	}
	// }
	if (dump_boxes) {
		std::printf("[FaceDet][nms] count=%zu\n", box_result.size());
	}
	return box_result;
}

void OjbName::Params::from_json(const json& j) {
	j.at("image_size").get_to(this->image_size_);
	j.at("mean").get_to(this->mean_);
	j.at("std").get_to(this->std_);
	j.at("norm").get_to(this->norm_);
	j.at("swapRGB").get_to(this->swapRGB_);
	j.at("mode").get_to(this->mode_);
	j.at("fixed").get_to(this->fixed_);
	j.at("confThreshold").get_to(this->confThreshold_);
	j.at("nmsThreshold").get_to(this->nmsThreshold_);
}

void OjbName::Params::to_json(json& j) const {
	j = json{
		{"image_size", this->image_size_},
		{"mean", this->mean_},
		{"std", this->std_},
		{"norm", this->norm_},
		{"swapRGB", this->swapRGB_},
		{"mode", this->mode_},
		{"fixed", this->fixed_},
		{"confThreshold", this->confThreshold_},
		{"nmsThreshold", this->nmsThreshold_},
	};
}

void OjbName::JsonCfg::load_from_json(const std::string& json_file) {
	std::ifstream i(json_file);
	if (!i.is_open()) {
		throw std::runtime_error("Unable to open file: " + json_file);
	}
	json j;
	i >> j;
	from_json(j);
}

void OjbName::JsonCfg::save_to_json(const std::string& json_file) {
	std::ofstream o(json_file);
	if (!o.is_open()) {
		throw std::runtime_error("Unable to open file: " + json_file);
	}
	json j;
	to_json(j);
	o << std::setw(4) << j << std::endl;
}

EXPORT_VISIBILITY std::shared_ptr<Infer> OjbName::create_infer(
	const std::string& file, const std::string& type, int device_id, const std::string& model_name, int batch_size) {
	printf("create_infer file:%s, type:%s, device_id:%d, model_name:%s, batch_size:%d\r\n", file.data(), type.data(), device_id, model_name.data(), batch_size);
	shared_ptr<OjbInfer> instance(new OjbInfer());
	if (!instance->startup(file, batch_size, type, model_name, device_id)) {
		instance.reset();
	}
	return instance;
}
