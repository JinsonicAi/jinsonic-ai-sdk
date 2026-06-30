
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <opencv2/opencv.hpp>
#include <vector>

#include "AxVideoFrame.hpp"
#include "HwCover.hpp"
#include "HwIvps.hpp"
#include "YOLOV5FACE.hpp"
#include "../sample_runtime.hpp"

using namespace std;
using namespace std::filesystem;

static std::tuple<uint8_t, uint8_t, uint8_t> hsv2bgr(float h, float s,
													 float v) {
	const int	h_i = static_cast<int>(h * 6);
	const float f	= h * 6 - h_i;
	const float p	= v * (1 - s);
	const float q	= v * (1 - f * s);
	const float t	= v * (1 - (1 - f) * s);
	float		r, g, b;
	switch (h_i) {
	case 0:
		r = v;
		g = t;
		b = p;
		break;
	case 1:
		r = q;
		g = v;
		b = p;
		break;
	case 2:
		r = p;
		g = v;
		b = t;
		break;
	case 3:
		r = p;
		g = q;
		b = v;
		break;
	case 4:
		r = t;
		g = p;
		b = v;
		break;
	case 5:
		r = v;
		g = p;
		b = q;
		break;
	default:
		r = 1;
		g = 1;
		b = 1;
		break;
	}
	return make_tuple(static_cast<uint8_t>(b * 255),
					  static_cast<uint8_t>(g * 255),
					  static_cast<uint8_t>(r * 255));
}

static std::tuple<uint8_t, uint8_t, uint8_t> random_color(int id) {
	float h_plane = ((((unsigned int)id << 2) ^ 0x937151) % 100) / 100.0f;
	;
	float s_plane = ((((unsigned int)id << 3) ^ 0x315793) % 100) / 100.0f;
	return hsv2bgr(h_plane, s_plane, 1);
}
cv::Scalar invertColor(const cv::Scalar& color) {
	return cv::Scalar(255 - color[0], 255 - color[1], 255 - color[2]);
}
void drawTransparentRectangle(cv::Mat& image, const std::string& text,
							  cv::Point topLeft, cv::Point bottomRight,
							  cv::Scalar color, double alpha = 0.5) {
	cv::Rect roi(topLeft, bottomRight);
	roi				= roi & cv::Rect(0, 0, image.cols, image.rows);
	cv::Mat overlay = image(roi).clone();
	cv::rectangle(overlay, cv::Point(0, 0), cv::Point(roi.width, roi.height),
				  color, -1);
	cv::putText(overlay, text, cv::Point(0, bottomRight.y - topLeft.y - 8), 0, 1,
				invertColor(color), 2, 16);
	cv::addWeighted(overlay, alpha, image(roi), 1 - alpha, 0, image(roi));
}

void mkdir(const std::string& path) {
	if (!directory_entry(path).is_directory()) {
		create_directories(path);
	}
}
#if 1
int draw_result(cv::Mat& image, std::vector<YOLOV5FACE::FaceBox> box_result,
				std::string file	 = "image-draw.jpg",
				std::string save_dir = "./") {
	printf(" box_result.size:%d\r\n", box_result.size());
	for (int i = 0; i < box_result.size(); ++i) {
		auto&	   ibox		   = box_result[i];
		float	   left		   = ibox.rect.tl().x;
		float	   top		   = ibox.rect.tl().y;
		float	   right	   = ibox.rect.br().x;
		float	   bottom	   = ibox.rect.br().y;
		int		   class_label = ibox.label;
		float	   confidence  = ibox.prob;
		cv::Scalar color;
		tie(color[0], color[1], color[2]) = random_color(class_label);

		cv::rectangle(image, cv::Point(left, top), cv::Point(right, bottom), color,
					  3);
		circle(image, cv::Point(ibox.landmarks[0].x, ibox.landmarks[0].y), 1, cv::Scalar(0, 0, 255), 4);
		circle(image, cv::Point(ibox.landmarks[1].x, ibox.landmarks[1].y), 1, cv::Scalar(0, 255, 255), 4);
		circle(image, cv::Point(ibox.landmarks[2].x, ibox.landmarks[2].y), 1, cv::Scalar(255, 0, 255), 4);
		circle(image, cv::Point(ibox.landmarks[3].x, ibox.landmarks[3].y), 1, cv::Scalar(0, 255, 0), 4);
		circle(image, cv::Point(ibox.landmarks[4].x, ibox.landmarks[4].y), 1, cv::Scalar(255, 0, 0), 4);
	}
	mkdir(save_dir);
	auto save_file = save_dir + "/" + file;
	cv::imwrite(save_file.data(), image);
	return 0;
}
#endif
vector<string> file_list(const string dir) {
	vector<string> files;
	if (!directory_entry(dir).is_directory())
		return files;
	directory_iterator iters(dir);
	for (auto& iter : iters) {
		string file_path(dir);
		file_path += "/";
		file_path += iter.path().filename();
		if (directory_entry(file_path).is_directory()) {
			files = file_list(file_path);
		} else {  // file
			string extension = iter.path().extension();
			if (extension == ".jpg" || extension == ".png" || extension == ".jpeg" ||
				extension == ".bmp") {
				files.push_back(file_path);
			}
		}
	}
	return files;
}

