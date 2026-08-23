#pragma once

#include <dlfcn.h>
#include <mutex>
#include <stdexcept>
#include <string>

#include "ax_algorithm_sdk.h"

namespace axalgo_dynamic {

inline void* library_handle() {
	static std::once_flag once;
	static void* handle = nullptr;
	std::call_once(once, [] {
		handle = dlopen("libax_algorithm.so", RTLD_NOW | RTLD_LOCAL);
	});
	return handle;
}

template <typename Fn>
inline Fn resolve(const char* name) {
	void* handle = library_handle();
	if (!handle) {
		throw std::runtime_error(std::string("unable to load libax_algorithm.so: ") + dlerror());
	}
	dlerror();
	void* symbol = dlsym(handle, name);
	const char* error = dlerror();
	if (error || !symbol) {
		throw std::runtime_error(std::string("unable to resolve ") + name + ": " + (error ? error : "missing symbol"));
	}
	return reinterpret_cast<Fn>(symbol);
}

#define AX_ALGORITHM_DYNAMIC_FUNCTION(name, signature) \
	inline auto name signature { \
		using Fn = decltype(+[](signature) -> decltype(name signature) { return {}; }); \
		return resolve<Fn>(#name); \
	}

using get_fingerprint_fn = void (*)(ax_algorithm_fingerprint_t*);
using get_default_param_fn = ax_algorithm_param_t (*)();
using init_fn = int (*)(ax_algorithm_init_t*, ax_algorithm_handle_t*);
using deinit_fn = int (*)(ax_algorithm_handle_t);
using detect_fn = int (*)(ax_algorithm_handle_t, ax_image_t*, ax_result_t*);
using track_fn = int (*)(ax_algorithm_handle_t, ax_image_t*, ax_result_t*);
using set_affinity_fn = int (*)(ax_algorithm_handle_t, ax_npu_affinity_e);
using get_param_fn = ax_algorithm_param_t (*)(ax_algorithm_handle_t);
using set_param_fn = int (*)(ax_algorithm_handle_t, ax_algorithm_param_t*);
using get_face_feature_fn = int (*)(ax_algorithm_handle_t, ax_image_t*, ax_result_t*, int, float*);
using get_face_feature2_fn = int (*)(ax_algorithm_handle_t, ax_image_t*, ax_object_t*, float*);
using face_compare_fn = float (*)(float*, float*);
using get_body_attr_fn = int (*)(ax_algorithm_handle_t, ax_image_t*, ax_bbox_t*, ax_body_attr_t*);
using get_car_attr_fn = int (*)(ax_algorithm_handle_t, ax_image_t*, ax_bbox_t*, ax_car_attr_t*);
using get_plate_id_fn = int (*)(ax_algorithm_handle_t, ax_image_t*, int*, int*);
using get_plate_str_fn = int (*)(int*, int, char*);
using create_image_fn = int (*)(int, int, int, ax_color_space_e, ax_image_t*, int);
using release_image_fn = void (*)(ax_image_t*, int);

inline void get_fingerprint(ax_algorithm_fingerprint_t* value) { return resolve<get_fingerprint_fn>("ax_algorithm_get_fingerprint")(value); }
inline ax_algorithm_param_t get_default_param() { return resolve<get_default_param_fn>("ax_algorithm_get_default_param")(); }
inline int init(ax_algorithm_init_t* info, ax_algorithm_handle_t* handle) { return resolve<init_fn>("ax_algorithm_init")(info, handle); }
inline int deinit(ax_algorithm_handle_t handle) { return resolve<deinit_fn>("ax_algorithm_deinit")(handle); }
inline int detect(ax_algorithm_handle_t h, ax_image_t* image, ax_result_t* result) { return resolve<detect_fn>("ax_algorithm_detect")(h, image, result); }
inline int track(ax_algorithm_handle_t h, ax_image_t* image, ax_result_t* result) { return resolve<track_fn>("ax_algorithm_track")(h, image, result); }
inline int set_affinity(ax_algorithm_handle_t h, ax_npu_affinity_e affinity) { return resolve<set_affinity_fn>("ax_algorithm_set_affinity")(h, affinity); }
inline ax_algorithm_param_t get_param(ax_algorithm_handle_t h) { return resolve<get_param_fn>("ax_algorithm_get_param")(h); }
inline int set_param(ax_algorithm_handle_t h, ax_algorithm_param_t* param) { return resolve<set_param_fn>("ax_algorithm_set_param")(h, param); }
inline int get_face_feature(ax_algorithm_handle_t h, ax_image_t* image, ax_result_t* result, int idx, float* feature) { return resolve<get_face_feature_fn>("ax_algorithm_get_face_feature")(h, image, result, idx, feature); }
inline int get_face_feature_2(ax_algorithm_handle_t h, ax_image_t* image, ax_object_t* object, float* feature) { return resolve<get_face_feature2_fn>("ax_algorithm_get_face_feature_2")(h, image, object, feature); }
inline float face_compare(float* first, float* second) { return resolve<face_compare_fn>("ax_algorithm_face_compare")(first, second); }
inline int get_body_attr(ax_algorithm_handle_t h, ax_image_t* image, ax_bbox_t* bbox, ax_body_attr_t* attr) { return resolve<get_body_attr_fn>("ax_algorithm_get_body_attr")(h, image, bbox, attr); }
inline int get_car_attr(ax_algorithm_handle_t h, ax_image_t* image, ax_bbox_t* bbox, ax_car_attr_t* attr) { return resolve<get_car_attr_fn>("ax_algorithm_get_car_attr")(h, image, bbox, attr); }
inline int get_plate_id(ax_algorithm_handle_t h, ax_image_t* image, int* ids, int* length) { return resolve<get_plate_id_fn>("ax_algorithm_get_plate_id")(h, image, ids, length); }
inline int get_plate_str(int* ids, int length, char* output) { return resolve<get_plate_str_fn>("ax_algorithm_get_plate_str")(ids, length, output); }
inline int create_image(int width, int height, int stride, ax_color_space_e color, ax_image_t* image, int device_id) { return resolve<create_image_fn>("ax_create_image")(width, height, stride, color, image, device_id); }
inline void release_image(ax_image_t* image, int device_id) { return resolve<release_image_fn>("ax_release_image")(image, device_id); }

} // namespace axalgo_dynamic
