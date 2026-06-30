#pragma once

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "AxVideoFrame.hpp"

struct SampleRuntime {
    std::string location{"compute_card_1"};
    int runtime_device_id{0};

    bool is_rk_local() const noexcept { return location == "rk.local"; }
    bool is_ax_local() const noexcept { return location == "ax.local"; }
    bool is_compute_card() const noexcept { return location.rfind("compute_card_", 0) == 0; }
    const char* infer_type() const noexcept { return is_rk_local() ? "rk" : "ax"; }
};

inline std::string sample_to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

inline bool sample_contains(const std::string& haystack, const char* needle) {
    return sample_to_lower(haystack).find(needle) != std::string::npos;
}

inline std::string sample_read_file(const char* path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return {};
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

inline std::string sample_default_runtime_location() {
    if (const char* env = std::getenv("AIBOX_SAMPLE_RUNTIME"); env && *env) {
        return env;
    }

    const std::string compatible = sample_read_file("/proc/device-tree/compatible") +
                                   sample_read_file("/sys/firmware/devicetree/base/compatible");
    if (sample_contains(compatible, "rockchip") || sample_contains(compatible, "rk") || sample_contains(compatible, "rv11")) {
        return "rk.local";
    }
    if (sample_contains(compatible, "ax650") || sample_contains(compatible, "ax8850")) {
        return "ax.local";
    }
    return "compute_card_1";
}

inline SampleRuntime parse_sample_runtime(int argc, char** argv, int arg_index = 1) {
    std::string loc = (argc > arg_index && argv[arg_index] && argv[arg_index][0])
        ? argv[arg_index]
        : sample_default_runtime_location();
    loc = sample_to_lower(loc);

    if (loc == "-1" || loc == "local" || loc == "ax" || loc == "ax.local" || loc == "ax_local") {
        return {"ax.local", -1};
    }
    if (loc == "rk" || loc == "rk.local" || loc == "rk_local") {
        return {"rk.local", -1};
    }
    if (loc == "0" || loc == "card1" || loc == "compute_card_1") {
        return {"compute_card_1", 0};
    }
    if (loc == "1" || loc == "card2" || loc == "compute_card_2") {
        return {"compute_card_2", 1};
    }
    if (loc.rfind("compute_card_", 0) == 0) {
        const int card_no = std::max(1, std::atoi(loc.c_str() + std::string("compute_card_").size()));
        return {"compute_card_" + std::to_string(card_no), card_no - 1};
    }

    std::cerr << "Unknown runtime location '" << loc << "', fallback to compute_card_1\n";
    return {"compute_card_1", 0};
}

inline void print_sample_runtime_usage(const char* app) {
    std::cout << "Usage: " << app << " [rk.local|ax.local|compute_card_1|compute_card_2|-1|0|1]\n"
              << "       env AIBOX_SAMPLE_RUNTIME can also set the default runtime location.\n";
}

inline std::shared_ptr<AXVideoFrame> make_sample_video_frame(const SampleRuntime& runtime,
                                                             int width,
                                                             int height,
                                                             AX_IMG_FORMAT_E format,
                                                             AX_U32 align = 16) {
    if (runtime.is_rk_local()) {
        return AXVideoFrame::createRK(width, height, format, align);
    }
    return std::make_shared<AXVideoFrame>(width, height, runtime.runtime_device_id, format, align);
}
