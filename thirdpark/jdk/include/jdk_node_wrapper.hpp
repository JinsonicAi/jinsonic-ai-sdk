#ifndef __JDK_NODE_WRAPPER_HPP__
#define __JDK_NODE_WRAPPER_HPP__
#pragma once
#include <cstddef>
#include <memory>
#include <string>

namespace jdk_nodes {
class jdk_node_base;
class ProxyNodeBridge;

class jdk_node_wrapper {
public:
	// static std::shared_ptr<jdk_node_wrapper> create(const std::string& node_name, std::shared_ptr<jdk_node_base> user_node);
	static std::shared_ptr<jdk_node_base> create(const std::string& node_name, std::shared_ptr<jdk_node_base> user_node);
	// Slow real-time consumers (notably inference nodes) should retain only the
	// newest pending decoder frame.  The two-argument overload is kept for ABI
	// compatibility with existing plugins.
	static std::shared_ptr<jdk_node_base> create(const std::string& node_name,
											  std::shared_ptr<jdk_node_base> user_node,
											  std::size_t input_queue_capacity);
	// Sampled inference branches may detach their frame from the VDEC pool so a
	// slow algorithm can never become the media pipeline's clock.
	static std::shared_ptr<jdk_node_base> create(const std::string& node_name,
											  std::shared_ptr<jdk_node_base> user_node,
											  std::size_t input_queue_capacity,
											  int sample_interval_ms,
											  bool isolate_input_frame);

	void start();
	void stop();

private:
	class Impl;
	std::shared_ptr<Impl> impl_;
};

}  // namespace jdk_nodes

#endif
