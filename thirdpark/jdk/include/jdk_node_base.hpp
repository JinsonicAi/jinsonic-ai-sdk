#ifndef __JDK_NODE_BASE_HPP__
#define __JDK_NODE_BASE_HPP__
#pragma once
// rmwei create.
#include <memory>
#include <string>
#include <vector>

#include "jdk_frame_meta.hpp"
#include "jdk_optional_handlers.hpp"

class AXVideoFrame;
namespace jdk_nodes {
class jdk_node;	 // forward declaration
}  // namespace jdk_nodes
typedef jdk_nodes::jdk_node* base_node_ptr;	 // for internal use of the sdk only

namespace jdk_nodes {
enum jdk_log_level {
	ERROR = 1,
	WARN  = 2,
	INFO  = 3,
	DEBUG = 4
};
void set_log_level(jdk_log_level level);

class jdk_node_base {
public:
	// virtual ~jdk_node_base() = default;
	virtual ~jdk_node_base() {
		std::cout << "[jdk_node_base] ~jdk_node_base called\n";
	}

	virtual void handle_run(std::stop_token stoken);
	virtual void handle_frame_meta(const std::vector<std::shared_ptr<jdk_objects::jdk_frame_meta>>& meta_with_batch);

	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_frame_meta(std::shared_ptr<jdk_objects::jdk_frame_meta> meta);
	virtual std::shared_ptr<jdk_objects::jdk_meta> handle_control_meta(std::shared_ptr<jdk_objects::jdk_control_meta> meta);

protected:
	// accessible bridge functions sdk internal injection
	void pend_meta(std::shared_ptr<jdk_objects::jdk_meta> meta);
	bool is_alive() noexcept;
	bool set_alive(bool v) noexcept;
	//  std::atomic<bool>* alive_ptr() noexcept
	const std::atomic<bool>* alive_ptr() const noexcept;
	// std::atomic<bool>& alive();
	int	 frame_index() const;
	void gate_knock();
	void gate_open();

	std::shared_ptr<jdk_objects::jdk_frame_meta>
		 make_frame_meta(std::shared_ptr<AXVideoFrame> frame, int frame_index = -1, int channel_index = -1, int original_width = 0, int original_height = 0, int fps = 0, uint64_t pts = 0);
	void out_queue_push(std::shared_ptr<jdk_objects::jdk_frame_meta>, std::atomic<bool>* thread_alive);
	int	 out_queue_size();

public:
	void start();
	void stop();
	void attach_to(std::vector<std::shared_ptr<jdk_node_base>> pre_nodes);
	void detach_recursively();
	// void netclient_stop();	// optional some nodes are private
	std::string node_name();
	int			node_type() const;
	int			node_fps() const;
	bool		connect_src_type() const;
	friend class ProxyNodeBridge;
	void bind_internal_node(jdk_node* ptr);

private:
	jdk_node* node_ptr_ = nullptr;
};

}  // namespace jdk_nodes
#endif
