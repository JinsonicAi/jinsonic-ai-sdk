#pragma once
// InferenceEngine.h
#ifndef INFERENCEENGINE_H
#define INFERENCEENGINE_H
#include <any>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

// #include "IPlugin.hpp"
class IInferencePlugin;
using InferencePluginPtr = std::shared_ptr<IInferencePlugin>;

class EXPORT_VISIBILITY InferenceEngine : public std::enable_shared_from_this<InferenceEngine> {
public:
	struct Job {
		std::any input;
		std::any output;
		int		 batch_size{0};

		std::shared_ptr<std::promise<std::any>> pro;
	};

	InferenceEngine();
	virtual ~InferenceEngine();
	// virtual ~InferenceEngine() = default;

	void			   registerPlugin(const std::string& name, const InferencePluginPtr& plugin);
	InferencePluginPtr findPlugin(const std::string& name) const;
	bool			   startup(std::tuple<std::string, std::string> param, int device_id, const std::string &model_name = "");
	void			   stop();

	std::shared_future<std::any>			  commit(const std::any& input);
	std::vector<std::shared_future<std::any>> commits(const std::vector<std::any>& inputs);

protected:
	// virtual void						thread_process(std::promise<bool>& result)					   = 0;
	virtual bool	 pre_process(Job& job, const std::any& input) = 0;
	virtual std::any post_process(const Job& job)				  = 0;

	InferencePluginPtr engine{nullptr};

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};
using InferenceEnginePtr = std::shared_ptr<InferenceEngine>;
#endif	// INFERENCEENGINE_H
