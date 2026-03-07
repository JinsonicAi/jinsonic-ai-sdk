#include <algorithm>
#include <opencv2/opencv.hpp>
#include <stdexcept>
#include <type_traits>
#include <vector>

// // define the y point and face box structs
// struct YPoint {
// 	float x;
// 	float y;
// };

// struct FaceBox {
// 	cv::Rect_<float> rect;
// 	int				 label;
// 	float			 prob;
// 	YPoint			 landmarks[5];
// };

// // define the object box struct
// struct ObjectBox {
// 	cv::Rect_<float>	rect;
// 	int					class_id;
// 	float				confidence;
// 	std::vector<YPoint> keypoints;
// };

// a universal template feature detection tool
template <typename, typename = std::void_t<>>
struct has_rect : std::false_type {};

template <typename T>
struct has_rect<T, std::void_t<decltype(std::declval<T>().rect)>> : std::true_type {};

template <typename, typename = std::void_t<>>
struct has_landmarks : std::false_type {};

template <typename T>
struct has_landmarks<T, std::void_t<decltype(std::declval<T>().landmarks)>> : std::true_type {};

template <typename, typename = std::void_t<>>
struct has_keypoints : std::false_type {};

template <typename T>
struct has_keypoints<T, std::void_t<decltype(std::declval<T>().keypoints)>> : std::true_type {};

template <typename T>
class AffineMatrix {
public:
	cv::Mat init(const cv::Mat& image, const cv::Size& size, bool percentage = false) {
		if (percentage) {
			org_width  = image.cols;
			org_height = image.rows;
		}
		if (size == image.size()) {
			warpAffineFlag_ = 0;
			d2i_			= {1.0f, 0.0f, 0.0f,
							   0.0f, 1.0f, 0.0f};
			return image;
		}

		const float scale_x = static_cast<float>(size.width) / image.cols;
		const float scale_y = static_cast<float>(size.height) / image.rows;
		const float scale	= std::min(scale_x, scale_y);

		cv::Mat i2d = (cv::Mat_<float>(2, 3) << scale, 0, 0,
					   0, scale, 0);

		i2d.at<float>(0, 2) = (-scale * image.cols + size.width + scale - 1) * 0.5;
		i2d.at<float>(1, 2) = (-scale * image.rows + size.height + scale - 1) * 0.5;
		cv::Mat d2i;
		cv::invertAffineTransform(i2d, d2i);
		d2i_ = {d2i.at<float>(0, 0), d2i.at<float>(0, 1), d2i.at<float>(0, 2),
				d2i.at<float>(1, 0), d2i.at<float>(1, 1), d2i.at<float>(1, 2)};

		cv::Mat input_image;
		cv::warpAffine(image, input_image, i2d, size,
					   cv::INTER_NEAREST, cv::BORDER_CONSTANT, cv::Scalar(114, 114, 114));

		warpAffineFlag_ = 1;
		return input_image;
	}

	T inv(const T& obj) const {
		T originalObj = obj;
		if (!warpAffineFlag_) {
			////
			if constexpr (has_rect<T>::value) {
				originalObj.rect.x		= obj.rect.x / org_width;
				originalObj.rect.y		= obj.rect.y / org_height;
				originalObj.rect.width	= obj.rect.width / org_width;
				originalObj.rect.height = obj.rect.height / org_height;
			}
			transformKeypoints(originalObj, d2i_);
			return originalObj;
			///
			// return obj;
		}

		// T originalObj = obj;
		// if the struct contains a rect the transformation is made
		if constexpr (has_rect<T>::value) {
			// printf("---------------->dsafdsafadsdf\r\n");
			originalObj.rect.x		= (d2i_[0] * obj.rect.x + d2i_[2]) / org_width;
			originalObj.rect.y		= (d2i_[0] * obj.rect.y + d2i_[5]) / org_height;
			originalObj.rect.width	= d2i_[0] * obj.rect.width / org_width;
			originalObj.rect.height = d2i_[0] * obj.rect.height / org_height;
		}

		transformKeypoints(originalObj, d2i_);

		return originalObj;
	}

private:
	std::vector<float> d2i_;
	int				   warpAffineFlag_{0};
	int				   org_width{1};
	int				   org_height{1};
	/**
	 * reverses the affine transformation of keys。
	 *
	 * @param obj objects that need to be reversed at the key
	 * @param d2i_ inverse affine matrix
	 */
	void
	transformKeypoints(T& obj, const std::vector<float>& d2i_) const {
		if constexpr (has_landmarks<T>::value) {
			for (auto& point : obj.landmarks) {
				float original_x = d2i_[0] * point.x + d2i_[2];
				float original_y = d2i_[0] * point.y + d2i_[5];
				point.x			 = original_x / org_width;
				point.y			 = original_y / org_height;
			}
		} else if constexpr (has_keypoints<T>::value) {
			for (auto& point : obj.keypoints) {
				float original_x = (d2i_[0] * point.x + d2i_[2]);
				float original_y = (d2i_[0] * point.y + d2i_[5]);
				point.x			 = original_x / org_width;
				point.y			 = original_y / org_height;
			}
		}
		// else if constexpr (std::is_same_v<T, Keypoints>) {
		// 	for (auto& point : obj.points) {
		// 		float original_x = d2i_[0] * point.x + d2i_[2];
		// 		float original_y = d2i_[0] * point.y + d2i_[5];
		// 		point.x			 = original_x;
		// 		point.y			 = original_y;
		// 	}
		// }
	}
};