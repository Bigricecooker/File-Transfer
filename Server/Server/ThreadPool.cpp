#include "ThreadPool.h"
#include <iostream>

ThreadPool::~ThreadPool()
{
	Stop();
}

void ThreadPool::Start(size_t minThreads, size_t maxThreads,
	std::chrono::milliseconds keepAlive)
{
	Stop();
	m_minThreads = std::max<size_t>(minThreads, 1);
	m_maxThreads = std::max(m_minThreads, maxThreads);
	m_keepAlive = keepAlive;
	m_running = true;
	m_workerCount = 0;
	m_idleCount = 0;

	m_permanentThreads.reserve(m_minThreads);
	for (size_t i = 0; i < m_minThreads; ++i)
	{
		++m_workerCount;
		m_permanentThreads.emplace_back([this]() { WorkerLoop(false); });
	}
}

void ThreadPool::WorkerLoop(bool dynamic)
{
	for (;;)
	{
		std::function<void()> fn;
		{
			std::unique_lock<std::mutex> lk(m_mtx);

			++m_idleCount;

			if (dynamic)
			{
				if (!m_cv.wait_for(lk, m_keepAlive,
					[this] { return !m_running || !m_q.empty(); }))
				{
					--m_idleCount;
					if (m_workerCount > m_minThreads)
					{
						--m_workerCount;
						return;
					}
					continue;
				}
			}
			else
			{
				m_cv.wait(lk, [this] { return !m_running || !m_q.empty(); });
			}

			--m_idleCount;

			if (!m_running && m_q.empty())
			{
				--m_workerCount;
				return;
			}

			fn = std::move(m_q.front());
			m_q.pop();
		}

		try { fn(); }
		catch (const std::exception& e) {
			std::cerr << "[ThreadPool] " << e.what() << std::endl;
		}
		catch (...) {}
	}
}

void ThreadPool::Stop()
{
	{
		std::lock_guard<std::mutex> lk(m_mtx);
		while (!m_q.empty())
			m_q.pop();
		m_running = false;
	}
	m_cv.notify_all();

	for (auto& t : m_permanentThreads)
		if (t.joinable()) t.join();
	m_permanentThreads.clear();

	{
		std::unique_lock<std::mutex> lk(m_mtx);
		m_cv.wait(lk, [this] { return m_workerCount == 0; });
	}
}

bool ThreadPool::Enqueue(std::function<void()> fn)
{
	{
		std::lock_guard<std::mutex> lk(m_mtx);
		if (!m_running) return false;

		m_q.push(std::move(fn));

		if (m_idleCount == 0 && m_workerCount < m_maxThreads)
		{
			++m_workerCount;
			std::thread([this]() { WorkerLoop(true); }).detach();
		}
	}
	m_cv.notify_one();
	return true;
}
