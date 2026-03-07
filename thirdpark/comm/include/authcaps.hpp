#pragma once
// rmwei create .
#include <cstdint>
#include <initializer_list>
#include <string>

namespace authcaps {
// Time tool (steady_clock)
uint64_t diag_now_ms();

//---Status read and write (shared) ---
void	 set_caps(int device_id, uint64_t caps);
uint64_t get_caps(int device_id);

// Initialization: load caps + initialize gate + clear cache
uint64_t load_caps(int				  device_id,
				   const std::string& verify_path,
				   const std::string& server_ip = "");

//---Inspection API called by the business side (with cache) ---
bool gate_ok(int device_id, uint64_t tag, uint64_t salt = 0);
bool cap_has(int device_id, uint64_t bit, uint64_t tag);

bool require_any(int							 device_id,
				 std::initializer_list<uint64_t> bits,
				 uint64_t						 tag_base);
bool require_all(int							 device_id,
				 std::initializer_list<uint64_t> bits,
				 uint64_t						 tag_base);
void print_caps(uint64_t				   caps,
				std::initializer_list<int> bits_to_print = {8, 9});

}  // namespace authcaps
