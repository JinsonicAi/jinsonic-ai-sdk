#pragma once

#include <functional>
#include <json.hpp>
#include <memory>
#include <string>

#include "anyx.hpp"
#include "jdk_node_base.hpp"
#include "json_utils.hpp"

using namespace json_utils;

struct SDKInterface {
	// register the node
	using NodeCreator = std::function<std::shared_ptr<jdk_nodes::jdk_node_base>(const std::string&, const nlohmann::json&)>;
	std::function<void(const std::string&, NodeCreator)> register_node;
	std::function<void(const std::string&)>				 unregister_node;
	// log interface（level: info/debug/warn/error）
	std::function<void(const std::string& level, const std::string& message)> log;

	// get configuration return string
	std::function<std::string(const std::string& key)> get_config;

	// publish an event or message
	std::function<void(const std::string& topic, const std::string& payload)> publish_event;
};
