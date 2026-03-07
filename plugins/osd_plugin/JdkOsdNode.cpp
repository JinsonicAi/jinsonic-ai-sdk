#include "JdkOsdNode.hpp"

#include <unordered_map>

#include "anyx.hpp"
#include "post_node_info.h"

namespace jdk_nodes {
osdNode::osdNode(std::string node_name, int device_id, std::string task_id)
	: device_id_(device_id), task_id_(task_id) {
	if (ivps_ == nullptr) {
		ivps_ = std::make_shared<HwIvps>(device_id_, 0, 0);
	}
	reporter_.set_osd_config({task_id_,
							  PLUGIN_NODE_NAME});
	fmt::print("✅ osdNode constructed! BUILD TIME: {} {}\n", __DATE__, __TIME__);
}

osdNode::~osdNode() {
	stop();
	fmt::print("✅ osdNode destructed!\n");
}

void osdNode::stop() {
	set_alive(false);
	std::lock_guard<std::mutex> lk(mutex_);
	latest_result_map_snap_.store(std::make_shared<jdk_objects::result_map_t>(), std::memory_order_release);

	if (ivps_) {
		ivps_.reset();
	}
	fmt::print("✅ osdNode stop ok!\n");
}

void merge_result_map(
	std::shared_ptr<jdk_objects::jdk_frame_meta> meta,
	jdk_objects::result_map_t&					 result_cache) {
	std::lock_guard<std::mutex> lk(*meta->mtx_);

	for (const auto& [key, entry] : result_cache) {
		// if there is no result of this key in the current frame,just merge it in
		if (!meta->result_map_.contains(key)) {
			meta->result_map_[key].exchange(entry);
		}
	}
	result_cache.clear();
}

void osdNode::update_latest_result_(const std::string&							  key,
									const jdk_objects::result_map_t::mapped_type& entry_sp) {
	// read old snapshots
	auto old = latest_result_map_snap_.load(std::memory_order_acquire);
	if (!old) old = std::make_shared<jdk_objects::result_map_t>();
	// copy to create a new snapshot(a scene of reading more and writing less,this expense is acceptable;the reading path has no locks in return)
	auto next	 = std::make_shared<jdk_objects::result_map_t>(*old);
	(*next)[key] = entry_sp;  // cover or insert
	// atomic replacement
	latest_result_map_snap_.store(next, std::memory_order_release);
}

std::shared_ptr<const jdk_objects::result_map_t> osdNode::load_latest_result_snapshot_() const {
	auto s = latest_result_map_snap_.load(std::memory_order_acquire);
	if (!s) s = std::make_shared<jdk_objects::result_map_t>();
	return s;
}

// handle frame meta one by one
std::shared_ptr<jdk_objects::jdk_meta> osdNode::handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta) {
	std::lock_guard<std::mutex> lk(mutex_);
	if (!is_alive()) return nullptr;
	if (!meta || (!meta->frame_)) {
		std::cerr << "❌ meta is null or contains no valid frame.\n";
		return jdk_node_base::handle_frame_meta(meta);
	}
	// ivps_->HwDrawOsd(meta->frame_->raw(), {100, 100}, "AXERA AI BOX,中文 OSD. ", 64, cv::Scalar(0, 255, 0, 255), 128);
	std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>> frame_meta_with_batch{meta};

	// Automatic timing, write the time spent to reporter_ at the end of the function.
	MetricsReporter::ScopedTimer timer(reporter_);

	if (jdk_node_base::connect_src_type()) {
		if (jdk_objects::jdk_node_type::SRC == meta->node_type) {
			// merge_result_map(meta, latest_result_map_);
			// based on atomic snapshots Copy-On-Write: copy -> merge -> atomic replacement
			{
				auto snap = load_latest_result_snapshot_();
				auto next = std::make_shared<jdk_objects::result_map_t>(*snap);	 // variable copy
				merge_result_map(meta, *next);									 // merge directly into the copy
				latest_result_map_snap_.store(next, std::memory_order_release);	 // replace the atom with the new snapshot
			}
			jdk_objects::result_map_t local_copy_src;
			{
				std::lock_guard<std::mutex> lk(*meta->mtx_);
				local_copy_src = meta->result_map_;
			}
			for (const auto& [key, entry_sp] : local_copy_src) {
				const auto entry_shared = entry_sp.load();
				if (!entry_shared) {
					fprintf(stderr, "[OSD] entry is nullptr for key [%s]\n", key.c_str());
					continue;
				}
				// anyx: unified reading shared_ptr<any>
				auto v = anyx::view(entry_shared->result);
				if (!entry_shared->render_fn || !v) {
					std::cerr << "OSD: result for [" << key << "] is empty or render_fn missing.\n";
					continue;
				}
				try {
					if (!meta->dump_frame_) {
						meta->dump_frame_ = meta->frame_->clone();
						if (!meta->dump_frame_) {
							fprintf(stderr, "OSD: clone failed, fallback to original frame\n");
						}
					}
					std::shared_ptr<AXVideoFrame> canvas =
						meta->dump_frame_ ? meta->dump_frame_  // use decorated frame if we have one
										  : meta->frame_;	   // otherwise fall back to raw frame
					// no more any_cast,directly take the bottom layer any quoted to hand over render_fn;
					// render_fn used internally anyx::visit performed type distribution and alerts
					entry_shared->render_fn(canvas, *v.a, ivps_);
				} catch (const std::exception& e) {
					std::cerr << "OSD: Error processing [" << key << "]: " << e.what() << "\n";
				}
			}
		} else {
			jdk_objects::result_map_t local_copy;
			{
				std::lock_guard<std::mutex> lk(*meta->mtx_);  //
				local_copy = meta->result_map_;				  // take a snapshot
			}
			for (const auto& [key, entry_sp] : local_copy) {
				const auto& entry = entry_sp.load();
				auto		v	  = entry ? anyx::view(entry->result) : anyx::view({});
				if (v) {
					// update the global cache
					// latest_result_map_[key] = entry_sp;
					update_latest_result_(key, entry_sp);
				}
			}
			return nullptr;
		}
	} else {
		std::lock_guard<std::mutex> lk(*meta->mtx_);
		for (const auto& [key, entry_sp] : meta->result_map_) {
			const auto entry_shared = entry_sp.load();
			if (!entry_shared) {
				fprintf(stderr, "[OSD] entry is nullptr for key [%s]\n", key.c_str());
				continue;
			}
			auto v = anyx::view(entry_shared->result);
			if (!v) {
				std::cerr << "OSD: result for [" << key << "] is empty.\n";
				continue;
			}
			try {
				if (!meta->dump_frame_) {
					meta->dump_frame_ = meta->frame_->clone();
					if (!meta->dump_frame_) {
						fprintf(stderr, "OSD: clone failed, fallback to original frame\n");
					}
				}
				std::shared_ptr<AXVideoFrame> canvas =
					meta->dump_frame_ ? meta->dump_frame_  // use decorated frame if we have one
									  : meta->frame_;	   // otherwise fall back to raw frame
				// direct call render_fn; type mismatch caused by render_fn internal self alarm and skip
				entry_shared->render_fn(canvas, *v.a, ivps_);
			} catch (const std::exception& e) {
				std::cerr << "OSD: Error processing [" << key << "]: " << e.what() << "\n";
			}
		}
	};
	// report node info
	reporter_.report_osd(jdk_node_base::node_fps(), meta->create_time);
	return jdk_node_base::handle_frame_meta(meta);
}

void osdNode::handle_frame_meta(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch) {
	const auto& frame_meta_with_batch = meta_with_batch;
	// run_infer_combinations(frame_meta_with_batch);
}

std::shared_ptr<jdk_objects::jdk_meta> osdNode::handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta) {
	if (!meta) return nullptr;
	std::cout << "[osdNode] handle_control_meta: control_type = " << meta->control_type << std::endl;
	if (meta->control_type == jdk_objects::jdk_control_type::SPEAK) this->stop();

	return meta;
};

}  // namespace jdk_nodes
