#pragma once
#include <any>
#include <cstdlib>
#include <iostream>
#include <json.hpp>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>

// rmwei add .
namespace json_utils {
/*
// bool
bool enabled = jp(j, "/params/enabled", false);     // true
bool missing = jp(j, "/params/missing_flag", true); // key doesn t exist → defaulttrue

// int
int width  = jp(j, "/params/width", 0);             // 1280
int height = jp(j, "/params/height", 0);            // "720" → convert to 720
int bad    = jp(j, "/params/invalid_num", 99);      // "abc" conversion failed → 99

// float / double
float  ratio  = jp(j, "/params/ratio", 0.0f);       // "0.8" → 0.8f
double scale  = jp(j, "/params/scale", 0.0);        // 1.25
double miss_d = jp(j, "/params/missing_double", -1.0); // doesn t exist → -1.0

// string
std::string name   = jp(j, "/params/name", "");     // "camera_01"
std::string no_key = jp(j, "/params/not_here", "default"); // "default"

// container/complex type(the premise is defined from_json)
std::vector<int> vec = jp<std::vector<int>>(j, "/params/some_list", {});

nlohmann::json j = R"({
	"params": {
		"some_list": [1, 2, 3, 4, 5],
		"str_list": ["10", "20", "30"],
		"mixed_list": [1, "2", 3.5, true]
	}
})"_json;

std::vector<int> v1 = jp<std::vector<int>>(j, "/params/some_list", {});
std::vector<int> v2 = jp<std::vector<int>>(j, "/params/str_list", {});
std::vector<int> v3 = jp<std::vector<int>>(j, "/params/mixed_list", {});
*/

template <typename T>
std::optional<T> try_any_cast(const std::any& a, const char* tag = nullptr) {
	try {
		return std::any_cast<T>(a);
	} catch (const std::bad_any_cast& e) {
		if (tag)
			std::cerr << tag << ": " << e.what() << std::endl;
		else
			std::cerr << "any_cast error: " << e.what() << std::endl;
		return std::nullopt;
	}
}

template <typename T>
static bool parse_number(const std::string& s, T& out) {
	std::istringstream iss(s);
	T				   val{};
	iss >> val;
	if (!iss.fail() && iss.eof()) {
		out = val;
		return true;
	}
	return false;
}

template <typename T>
inline T parse_number_or(const std::string& raw, const T& def) {
	T out{};
	return parse_number(raw, out) ? out : def;
}

template <typename T>
inline T clamp_value(const T& value, const T& min_v, const T& max_v) {
	return value < min_v ? min_v : (value > max_v ? max_v : value);
}

template <typename T>
inline T parse_number_clamped_or(const std::string& raw, const T& def, const T& min_v, const T& max_v) {
	return clamp_value(parse_number_or(raw, def), min_v, max_v);
}

inline std::string normalize_path(const char* path) {
	if (path == nullptr || *path == '\0') return "/";
	if (path[0] == '/') return path;  // already have "/"，return as it is
	return "/" + std::string(path);	  // auto complete "/"
}

template <typename T>
T jp(const nlohmann::json& j, const char* path, const T& def = T{}) {
	std::string					 fixed = normalize_path(path);
	nlohmann::json::json_pointer ptr(fixed);

#if NLOHMANN_JSON_VERSION_MAJOR >= 3
	if (!j.contains(ptr)) return def;
#endif

	const nlohmann::json* v = nullptr;
	try {
		v = &j.at(ptr);
	} catch (...) {
		return def;
	}
	if (v->is_null()) return def;

	try {
		if constexpr (std::is_arithmetic_v<T>) {
			if constexpr (std::is_same_v<T, bool>) {
				if (v->is_boolean()) return v->get<bool>();
				if (v->is_number_integer()) return static_cast<bool>(v->get<long long>());
				if (v->is_number()) return static_cast<bool>(v->get<double>());
				if (v->is_string()) {
					const auto& s = v->get_ref<const std::string&>();
					if (s == "true" || s == "1") return true;
					if (s == "false" || s == "0") return false;
					return def;
				}
				return def;
			} else if constexpr (std::is_integral_v<T>) {
				if (v->is_number_integer()) return static_cast<T>(v->get<long long>());
				if (v->is_number()) return static_cast<T>(v->get<double>());
				if (v->is_boolean()) return static_cast<T>(v->get<bool>());
				if (v->is_string()) {
					T out{};
					return parse_number(v->get<std::string>(), out) ? out : def;
				}
				return def;
			} else {  // floating point
				if (v->is_number()) return static_cast<T>(v->get<double>());
				if (v->is_boolean()) return static_cast<T>(v->get<bool>());
				if (v->is_string()) {
					T out{};
					return parse_number(v->get<std::string>(), out) ? out : def;
				}
				return def;
			}
		} else if constexpr (std::is_same_v<T, std::string>) {
			if (v->is_string()) return v->get<std::string>();
			return v->dump();  // non string type → dump become a string
		} else {
			return v->get<T>();	 // other types（container custom structure）
		}
	} catch (...) {
		return def;
	}
}

template <typename T>
T jp_clamped(const nlohmann::json& j, const char* path, const T& def, const T& min_v, const T& max_v) {
	static_assert(std::is_arithmetic<T>::value, "jp_clamped only supports arithmetic types");
	return clamp_value(jp<T>(j, path, def), min_v, max_v);
}

template <typename T>
T env_number_or(const char* key, const T& def) {
	const char* raw = std::getenv(key);
	if (!raw || !raw[0])
		return def;
	return parse_number_or(std::string(raw), def);
}

template <typename T>
T env_number_clamped_or(const char* key, const T& def, const T& min_v, const T& max_v) {
	return clamp_value(env_number_or(key, def), min_v, max_v);
}

// //----overload for nlohmann::json: when you want to MODIFY json ----
// // Semantics: If path does not exist, write def (default {}) and return a reference to j[path].
// inline nlohmann::json& jp(nlohmann::json&		j,
// 						  const char*			path,
// 						  const nlohmann::json& def = nlohmann::json::object()) {
// 	std::string					 fixed = normalize_path(path);
// 	nlohmann::json::json_pointer ptr(fixed);

// #if NLOHMANN_JSON_VERSION_MAJOR >= 3
// 	if (!j.contains(ptr)) {
// 		j[ptr] = def;
// 	}
// #else
// 	// The old version does not contain, use exceptions to judge
// 	try {
// 		(void)j.at(ptr);
// 	} catch (...) {
// 		j[ptr] = def;
// 	}
// #endif
// 	return j[ptr];	// Return a writable reference
// }

// ---- convenient reload:make string literal default value"naturally usable",unified return std::string ----
inline std::string jp(const nlohmann::json& j, const char* path, const char* def) {
	return jp<std::string>(j, path, std::string(def));
}
template <size_t N>
inline std::string jp(const nlohmann::json& j, const char* path, const char (&def)[N]) {
	return jp<std::string>(j, path, std::string(def));
}
inline std::string jp(const nlohmann::json& j, const char* path, std::string_view def) {
	return jp<std::string>(j, path, std::string(def));
}

// abbreviation optional
#define JP(j, path, def) (json_utils::jp((j), (path), (def)))

}  // namespace json_utils