int draw_nv12(std::shared_ptr<AXVideoFrame> frame, std::vector<YOLOV5FACE::FaceBox> box_result, const SampleRuntime& runtime) {
	printf(" box_result.size:%d\r\n", box_result.size());
	auto ipvs = std::make_shared<HwIvps>(runtime.runtime_device_id, 0, 0, runtime.location);
	for (int i = 0; i < box_result.size(); ++i) {
		auto& ibox		  = box_result[i];
		float left		  = ibox.rect.tl().x;
		float top		  = ibox.rect.tl().y;
		float right		  = ibox.rect.br().x;
		float bottom	  = ibox.rect.br().y;
		int	  class_label = ibox.label;
		float confidence  = ibox.prob;
		printf("----->box[%d]:left:%f,top:%f,right:%f,bottom:%f,label:%d,prob:%f\r\n", i, left, top, right, bottom, class_label, confidence);
		ipvs->HwDrawRect(frame->raw(), {ibox.rect.x, ibox.rect.y, ibox.rect.width, ibox.rect.height}, 0xff0000);
	}
	printf("----->w:%d, stride:%d\r\n", frame->raw()->u32Width, frame->raw()->u32PicStride[0]);
	// mkdir(save_dir);
	// auto save_file = save_dir + "/" + file;
	// cv::imwrite(save_file.data(), image);
	frame->save_data("frame_1280x886_OSD_result.nv12");
	return 0;
}

int main(int argc, char* argv[]) {
	print_sample_runtime_usage(argv[0]);
	const auto runtime = parse_sample_runtime(argc, argv);
	std::cout << "runtime_location=" << runtime.location
			  << ", runtime_device_id=" << runtime.runtime_device_id
			  << ", infer_type=" << runtime.infer_type() << std::endl;
	#if 0
	auto DewarpFrame = std::make_shared<AXVideoFrame>(1280, 886, device_id, AX_FORMAT_YUV420_SEMIPLANAR, 16);  // decode 256

	DewarpFrame->load_data("1280x886_nv12.yuv");
	printf(" info:%s,%d\r\n", __FUNCTION__, __LINE__);

	auto Engin = YOLOV5FACE::create_infer("models/yolov5n-face.axmodel", "ax", device_id);
	if (!Engin) {
		std::cerr << "create_infer error!!!!" << std::endl;
		return -1;
		// continue;
	}
	auto result = Engin->commit(DewarpFrame).get();
	#endif

	// 根据运行位置创建输入帧：rk.local 使用 RK dma-buf，其它路径使用 AX/AXCL/Host frame。
	auto frame = make_sample_video_frame(runtime, 1280, 886, AX_FORMAT_YUV420_SEMIPLANAR, 16);

	frame->load_data("data/1280x886_nv12.yuv");
	const std::string model_path = runtime.is_rk_local()
		? "models/yolov5n-face_rk3588.rknn"
		: "models/yolov5n-face.axmodel";
	auto Engin = YOLOV5FACE::create_infer(model_path, runtime.infer_type(), runtime.runtime_device_id);
	if (!Engin) {
		std::cerr << "create_infer error!!!!" << std::endl;
		return -1;
		// continue;
	}
	auto result = Engin->commit(frame).get();
	if (!result.has_value() || result.type() != typeid(YOLOV5FACE::Objects)) {
		std::cerr << "inference failed, result type=" << (result.has_value() ? result.type().name() : "empty") << std::endl;
		return -1;
	}
	draw_nv12(frame, std::any_cast<YOLOV5FACE::Objects>(result), runtime);
	std::cout << "create_infer exit ok." << std::endl;
	return 0;
}