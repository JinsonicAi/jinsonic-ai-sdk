#ifndef __BASE64_HPP__
#define __BASE64_HPP__

#include <openssl/pem.h>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "libbase64.h"

using byte = std::uint8_t;
// rmwei add .
//  ================== Encode ==================
inline std::string base64Encode(const byte* data, size_t len, int flags = 0) {
	if (!data || len == 0) return {};

	const size_t cap = ((len + 2) / 3) * 4;

	std::string out(cap, '\0');
	size_t		actual = cap;

	base64_encode(reinterpret_cast<const char*>(data),
				  len,
				  out.data(),
				  &actual,
				  flags);

	out.resize(actual);
	return out;
}

inline std::string base64Encode(std::string_view bytes, int flags = 0) {
	return base64Encode(reinterpret_cast<const byte*>(bytes.data()), bytes.size(), flags);
}

inline std::string base64Encode(const std::vector<byte>& buf, int flags = 0) {
	return base64Encode(buf.data(), buf.size(), flags);
}

#if __cplusplus >= 202002L
#include <span>
inline std::string base64Encode(std::span<const byte> bytes, int flags = 0) {
	return base64Encode(bytes.data(), bytes.size(), flags);
}
#endif

// ================== Decode ==================
inline std::vector<byte> base64Decode(const char* data, size_t len, int flags = 0) {
	if (!data || len == 0) return {};

	size_t cap = (len / 4) * 3;

	std::vector<byte> out(cap);
	size_t			  actual = cap;

	int rc = base64_decode(data,
						   len,
						   reinterpret_cast<char*>(out.data()),
						   &actual,
						   flags);
	if (rc != 1) {
		throw std::runtime_error("base64_decode failed");
	}

	out.resize(actual);
	return out;
}

inline std::vector<byte> base64Decode(const std::string& input, int flags = 0) {
	return base64Decode(input.data(), input.size(), flags);
}

inline std::vector<byte> base64Decode(std::string_view input, int flags = 0) {
	return base64Decode(input.data(), input.size(), flags);
}

#if 0
std::string base64Encode(const std::vector<uchar>& input, int flags = 0) {
	if (input.empty()) return {};

	// Base64 the maximum length after encoding is the original 4/3,then round up
	size_t encoded_len_estimate = ((input.size() + 2) / 3) * 4;

	std::string output(encoded_len_estimate, '\0');	 // pre allocated space
	size_t		actual_output_len = encoded_len_estimate;

	base64_encode(
		reinterpret_cast<const char*>(input.data()),
		input.size(),
		output.data(),
		&actual_output_len,
		flags);

	output.resize(actual_output_len);
	return output;
}

std::vector<uchar> base64Decode(const std::string& input, int flags = 0) {
	if (input.empty()) return {};

	size_t decoded_len_estimate = (input.size() / 4) * 3;

	std::vector<uchar> output(decoded_len_estimate);
	size_t			   actual_output_len = decoded_len_estimate;

	int rc = base64_decode(
		input.data(),
		input.size(),
		reinterpret_cast<char*>(output.data()),
		&actual_output_len,
		flags);

	if (rc != 1) {
		throw std::runtime_error("base64_decode failed");
	}

	output.resize(actual_output_len);
	return output;
}
#endif
#endif
