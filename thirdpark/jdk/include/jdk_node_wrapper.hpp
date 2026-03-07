#ifndef __JDK_NODE_WRAPPER_HPP__
#define __JDK_NODE_WRAPPER_HPP__
#pragma once
#include <memory>
#include <string>

namespace jdk_nodes {
class jdk_node_base;
class ProxyNodeBridge;

class jdk_node_wrapper {
public:
	// static std::shared_ptr<jdk_node_wrapper> create(const std::string& node_name, std::shared_ptr<jdk_node_base> user_node);
	static std::shared_ptr<jdk_node_base> create(const std::string& node_name, std::shared_ptr<jdk_node_base> user_node);

	void start();
	void stop();

private:
	class Impl;
	std::shared_ptr<Impl> impl_;
};

}  // namespace jdk_nodes

#endif
