#ifndef __YOLOFACE_HPP__
#define __YOLOFACE_HPP__
#include <any>
#include <condition_variable>
#include <functional>
#include <future>
#include <iomanip>
#include <json.hpp>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
// #include <inferMat.hpp>

// using namespace inferMat;
#include <opencv2/opencv.hpp>

using namespace cv;
namespace YOLOFACE {
using json = nlohmann::ordered_json;
// cfg
class JsonCfg {	 // json cfg api
public:
	virtual ~JsonCfg() {}

	virtual void from_json(const json &j) = 0;
	virtual void to_json(json &j) const	  = 0;

	virtual void load_from_json(const std::string &json_file);
	virtual void save_to_json(const std::string &json_file);
};

class EXPORT_VISIBILITY Params : public JsonCfg {
public:
	std::vector<int>   image_size_ = {1, 3, 416, 416};	// nchw
	std::vector<float> mean_	   = {0, 0, 0};
	std::vector<float> std_		   = {1, 1, 1};
	float			   norm_	   = 1.0f / 255.0f;
	bool			   swapRGB_	   = true;
	int				   mode_	   = 0;
	bool			   fixed_	   = false;

	float confThreshold_ = 0.45;
	float nmsThreshold_	 = 0.45;

private:
	// json cfg api
	void from_json(const json &j) override;
	void to_json(json &j) const override;
};

struct YPoint {
	float x;
	float y;
};

typedef struct {
	cv::Rect_<float> rect;
	int				 label{0};
	float			 prob;
	// YPoint			 landmarks[5];
	std::array<YPoint, 5> landmarks;
} FaceBox;

typedef std::vector<FaceBox> Objects;

class Infer {
public:
	virtual std::shared_future<std::any>			  commit(const std::any &image)				   = 0;
	virtual std::vector<std::shared_future<std::any>> commits(const std::vector<std::any> &images) = 0;

	virtual int paramsSet(const Params params) = 0;
	virtual int paramsGet(Params &params)	   = 0;
};

std::shared_ptr<Infer> create_infer(const std::string &file, const std::string &type = "ax", int device_id = -1, int batch_size = 1);
}  // namespace YOLOFACE

typedef YOLOFACE::Objects FaceDetTarget;
#endif