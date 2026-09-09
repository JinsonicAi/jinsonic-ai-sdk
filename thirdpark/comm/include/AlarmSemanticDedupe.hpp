#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <json.hpp>

namespace jdk_alarm_dedupe {

// Output-side safety net for alarms that carry a stable tracking id.
//
// Algorithm state machines still decide when an event begins. This bounded
// guard prevents a PeriodicState regression, or a newly allocated ResultEntry
// on every frame, from delivering one stable track repeatedly. Tracker ids are
// monotonic in the current plugins, so evicting the oldest quarter at capacity
// keeps memory constant without affecting live identities.
class TrackAlarmDeduper {
public:
	struct Claim {
		std::string key{};
		bool accepted{true};

		explicit operator bool() const noexcept { return accepted; }
	};

	explicit TrackAlarmDeduper(std::size_t capacity = 8192)
	    : capacity_(std::max<std::size_t>(256, capacity)) {}

	Claim claim(const std::string& source_key, const nlohmann::json& alarm) {
		const auto track_id = find_track_id(alarm);
		if (!track_id || track_id->empty() || *track_id == "0" || *track_id == "-1")
			return {};

		Claim result;
		result.key = source_key + "|track=" + *track_id;

		std::lock_guard<std::mutex> lock(mutex_);
		if (seen_.find(result.key) != seen_.end()) {
			result.accepted = false;
			++duplicates_;
			return result;
		}

		seen_.emplace(result.key, ++sequence_);
		trim_locked();
		return result;
	}

	void release(const Claim& claim) {
		if (claim.key.empty() || !claim.accepted) return;
		std::lock_guard<std::mutex> lock(mutex_);
		seen_.erase(claim.key);
	}

	void release(const std::vector<Claim>& claims) {
		if (claims.empty()) return;
		std::lock_guard<std::mutex> lock(mutex_);
		for (const auto& claim : claims) {
			if (!claim.key.empty() && claim.accepted) seen_.erase(claim.key);
		}
	}

	// Snapshot binding indices remain valid because the provider vector is not
	// compacted. Unused providers are released with the completed job.
	std::size_t filter_and_claim(nlohmann::json& root,
		const std::string& source_key,
		std::vector<Claim>* accepted_claims = nullptr) {
		if (!root.is_object() || !root.contains("alarms") ||
		    !root["alarms"].is_array())
			return 0;
		// Scene/group/line events own a lifecycle key rather than a stable
		// object identity. They may legitimately open again after recovery, so
		// they must not be retained in the process-wide track-id set. Producers
		// declare that distinction explicitly; legacy tracked producers keep the
		// default "track" behavior.
		if (root.value("dedupe_scope", std::string("track")) != "track") return 0;

		nlohmann::json kept = nlohmann::json::array();
		std::size_t removed = 0;
		for (const auto& alarm : root["alarms"]) {
			Claim claimed = claim(source_key, alarm);
			if (!claimed) {
				++removed;
				continue;
			}
			kept.push_back(alarm);
			if (accepted_claims && !claimed.key.empty())
				accepted_claims->push_back(std::move(claimed));
		}
		root["alarms"] = std::move(kept);
		root["alarm_count"] = root["alarms"].size();
		return removed;
	}

	std::uint64_t duplicate_count() const {
		std::lock_guard<std::mutex> lock(mutex_);
		return duplicates_;
	}

private:
	static std::optional<std::string> scalar_to_string(const nlohmann::json& value) {
		if (value.is_string()) return value.get<std::string>();
		if (value.is_number_unsigned()) return std::to_string(value.get<std::uint64_t>());
		if (value.is_number_integer()) return std::to_string(value.get<std::int64_t>());
		return std::nullopt;
	}

	static std::optional<std::string> find_track_id(const nlohmann::json& value,
	                                                int depth = 0) {
		if (depth > 8) return std::nullopt;
		if (value.is_object()) {
			if (const auto it = value.find("track_id"); it != value.end()) {
				if (auto scalar = scalar_to_string(*it); scalar && !scalar->empty())
					return scalar;
			}
			for (auto it = value.begin(); it != value.end(); ++it) {
				if (auto nested = find_track_id(it.value(), depth + 1)) return nested;
			}
		} else if (value.is_array()) {
			for (const auto& item : value) {
				if (auto nested = find_track_id(item, depth + 1)) return nested;
			}
		}
		return std::nullopt;
	}

	void trim_locked() {
		if (seen_.size() <= capacity_) return;
		std::vector<std::pair<std::string, std::uint64_t>> ordered;
		ordered.reserve(seen_.size());
		for (const auto& item : seen_) ordered.push_back(item);
		const std::size_t remove_count = seen_.size() - (capacity_ * 3 / 4);
		std::nth_element(
			ordered.begin(), ordered.begin() + static_cast<std::ptrdiff_t>(remove_count),
			ordered.end(), [](const auto& lhs, const auto& rhs) {
				return lhs.second < rhs.second;
			});
		for (std::size_t index = 0; index < remove_count; ++index)
			seen_.erase(ordered[index].first);
	}

	const std::size_t capacity_;
	mutable std::mutex mutex_;
	std::unordered_map<std::string, std::uint64_t> seen_{};
	std::uint64_t sequence_{0};
	std::uint64_t duplicates_{0};
};

}  // namespace jdk_alarm_dedupe
