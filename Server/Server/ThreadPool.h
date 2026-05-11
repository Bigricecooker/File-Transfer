#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <thread>
#include <functional>


class ThreadPool
{
public:
	ThreadPool() = default;
	~ThreadPool();
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	void Start(size_t threadCount);
	void Stop();
	void Enqueue(std::function<void()> fn);

private:
	std::mutex m_mtx;
	std::condition_variable m_cv;
	std::queue<std::function<void()>> m_q;
	std::vector<std::thread> m_threads;
	bool m_running = false;
};

#endif