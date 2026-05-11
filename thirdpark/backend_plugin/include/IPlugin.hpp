#pragma once
// rmwei create.
#ifndef __IINFERENCE_PLUGIN__
#define __IINFERENCEP_LUGIN__

/* for general */
#include <stdlib.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

// #include "Tensor.hpp"
class Tensor;
#define INFER_(x, y) x##y
#define IMPLINFER(x, y) INFER_(x, y)

#define TO_STRING(x) #x
#define STRINGIFY(x) TO_STRING(x)

// Helper struct to detect if a member variable exists
template <typename T, typename, typename = void>
struct has_member : std::false_type {};

template <typename T, typename V>
struct has_member<T, V, std::void_t<decltype(&T::template get<V>)>> : std::true_type {};

// Alias template for the existence check
template <typename T, typename V>
inline constexpr bool has_member_v = has_member<T, V>::value;

// Example usage of the helper macro to check for 'mode_' and 'fixed_' members
typedef struct {
	float	scale;
	int32_t zero_point;
} quant_t;

class TensorParam {
public:
	TensorParam() = default;
	template <typename DType>
	TensorParam(const DType& p)
		: norm(p.norm_),
		  mean(p.mean_),
		  std(p.std_),
		  image_size(p.image_size_),
		  is_nchw(p.isNchw_),
		  swapRGB(p.swapRGB_) {
		// if constexpr (has_member_fixed<DType>::value) fixed = p.fixed_;
		// if constexpr (has_member_mode<DType>::value) mode = p.mode_;
		if constexpr (has_member_v<DType, decltype(&DType::fixed_)>)
			fixed = p.fixed_;
		if constexpr (has_member_v<DType, decltype(&DType::mode_)>)
			mode = p.mode_;
	}

public:
	int				   mode{0};			// engin mode //rgb ro yuv
	bool			   fixed{false};	// fixed output ?
	bool			   swapRGB{false};	// used when channel == 3 (true: BGR, false: RGB)
	bool			   is_nchw{true};	// [IN] NCHW or NHWC
	float			   norm{1.0f};
	quant_t			   quant{1.0f, 0};
	std::vector<float> mean;
	std::vector<float> std;
	std::vector<int>   image_size;
};

// class EXPORT_VISIBILITY IInferencePlugin : public TensorParam {
class EXPORT_VISIBILITY IInferencePlugin {
public:
	IInferencePlugin(){};
	virtual ~IInferencePlugin() {}

public:
	virtual int32_t Initialize(const std::string& model_filename, int device_id = -1, const std::string& model_name = "") = 0;
	virtual int32_t SetPriority(const int32_t priority)								  = 0;
	virtual int32_t SetNumThreads(const int32_t num_threads)						  = 0;
	virtual int32_t SetCustomOps(const TensorParam& custom_ops)						  = 0;
	virtual int32_t SetExmem(bool exmem)											  = 0;
	virtual void	run()															  = 0;
	virtual void	ShapeInfo();
	virtual void	FrameRateInfo(std::string name);

	// virtual std::vector<std::shared_ptr<Tensor>> TensorList(std::vector<std::string> name_list);

public:
	std::vector<std::shared_ptr<Tensor>> inputs;
	std::vector<std::shared_ptr<Tensor>> outputs;
};
using InferencePluginPtr = std::shared_ptr<IInferencePlugin>;
#endif
