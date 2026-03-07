#pragma once

#include <assert.h>

#include <any>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace jdk_objects {
enum jdk_node_type {
	SRC,  // src node, must not have input branchs
	MID,  // middle node, can have input and output branchs
	DES,  // des node, must not have output branchs
};
// meta type
enum jdk_meta_type {
	FRAME,
	CONTROL
};

// meta trace field
// 1. sequence   ->int       ,sequence number the meta flowing through pipeline
// 2. node_name  ->string    ,name of current node the meta flow through
// 3. in_time    ->long      ,time when the meta arrive current node
// 4. out_time   ->long      ,time when the meta leave current node
// 5. text_info  ->vector    ,text info while the meta inside node
enum jdk_meta_trace_field {
	SEQUENCE,
	NODE_NAME,
	IN_TIME,
	OUT_TIME,
	TEXT_INFO
};

// base class for meta
class jdk_meta {
private:
protected:
	// std::map<std::string, std::map<jdk_meta_trace_field, std::any>> trace_table;
public:
	jdk_meta(jdk_meta_type meta_type, int channel_index);
	~jdk_meta();

	// the time when meta created
	std::chrono::system_clock::time_point create_time;

	jdk_meta_type meta_type;
	jdk_node_type node_type{jdk_node_type::SRC};

	std::string name;
	std::string image_name;

	// channel the meta belongs to
	int channel_index;

	// write trace record or not
	bool trace_on = false;

	// format traces string
	virtual std::string get_traces_str();

	// format meta string
	virtual std::string get_meta_str();

	// virtual clone method since we do not know what specific meta we need copy in some situations, return a new pointer pointting to new memory allocation in heap.
	// note: every child class need implement its own clone() method.
	virtual std::shared_ptr<jdk_meta> clone() = 0;

	// attach a new trace record for specific node (initialize key-value for current node)
	// void attach_trace(std::string node_name);

	// update trace record
	// void update_trace(std::string node_name, jdk_meta_trace_field trace_key, std::any trace_value);
};

}  // namespace jdk_objects