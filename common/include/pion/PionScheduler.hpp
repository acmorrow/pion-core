// -----------------------------------------------------------------------
// pion-common: a collection of common libraries used by the Pion Platform
// -----------------------------------------------------------------------
// Copyright (C) 2007-2008 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_PIONSCHEDULER_HEADER__
#define __PION_PIONSCHEDULER_HEADER__

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/PionLogger.hpp>


namespace pion {	// begin namespace pion

///
/// PionScheduler: combines Boost.ASIO with a managed thread pool for scheduling
/// 
class PION_COMMON_API PionScheduler :
	private boost::noncopyable
{
public:

	/// constructs a new PionScheduler
	PionScheduler(void)
		: m_logger(PION_GET_LOGGER("pion.PionScheduler")),
		m_num_threads(DEFAULT_NUM_THREADS), m_active_users(0), m_is_running(false)
	{}
	
	/// virtual destructor
	virtual ~PionScheduler() {}

	/// Starts the thread scheduler (this is called automatically when necessary)
	virtual void startup(void) {}
	
	/// Stops the thread scheduler (this is called automatically when the program exits)
	virtual void shutdown(void);

	/// the calling thread will sleep until the scheduler has stopped
	void join(void);
	
	/// registers an active user with the thread scheduler.  Shutdown of the
	/// PionScheduler is deferred until there are no more active users.  This
	/// ensures that any work queued will not reference destructed objects
	void addActiveUser(void);

	/// unregisters an active user with the thread scheduler
	void removeActiveUser(void);
	
	/// returns true if the scheduler is running
	inline bool isRunning(void) const { return m_is_running; }
	
	/// sets the number of threads to be used (these are shared by all servers)
	inline void setNumThreads(const std::uint32_t n) { m_num_threads = n; }
	
	/// returns the number of threads currently in use
	inline std::uint32_t getNumThreads(void) const { return m_num_threads; }

	/// sets the logger to be used
	inline void setLogger(PionLogger log_ptr) { m_logger = log_ptr; }

	/// returns the logger currently in use
	inline PionLogger getLogger(void) { return m_logger; }
	
	/// returns an async I/O service used to schedule work
	virtual boost::asio::io_service& getIOService(void) = 0;
	
	/**
	 * schedules work to be performed by one of the pooled threads
	 *
	 * @param work_func work function to be executed
	 */
	virtual void post(std::function<void()> work_func) {
		getIOService().post(work_func);
	}
	
	/**
	 * thread function used to keep the io_service running
	 *
	 * @param my_service IO service used to re-schedule keepRunning()
	 * @param my_timer deadline timer used to keep the IO service active while running
	 */
	void keepRunning(boost::asio::io_service& my_service,
					 boost::asio::deadline_timer& my_timer);
	
	/**
	 * puts the current thread to sleep for a specific period of time
	 *
	 * @param sleep_sec number of entire seconds to sleep for
	 * @param sleep_nsec number of nanoseconds to sleep for (10^-9 in 1 second)
	 */
	inline static void sleep(std::uint32_t sleep_sec, std::uint32_t sleep_nsec) {
		std::this_thread::sleep_for(std::chrono::seconds(sleep_sec) + std::chrono::nanoseconds(sleep_nsec));
	}

	/**
	 * puts the current thread to sleep for a specific period of time, or until
	 * a wakeup condition is signaled
	 *
	 * @param wakeup_condition if signaled, the condition will wakeup the thread early
	 * @param wakeup_lock scoped lock protecting the wakeup condition
	 * @param sleep_sec number of entire seconds to sleep for
	 * @param sleep_nsec number of nanoseconds to sleep for (10^-9 in 1 second)
	 */
	template <typename ConditionType, typename LockType>
	inline static void sleep(ConditionType& wakeup_condition, LockType& wakeup_lock,
							 std::uint32_t sleep_sec, std::uint32_t sleep_nsec)
	{
		wakeup_condition.wait_for(wakeup_lock, std::chrono::seconds(sleep_sec) + std::chrono::nanoseconds(sleep_nsec));
	}
	
	
	/// processes work passed to the asio service & handles uncaught exceptions
	void processServiceWork(boost::asio::io_service& service);


protected:

	/// stops all services used to schedule work
	virtual void stopServices(void) {}
	
	/// stops all threads used to perform work
	virtual void stopThreads(void) {}

	/// finishes all services used to schedule work
	virtual void finishServices(void) {}

	/// finishes all threads used to perform work
	virtual void finishThreads(void) {}
	
	
	/// default number of worker threads in the thread pool
	static const std::uint32_t	DEFAULT_NUM_THREADS;

	/// number of nanoseconds in one full second (10 ^ 9)
	static const std::uint32_t	NSEC_IN_SECOND;

	/// number of microseconds in one full second (10 ^ 6)
	static const std::uint32_t	MICROSEC_IN_SECOND;
	
	/// number of seconds a timer should wait for to keep the IO services running
	static const std::uint32_t	KEEP_RUNNING_TIMER_SECONDS;


	/// mutex to make class thread-safe
	std::mutex					m_mutex;
	
	/// primary logging interface used by this class
	PionLogger						m_logger;

	/// condition triggered when there are no more active users
	std::condition_variable				m_no_more_active_users;

	/// condition triggered when the scheduler has stopped
	std::condition_variable				m_scheduler_has_stopped;

	/// total number of worker threads in the pool
	std::uint32_t					m_num_threads;

	/// the scheduler will not shutdown until there are no more active users
	std::uint32_t					m_active_users;

	/// true if the thread scheduler is running
	bool							m_is_running;
};

	
///
/// PionMultiThreadScheduler: uses a pool of threads to perform work
/// 
class PION_COMMON_API PionMultiThreadScheduler :
	public PionScheduler
{
public:
	
	/// constructs a new PionSingleServiceScheduler
	PionMultiThreadScheduler(void) {}
	
	/// virtual destructor
	virtual ~PionMultiThreadScheduler() {}

	
protected:
	
	/// stops all threads used to perform work
	virtual void stopThreads(void) {
		if (! m_thread_pool.empty()) {
			PION_LOG_DEBUG(m_logger, "Waiting for threads to shutdown");
			
			// wait until all threads in the pool have stopped
			for (ThreadPool::iterator i = m_thread_pool.begin();
				 i != m_thread_pool.end(); ++i)
			{
				// make sure we do not call join() for the current thread,
				// since this may yield "undefined behavior"
				if ((*i)->get_id() != std::this_thread::get_id()) (*i)->join();
			}
		}
	}
	
	/// finishes all threads used to perform work
	virtual void finishThreads(void) { m_thread_pool.clear(); }

	
	/// typedef for a pool of worker threads
	typedef std::vector<std::shared_ptr<std::thread> >	ThreadPool;
	
	
	/// pool of threads used to perform work
	ThreadPool				m_thread_pool;
};
	
	
///
/// PionSingleServiceScheduler: uses a single IO service to schedule work
/// 
class PION_COMMON_API PionSingleServiceScheduler :
	public PionMultiThreadScheduler
{
public:
	
	/// constructs a new PionSingleServiceScheduler
	PionSingleServiceScheduler(void)
		: m_service(), m_timer(m_service)
	{}
	
	/// virtual destructor
	virtual ~PionSingleServiceScheduler() { shutdown(); }
	
	/// returns an async I/O service used to schedule work
	virtual boost::asio::io_service& getIOService(void) { return m_service; }
	
	/// Starts the thread scheduler (this is called automatically when necessary)
	virtual void startup(void);
		
	
protected:
	
	/// stops all services used to schedule work
	virtual void stopServices(void) { m_service.stop(); }
	
	/// finishes all services used to schedule work
	virtual void finishServices(void) { m_service.reset(); }

	
	/// service used to manage async I/O events
	boost::asio::io_service			m_service;
	
	/// timer used to periodically check for shutdown
	boost::asio::deadline_timer		m_timer;
};
	

///
/// PionOneToOneScheduler: uses a single IO service for each thread
/// 
class PION_COMMON_API PionOneToOneScheduler :
	public PionMultiThreadScheduler
{
public:
	
	/// constructs a new PionOneToOneScheduler
	PionOneToOneScheduler(void)
		: m_service_pool(), m_next_service(0)
	{}
	
	/// virtual destructor
	virtual ~PionOneToOneScheduler() { shutdown(); }
	
	/// returns an async I/O service used to schedule work
	virtual boost::asio::io_service& getIOService(void) {
		std::lock_guard<std::mutex> scheduler_lock(m_mutex);
		while (m_service_pool.size() < m_num_threads) {
			std::shared_ptr<ServicePair>	service_ptr(new ServicePair());
			m_service_pool.push_back(service_ptr);
		}
		if (++m_next_service >= m_num_threads)
			m_next_service = 0;
		PION_ASSERT(m_next_service < m_num_threads);
		return m_service_pool[m_next_service]->first;
	}
	
	/**
	 * returns an async I/O service used to schedule work (provides direct
	 * access to avoid locking when possible)
	 *
	 * @param n integer number representing the service object
	 */
	virtual boost::asio::io_service& getIOService(std::uint32_t n) {
		PION_ASSERT(n < m_num_threads);
		PION_ASSERT(n < m_service_pool.size());
		return m_service_pool[n]->first;
	}

	/// Starts the thread scheduler (this is called automatically when necessary)
	virtual void startup(void);
	
	
protected:
	
	/// stops all services used to schedule work
	virtual void stopServices(void) {
		for (ServicePool::iterator i = m_service_pool.begin(); i != m_service_pool.end(); ++i) {
			(*i)->first.stop();
		}
	}
		
	/// finishes all services used to schedule work
	virtual void finishServices(void) { m_service_pool.clear(); }
	

	/// typedef for a pair object where first is an IO service and second is a deadline timer
	struct ServicePair {
		ServicePair(void) : first(), second(first) {}
		boost::asio::io_service			first;
		boost::asio::deadline_timer		second;
	};
	
	/// typedef for a pool of IO services
	typedef std::vector<std::shared_ptr<ServicePair> >		ServicePool;

	
	/// pool of IO services used to schedule work
	ServicePool						m_service_pool;

	/// the next service to use for scheduling work
	std::uint32_t					m_next_service;
};
	
	
}	// end namespace pion

#endif
