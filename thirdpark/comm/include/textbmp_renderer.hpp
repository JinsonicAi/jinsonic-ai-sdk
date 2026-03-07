#pragma once
#include <memory>
#include <mutex>
#include <opencv2/core.hpp>
#include <opencv2/freetype.hpp>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef TEXTBMP_API
#if defined(_WIN32) || defined(_WIN64)
#define TEXTBMP_API
#else
#define TEXTBMP_API __attribute__((visibility("default")))
#endif
#endif

namespace textbmp {

struct Options {
	int		   font_height		  = 48;
	cv::Scalar color			  = {255, 255, 255, 255};
	int		   thickness		  = -1;
	int		   line_gap			  = -1;	 // <0 -> auto = font_height/4
	int		   align_w			  = 16;
	int		   align_h			  = 2;
	bool	   with_alpha		  = true;
	cv::Scalar bg_color			  = {0, 0, 0, 0};
	bool	   bottom_left_origin = false;
};

class TEXTBMP_API Renderer final {
public:
	static void		 Init(const std::string& font_file = "/usr/local/aibox/freetype/NotoSansSC-Regular.otf", int face_index = 0);
	static void		 Reset(const std::string& font_file, int face_index = 0);
	static Renderer& Instance();
	cv::Mat			 render(const std::string& utf8_text, const Options& opt = {}) const;

private:
	explicit Renderer(const std::string& font_file, int face_index);
	Renderer(const Renderer&)			 = delete;
	Renderer& operator=(const Renderer&) = delete;
	Renderer(Renderer&&)				 = delete;
	Renderer& operator=(Renderer&&)		 = delete;

	static int						align_up(int v, int a);
	static std::vector<std::string> splitLines(const std::string& s);

private:
	cv::Ptr<cv::freetype::FreeType2> ft2_;
	std::mutex						 ft_mtx_;

	static std::unique_ptr<Renderer> singleton_;
	static std::once_flag			 init_flag_;
	static std::mutex				 reset_mtx_;
};

}  // namespace textbmp
