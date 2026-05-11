#ifndef _IVPS_COVER_HPP_
#define _IVPS_COVER_HPP_
// rmwei create .
#include <iostream>
#include <opencv2/opencv.hpp>
#include <type_traits>
#include <vector>

// // key points
// struct YPoint {
//     float x;
//     float y;
// };

// // face detection frame
// struct FaceBox {
//     cv::Rect_<float> rect;
//     int label;
//     float prob;
//     YPoint landmarks[5];
// };

// // universal target detection frame
// struct ObjectBox {
//     cv::Rect_<float> rect;
//     int class_id;
//     float confidence;
//     std::vector<YPoint> keypoints;
// };
#ifndef CV_MEMBER_TRAITS_DEFINED
#define CV_MEMBER_TRAITS_DEFINED
template <typename, typename = std::void_t<>>
struct has_rect : std::false_type {};

template <typename T>
struct has_rect<T, std::void_t<decltype(std::declval<T>().rect)>> : std::true_type {};

// ----

template <typename, typename = std::void_t<>>
struct has_landmarks : std::false_type {};

template <typename T>
struct has_landmarks<T, std::void_t<decltype(std::declval<T>().landmarks)>> : std::true_type {};

// ----

template <typename, typename = std::void_t<>>
struct has_keypoints : std::false_type {};

template <typename T>
struct has_keypoints<T, std::void_t<decltype(std::declval<T>().keypoints)>> : std::true_type {};
#endif
class FillRect {
public:
	AX_U32 x;
	AX_U32 y;
	AX_U32 w;
	AX_U32 h;
};

class EXPORT_VISIBILITY TransformMatrices {
public:
	// forward matrix: maps the source image to the target canvas
	double forward[9];
	// inverse matrix: map the target coordinates back to the source image(the inverse matrix of the forward matrix)
	double inverse[9];
	// fixed point version(inverse matrix),1.0 corresponding FIXED_SCALE
	AX_S64 inverse_fixed[9];
	// 4) record the filled area information (optional)
	int		 fillCount;
	FillRect fillAreas[2] = {};
	// 5) if needed"normalized to [0,1]"or"divide by the width and height of the original image",you can record the size of the artwork here
	int orgWidth  = 1;
	int orgHeight = 1;

	template <typename T>
	T inv(const T& obj) const {
		// make a copy first and prepare to make changes
		T originalObj = obj;

		// if it has a rect member it is inversely transformed
		if constexpr (has_rect<T>::value) {
			// use the four corners to do a homogeneous transformation, then take the external torque
			cv::Rect_<float> newRect = transformRectInverse(obj.rect);
			// if you want to"normalization"to [0,1] range,or divide orgWidth/Height, it can be handled here
			newRect.x /= float(orgWidth);
			newRect.y /= float(orgHeight);
			newRect.width /= float(orgWidth);
			newRect.height /= float(orgHeight);
			originalObj.rect = newRect;
		}

		// if you have landmarks member (static arrays) then traverses and inversely transforms
		if constexpr (has_landmarks<T>::value) {
			for (auto& lm : originalObj.landmarks) {
				const auto p = transformPointInverse(lm.x, lm.y);
				// in the same way you can also choose whether to divide or not orgWidth/Height
				lm.x = p.first / float(orgWidth);
				lm.y = p.second / float(orgHeight);
			}
		}

		// if you have a keypoints member (std::vector<YPoint>) then traverses and inversely transforms
		if constexpr (has_keypoints<T>::value) {
			for (auto& kp : originalObj.keypoints) {
				auto p = transformPointInverse(kp.x, kp.y);
				kp.x   = p.first / float(orgWidth);
				kp.y   = p.second / float(orgHeight);
			}
		}

		return originalObj;
	}

private:
	std::pair<double, double> transformPointInverse(double x, double y) const;
	cv::Rect_<float>		  transformRectInverse(const cv::Rect_<float>& rc) const;
};

#endif
