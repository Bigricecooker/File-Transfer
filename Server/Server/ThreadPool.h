#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <thread>
#include <functional>
#include <chrono>


class ThreadPool
{
public:
	ThreadPool() = default;
	~ThreadPool();
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

	void Start(size_t minThreads, size_t maxThreads,
		std::chrono::milliseconds keepAlive = std::chrono::seconds(30));
	void Stop();
	bool Enqueue(std::function<void()> fn);

private:
	void WorkerLoop(bool dynamic);

	std::mutex m_mtx;
	std::condition_variable m_cv;
	std::queue<std::function<void()>> m_q;
	std::vector<std::thread> m_permanentThreads;
	size_t m_workerCount = 0;
	size_t m_idleCount = 0;
	bool m_running = false;
	size_t m_minThreads = 0;
	size_t m_maxThreads = 0;
	std::chrono::milliseconds m_keepAlive;
};

#endif
