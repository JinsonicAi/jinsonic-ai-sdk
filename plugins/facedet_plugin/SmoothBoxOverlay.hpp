#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>
#include <vector>

struct TBS_Box {
	float	 x{0}, y{0}, w{0}, h{0};
	int		 label{0};
	uint64_t id{0};
	float	 score{1.0f};
};

struct TBS_RenderBox : TBS_Box {
	bool  is_ghost{false};
	float age_ms{0};
};

class TemporalBoxSmoother {
public:
	struct Params {
		int	  linger_ms = 1000;	  // 没新框时最多保留多久（毫秒）
		float ema		= 0.60f;  // 简单 EMA 系数（0..1，越大越跟手）
		int	  max_keep	= 256;	  // 防御：最多保留这么多个框
		float iou_thr	= 0.30f;  // 最近邻匹配阈值：IoU 小于它视为“新框”
	};

	TemporalBoxSmoother() : p_{} {}
	explicit TemporalBoxSmoother(const Params& p) : p_(p) {}

	void set_params(const Params& p) {
		std::lock_guard<std::mutex> lk(mu_);
		p_ = p;
	}

	static float IoU(const TBS_Box& a, const TBS_Box& b) {
		float ax2 = a.x + a.w, ay2 = a.y + a.h;
		float bx2 = b.x + b.w, by2 = b.y + b.h;
		float ix	= std::max(0.f, std::min(ax2, bx2) - std::max(a.x, b.x));
		float iy	= std::max(0.f, std::min(ay2, by2) - std::max(a.y, b.y));
		float inter = ix * iy;
		float uni	= a.w * a.h + b.w * b.h - inter + 1e-6f;
		return inter / uni;
	}

	int find_best_prev(const TBS_Box& nb, const std::vector<bool>& used) const {
		int	  best	   = -1;
		float best_iou = 0.0f;
		for (size_t i = 0; i < prev_output_.size(); ++i) {
			if (used[i]) continue;
			float iou = IoU(nb, prev_output_[i]);
			if (iou > best_iou) {
				best_iou = iou;
				best	 = (int)i;
			}
		}
		if (best >= 0 && IoU(nb, prev_output_[best]) >= p_.iou_thr) return best;
		return -1;
	}

	TBS_Box ema_merge(const TBS_Box& cur, const TBS_Box& prev) const {
		const float e = std::clamp(p_.ema, 0.0f, 1.0f);
		TBS_Box		s = cur;
		s.x			  = e * cur.x + (1.0f - e) * prev.x;
		s.y			  = e * cur.y + (1.0f - e) * prev.y;
		s.w			  = e * cur.w + (1.0f - e) * prev.w;
		s.h			  = e * cur.h + (1.0f - e) * prev.h;
		return s;
	}

	void update(const std::vector<TBS_Box>& new_boxes) {
		using clock = std::chrono::steady_clock;
		auto now	= clock::now();

		std::lock_guard<std::mutex> lk(mu_);

		if (!new_boxes.empty()) {
			std::vector<TBS_Box> smoothed;
			smoothed.reserve(new_boxes.size());
			std::vector<bool> used(prev_output_.size(), false);

			for (const auto& nb : new_boxes) {
				int idx = find_best_prev(nb, used);
				if (idx >= 0) {
					smoothed.push_back(ema_merge(nb, prev_output_[idx]));
					used[idx] = true;
				} else {
					smoothed.push_back(nb);
				}
			}

			if ((int)smoothed.size() > p_.max_keep) {
				smoothed.resize(p_.max_keep);
			}

			prev_output_.swap(smoothed);
			last_valid_output_	= prev_output_;
			last_update_tp_		= now;
			updated_this_frame_ = true;
			has_valid_snapshot_ = true;
		} else {
			updated_this_frame_ = false;
			if (!has_valid_snapshot_) {
				prev_output_.clear();
				return;
			}
			const auto age_ms = ms(now - last_update_tp_);
			if (age_ms > p_.linger_ms) {
				prev_output_.clear();
			} else {
				prev_output_ = last_valid_output_;
			}
		}
	}

	std::vector<TBS_RenderBox> for_render() const {
		using clock						= std::chrono::steady_clock;
		auto						now = clock::now();
		std::lock_guard<std::mutex> lk(mu_);

		std::vector<TBS_RenderBox> out;
		out.reserve(prev_output_.size());

		float age	= 0.0f;
		bool  ghost = false;
		if (has_valid_snapshot_) {
			age	  = ms(now - last_update_tp_);
			ghost = !updated_this_frame_;
		}

		for (const auto& b : prev_output_) {
			TBS_RenderBox r;
			r.x		   = b.x;
			r.y		   = b.y;
			r.w		   = b.w;
			r.h		   = b.h;
			r.label	   = b.label;
			r.id	   = b.id;
			r.score	   = b.score;
			r.is_ghost = ghost;
			r.age_ms   = age;
			out.push_back(r);
		}
		return out;
	}

private:
	static float ms(std::chrono::steady_clock::duration d) {
		return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
	}

	mutable std::mutex mu_;
	Params			   p_{};

	std::vector<TBS_Box>				  prev_output_;
	std::vector<TBS_Box>				  last_valid_output_;
	std::chrono::steady_clock::time_point last_update_tp_{};
	bool								  has_valid_snapshot_{false};
	bool								  updated_this_frame_{false};
};