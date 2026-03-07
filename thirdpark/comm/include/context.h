#ifndef THREAD_CONTEXT_H
#define THREAD_CONTEXT_H

#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
struct session_st;
#define FILEMAX 1024
// #include "session.h"
struct ThreadContext {
	int epoll_fd;
	// std::thread		   epoll_thd;
	struct session_st *session_arr[FILEMAX] = {nullptr};

	int				reloop_flag;
	int				run_flag;
	std::thread		epoll_thd;
	int				sum_client;
	pthread_mutex_t mut_session;
	pthread_mutex_t mut_clientcount;
	bool			epoll_started = false;
	// pthread_mutex_t	   mut_epoll = PTHREAD_MUTEX_INITIALIZER;
	std::mutex mut_epoll;

	std::vector<char> mp4Dir;
	std::vector<char> buffer_recv;
	std::vector<char> buffer_send;

	ThreadContext() : mp4Dir(1024), buffer_recv(4096), buffer_send(4096) {
		reloop_flag = 1;
		run_flag	= 1;
		sum_client	= 0;
		pthread_mutex_init(&mut_session, nullptr);
		pthread_mutex_init(&mut_clientcount, nullptr);
	}
};

#endif	// THREAD_CONTEXT_H
