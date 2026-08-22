#ifndef THREAD_CONTEXT_H
#define THREAD_CONTEXT_H

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>
struct session_st;
#define FILEMAX 1024
// #include "session.h"
struct ThreadContext {
	int epoll_fd{-1};
	// std::thread		   epoll_thd;
	struct session_st *session_arr[FILEMAX] = {nullptr};

	int				reloop_flag;
	std::atomic<int> run_flag{1};
	std::thread		epoll_thd;
	int				sum_client;
	pthread_mutex_t mut_session;
	pthread_mutex_t mut_clientcount;
	bool			epoll_started = false;
	// pthread_mutex_t	   mut_epoll = PTHREAD_MUTEX_INITIALIZER;
	std::mutex mut_epoll;
	// A client is kept here only while its RTSP handshake is handled outside
	// the epoll loop.  Tracking these descriptors lets shutdown wake and join
	// every handler before session storage is destroyed.
	std::mutex pending_client_mutex;
	std::condition_variable pending_client_cv;
	std::unordered_set<int> pending_client_fds;

	std::vector<char> mp4Dir;
	std::vector<char> buffer_recv;
	std::vector<char> buffer_send;

	ThreadContext() : mp4Dir(1024), buffer_recv(4096), buffer_send(4096) {
		reloop_flag = 1;
		sum_client	= 0;
		pthread_mutex_init(&mut_session, nullptr);
		pthread_mutex_init(&mut_clientcount, nullptr);
	}

	~ThreadContext() {
		run_flag.store(0, std::memory_order_release);
		if (epoll_thd.joinable())
			epoll_thd.join();
		pthread_mutex_destroy(&mut_session);
		pthread_mutex_destroy(&mut_clientcount);
	}
};

#endif	// THREAD_CONTEXT_H
