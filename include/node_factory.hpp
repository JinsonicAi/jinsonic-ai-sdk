#pragma once
#include <functional>
#include <json.hpp>
#include <map>
#include <memory>
#include <string>

#include "jdk_node_base.hpp"

class NodeFactory {
public:
	using Creator = std::function<std::shared_ptr<jdk_nodes::jdk_node_base>(const std::string& name, const nlohmann::json& config)>;
	static NodeFactory&						  instance();
	bool									  has_node_type(const std::string& type);
	void									  register_node(const std::string& type, Creator func);
	void									  unregister_node(const std::string& type);
	std::shared_ptr<jdk_nodes::jdk_node_base> create(const std::string& type, const std::string& name, const nlohmann::json& config);

private:
	std::map<std::string, Creator> creators_;
};

#define REGISTER_NODE(type, class_name)                                                                     \
	NodeFactory::instance().register_node(type, [](const std::string& name, const nlohmann::json& config) { \
		return jdk_nodes::jdk_node_wrapper::create(name, std::make_shared<class_name>(name, config));       \
	});
