#include "ThreadPool.h"

ThreadPool::~ThreadPool()
{
	Stop();
}

void ThreadPool::Start(size_t threadCount)
{
	Stop();
	m_running = true;
	if (threadCount == 0)
		threadCount = 1;

	m_threads.reserve(threadCount);
	for (size_t i = 0; i < threadCount; ++i)
	{
		m_threads.emplace_back([this]() {
			for (;;)
			{
				std::function<void()> fn;
				{
					std::unique_lock<std::mutex> lk(m_mtx);
					m_cv.wait(lk, [this]() { return !m_running || !m_q.empty(); });
					if (!m_running && m_q.empty())
						return;
					fn = std::move(m_q.front());
					m_q.pop();
				}
				try
				{
					fn();
				}
				catch (...)
				{
				}
			}
			});
	}
}

void ThreadPool::Stop()
{
	{
		std::lock_guard<std::mutex> lk(m_mtx);
		m_running = false;
	}
	m_cv.notify_all();
	for (auto& t : m_threads)
	{
		if (t.joinable())
			t.join();
	}
	m_threads.clear();
	{
		std::lock_guard<std::mutex> lk(m_mtx);
		while (!m_q.empty())
			m_q.pop();
	}
}

void ThreadPool::Enqueue(std::function<void()> fn)
{
	{
		std::lock_guard<std::mutex> lk(m_mtx);
		if (!m_running)
			return;
		m_q.push(std::move(fn));
	}
	m_cv.notify_one();
}