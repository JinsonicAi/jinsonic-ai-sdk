
#include "yoloFace.hpp"

#include <fstream>
#include <iostream>
#include <string>
#include <typeinfo>

#include "AxVideoFrame.hpp"
#include "HwIvps.hpp"
#include "IEngine.hpp"
#include "IPlugin.hpp"
#include "Tensor.hpp"

using namespace std;
using namespace YOLOFACE;
#define OjbName YOLOFACE
#define OjbInfer IMPLINFER(OjbName, Impl)

class OjbInfer : public InferenceEngine, public Infer, public Params {
public:
	OjbInfer() : InferenceEngine() {}
	virtual ~OjbInfer() {
		std::cout << "Destructor of OjbInfer called" << std::endl;
	}
	virtual bool startup(const std::string &file, int batch_size,
						 const std::string type, int device_id) {
		device_id_ = device_id;
		if (ipvs == nullptr) {
			ipvs = std::make_shared<HwIvps>(device_id_, 0, 0);
		}
		return InferenceEngine::startup(make_tuple(file, type), device_id);
	}
	virtual bool	 pre_process(Job &job, const std::any &input) override;
	virtual std::any post_process(const Job &job) override;

	virtual std::shared_future<std::any> commit(const std::any &image) override {
		return InferenceEngine::commit(image);
	}
	virtual std::vector<std::shared_future<std::any>> commits(const std::vector<std::any> &images) override {
		return InferenceEngine::commits(std::vector<std::any>(images.begin(), images.end()));
	}

	virtual int paramsSet(const Params params) override {
		*(Params *)this = params;
		return 0;
	}
	virtual int paramsGet(Params &params) override {
		params = *this;
		return 0;
	}

private:
	std::shared_ptr<HwIvps> ipvs	   = nullptr;
	TransformMatrices		tm_		   = {0};
	int						device_id_ = -1;
	bool					letterbox_dewarp_failed_{false};

	bool isHost() {
		return device_id_ < 0;
	}
};

/*-----------------------------------------------------------------*/

bool OjbInfer::pre_process(Job &job, const std::any &input) {
	auto tensor = engine->inputs[0];
	int	 width	= tensor->GetWidth();
	int	 height = tensor->GetHeight();
	// std::cout << "Input type: " << input.type().name() << std::endl;
	auto frame = std::any_cast<std::shared_ptr<AXVideoFrame>>(input);

	if (!frame) {
		return false;
	}
	// printf(" device_id:%d,size:%d\r\n", frame->device_id(), frame->size());
	// frame->save_data("pre_process_1280x720_nv12.yuv");

	auto dstDewarpFrame = ipvs->HwIvpsDewarp(frame->raw(), {width, height}, tm_, !letterbox_dewarp_failed_);
	if (!dstDewarpFrame && !letterbox_dewarp_failed_) {
		letterbox_dewarp_failed_ = true;
		std::cout << "[FaceDet] letterbox dewarp failed, retry with stretch resize.\n";
		dstDewarpFrame = ipvs->HwIvpsDewarp(frame->raw(), {width, height}, tm_, false);
	}
	if (!dstDewarpFrame) {
		std::cout << "[FaceDet] invalid dstDewarpFrame\n";
		return false;
	}

	// dstDewarpFrame->save_data("416x416_nv12_Dewarp.yuv");
	// ⚠️ it is strongly recommended to directly implement the code downwards in the model and realize it in the npu!!!!!!!
	//  NV12 TO RGB
	auto RgbFrame = ipvs->HwIvpsCsc(dstDewarpFrame->raw());
	if (!RgbFrame) {
		std::cout << "[FaceDet] invalid RgbFrame\n";
		return false;
	}
	// RgbFrame->save_data("416x416_nv12_Dewarp.rgb");
	// NV12 -> RGB -> Float32
	auto inputFrame = std::make_shared<AXVideoFrame>(width, height, -1 /*device_id_*/, width * height * 3 * 4);	 // decode 256
	// printf("inputFrame ------>size:%d\r\n", inputFrame->size());
	cv::Mat Fp32Image;	//(height, width, CV_32FC3, inputFrame->getPviraddr());
	cv::Mat(height, width, CV_8UC3, RgbFrame->toHost()->getPviraddr()).convertTo(Fp32Image, CV_32FC3, 1.0 / 255.0);

	std::vector<cv::Mat> channels(Fp32Image.channels());
	for (int i = 0; i < Fp32Image.channels(); ++i) {
		auto index	= true ? (2 - i) : i;
		channels[i] = cv::Mat(height, width, CV_32F,
							  (float *)inputFrame->getPviraddr() + index * Fp32Image.total());
	}
	cv::split(Fp32Image, channels);
	if (isHost()) {
		job.input = inputFrame;
		// cv::Mat merged;
		// cv::vconcat(channels, merged);
		// job.input = merged;
		// printf("job.input ------>size:%d\r\n", inputFrame->size());
	} else {
		cv::Mat merged;
		cv::vconcat(channels, merged);
		job.input = merged;
	}

	// }
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
void calculate_box(float **ptrs, int index, int stride, int j, int i, float anchor_w, float anchor_h, cv::Rect_<float> &rect) {
	float pb_cx = (sigmoid(ptrs[0][index]) * 2.f - 0.5f + j) * stride;
	float pb_cy = (sigmoid(ptrs[1][index]) * 2.f - 0.5f + i) * stride;
	float pb_w	= pow(sigmoid(ptrs[2][index]) * 2.f, 2) * anchor_w;
	float pb_h	= pow(sigmoid(ptrs[3][index]) * 2.f, 2) * anchor_h;
	rect		= cv::Rect_<float>(pb_cx - pb_w * 0.5f, pb_cy - pb_h * 0.5f, pb_w, pb_h);
}

void calculate_landmarks(float **ptrs, int index, int stride, int j, int i, float anchor_w, float anchor_h, YPoint *landmarks) {
	for (int k = 0; k < 5; k++) {
		landmarks[k].x = ptrs[5 + 2 * k][index] * anchor_w + j * stride;
		landmarks[k].y = ptrs[6 + 2 * k][index] * anchor_h + i * stride;
	}
}

void decode(float *ptr, int fea_w, int fea_h, int stride, std::vector<int> anchor, Objects &prebox, float threshold) {
	float dthreshold   = desigmoid(threshold);
	int	  spacial_size = fea_w * fea_h;
	for (int c = 0; c < anchor.size() / 2; c++) {
		float  anchor_w = float(anchor[c * 2 + 0]);
		float  anchor_h = float(anchor[c * 2 + 1]);
		float *ptrs[16];
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

std::any OjbInfer::post_process(const Job &job) {
	// without grid
	std::vector<int> anchor0 = {4, 5, 8, 10, 13, 16};
	std::vector<int> anchor1 = {23, 29, 43, 55, 73, 105};
	std::vector<int> anchor2 = {146, 217, 231, 300, 335, 433};

	std::vector<std::vector<int>> anchor;
	anchor.push_back(anchor0);
	anchor.push_back(anchor1);
	anchor.push_back(anchor2);

	Objects bboxes;
	for (auto &tensor : engine->outputs) {
		int input_height = engine->inputs[0]->GetWidth();
		int batch		 = tensor->GetBatch();
		int channel		 = tensor->GetChannel();
		int height		 = tensor->GetHeight();
		int width		 = tensor->GetWidth();
		int stride		 = input_height / height;
		// printf("[%d,%d,%d,%d],stride:%d\r\n", batch, channel, height, width, stride);
		decode(tensor->host<float>(), width, height, stride, anchor[&tensor - &engine->outputs[0]], bboxes, confThreshold_);
	}

	// printf("bboxes size: %d\n", bboxes.size());
	auto fast_mns = [&](Objects &src_box, Objects &box_result, float threshold) {
		std::sort(src_box.begin(), src_box.end(),
				  [](FaceBox &a, FaceBox &b) { return a.prob > b.prob; });
		std::vector<bool> remove_flags(src_box.size());

		// BoxArray box_result;
		if (0 < src_box.size()) {
			box_result.reserve(src_box.size());

			auto iou = [](const FaceBox &a, const FaceBox &b) {
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
				auto &ibox = src_box[i];
				// box_result.emplace_back(affine.inv(ibox));
				box_result.emplace_back(tm_.inv(ibox));
				for (int j = i + 1; j < src_box.size(); ++j) {
					if (remove_flags[j])
						continue;

					auto &jbox = src_box[j];
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

	// for (auto &box : box_result) {
	// 	for (auto &landmark : box.landmarks) {
	// 		printf("==>x,y [%d,%d]\r\n", int(landmark.x), int(landmark.y));
	// 	}
	// }
	// printf("fast_mns box_result size: %d\n", box_result.size());
	return box_result;
}

void OjbName::Params::from_json(const json &j) {
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

void OjbName::Params::to_json(json &j) const {
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

void OjbName::JsonCfg::load_from_json(const std::string &json_file) {
	std::ifstream i(json_file);
	if (!i.is_open()) {
		throw std::runtime_error("Unable to open file: " + json_file);
	}
	json j;
	i >> j;
	from_json(j);
}

void OjbName::JsonCfg::save_to_json(const std::string &json_file) {
	std::ofstream o(json_file);
	if (!o.is_open()) {
		throw std::runtime_error("Unable to open file: " + json_file);
	}
	json j;
	to_json(j);
	o << std::setw(4) << j << std::endl;
}

EXPORT_VISIBILITY std::shared_ptr<Infer> OjbName::create_infer(
	const std::string &file, const std::string &type, int device_id, int batch_size) {
	printf("create_infer file:%s, type:%s, device_id:%d, batch_size:%d\r\n", file.data(), type.data(), device_id, batch_size);
	shared_ptr<OjbInfer> instance(new OjbInfer());
	if (!instance->startup(file, batch_size, type, device_id)) {
		instance.reset();
	}
	return instance;
}
