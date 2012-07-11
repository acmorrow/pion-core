// ------------------------------------------------------------------------
// Pion is a development platform for building Reactors that process Events
// ------------------------------------------------------------------------
// Copyright (C) 2007-2008 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Pion is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Pion is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License for
// more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with Pion.  If not, see <http://www.gnu.org/licenses/>.
//

#include <functional>
#include <iostream>
#include <vector>
#include <boost/asio.hpp>
#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <boost/pool/pool.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>
#include <pion/PionId.hpp>
#include <pion/PionBlob.hpp>
#include <pion/PionScheduler.hpp>
#include <pion/PionLockedQueue.hpp>
#include <pion/PionPoolAllocator.hpp>
#include <pion/platform/Event.hpp>

#if 0
	// for tests that use the GNU "multithread" allocator included with GCC
	#include <ext/mt_allocator.h>
	/// data type for an Event allocator that uses the GCC "multithread" allocator
	typedef __gnu_cxx::__mt_alloc<pion::platform::Event,
		__gnu_cxx::__per_type_pool_policy<pion::platform::Event,
		__gnu_cxx::__pool, true> >	EventGCCPoolAlloc;
#endif

/// data type for a shared Event pool allocator using boost::fast_pool_allocator
typedef boost::fast_pool_allocator<pion::platform::Event>	EventBoostPoolAlloc;

/// MSVC and gcc> 4.2 are picky about template template parameters.  Even though
/// PionLockedQueue has default settings for the second two params, they will
/// not let us use it for the QueueType template parameter below.  To try to
/// work around this, we'll define a new class PionLockedQueueOneParam that
/// sets the default parameters through inheritance versus default settings
template <typename T>
class PionLockedQueueOneParam :
	public pion::PionLockedQueue<T, 250000, 10>
{};

/// default queue type used when there is at least 1 consumer thread
#define DEFAULT_QUEUE	PionLockedQueueOneParam

/// template parameters used for most of the unit test classes
#define TEST_TEMPLATE_PARAMS	unsigned int ProducerThreads = 1, \
	unsigned int ConsumerThreads = 0, \
	template <typename T> class QueueType = DEFAULT_QUEUE


using namespace std;
using namespace pion;
using namespace pion::platform;


///
/// PerformanceTest: interface class for Pion performance tests
///
class PerformanceTest {
public:

	/// the number of samples to take (one per second)
	enum { NUM_SAMPLES = 10 };

	/// data type for a pointer to a thread
	typedef boost::shared_ptr<boost::thread>	ThreadPtr;
	
	/// data type for a container of counters
	typedef std::vector<boost::uint64_t>		CounterContainer;
	
	/// data type for a container of threads
	typedef std::vector<ThreadPtr>			ThreadContainer;
	
	/// data type for a pointer to an EventAllocator
	typedef boost::shared_ptr<EventAllocator>	EventAllocatorPtr;
	
	
	/**
	 * constructs a new PerformanceTest
	 *
	 * @param test_description used to describe the performance test
	 */
	PerformanceTest(void)
		: m_counter(0), m_is_running(true), m_last_count(0),
		m_sample_sum(0), m_total_samples(0), m_samples_remaining(NUM_SAMPLES),
		m_count_description("counter"), m_service(), m_timer(m_service)
	{
		scheduleSample();
	}
	
	/// virtual destructor -> this class should be extended
	virtual ~PerformanceTest() { stop(); }

	/// starts the performance test
	virtual void start(void) = 0;
	
	/// stops the performance test
	virtual void stop(void) { if (m_is_running) stopTest(); }

	/// runs the performance test
	virtual void run(void) {
		std::cout << "Running test: " << m_test_description << std::endl;
		start();
		m_service.run();
	}
	
	/// returns true if the test is running
	inline bool isRunning(void) const { return m_is_running; }

	
protected:

	/// stops the performance test
	virtual void stopTest(void) { m_is_running=false; }

	/// sets a description for the performance test
	inline void setTestDescription(const std::string& d) { m_test_description=d; }

	/// sets a description for the counter
	inline void setCountDescription(const std::string& d) { m_count_description=d; }

	/// sets the number of counter samples remaining
	inline void setSamplesRemaining(const boost::uint32_t n) { m_samples_remaining=n; }

	
	/// the current value of the counter
	volatile boost::uint64_t			m_counter;

	/// true if the performance test is running
	volatile bool						m_is_running;


private:

	/// returns the current counter value
	virtual boost::uint64_t getCurrentCount(void) { return m_counter; }

	/// schedules a timer to print out the value of the counter
	inline void scheduleSample(void) {
		m_timer.expires_from_now(boost::posix_time::seconds(1));
		m_timer.async_wait(std::bind(&PerformanceTest::getSample, this));
	}

	/// gets a sample and prints out the value of the counter
	inline void getSample(void) {
		// get and print the value of the counter
		const boost::uint64_t current_count(getCurrentCount());
		cout << m_count_description << ": " << (current_count-m_last_count) << endl;

		// increase the counter sum and update last count
		m_sample_sum += (current_count-m_last_count);
		m_last_count = current_count;

		// update sample numbers and schedule another sample if necessary
		++m_total_samples;
		if (--m_samples_remaining > 0) {
			scheduleSample();
		} else {
			cout << m_count_description << " (avg): " << (m_sample_sum/m_total_samples)
				<< endl << endl;
			stop();
		}
	}

	/// the value of the counter when the last sample was taken
	boost::uint64_t					m_last_count;
	
	/// the sum of all of sample values
	boost::uint64_t					m_sample_sum;
	
	/// the total number of samples that have been taken
	boost::uint32_t					m_total_samples;
	
	/// the number of samples remaining
	boost::uint32_t					m_samples_remaining;
	
	/// a description of what the counter represents
	std::string						m_count_description;
	
	/// a description of the performance test
	std::string						m_test_description;
	
	/// a service used to schedule counter samples
	boost::asio::io_service			m_service;
	
	/// a timer used to schedule counter samples
	boost::asio::deadline_timer		m_timer;
};


///
/// HashValueTest: tests performance of hash_value(T)
///
template <typename T>
class HashValueTest :
	public PerformanceTest
{
public:

	/// default constructor
	HashValueTest(void) {
		PerformanceTest::setTestDescription("HashValueTest");
		PerformanceTest::setCountDescription("ops");
	}

	/// constructs with value to use for hashing
	template <typename X>
	HashValueTest(const X& x) :
		m_value(x)
	{
		PerformanceTest::setTestDescription("HashValueTest");
		PerformanceTest::setCountDescription("ops");
	}

	/// virtual destructor
	virtual ~HashValueTest() { PerformanceTest::stop(); }

	/// starts the performance test
	virtual void start(void) {
		m_consumer_ptr.reset(new boost::thread(std::bind(&HashValueTest::produce, this)));
	}

protected:

	/// produces hash values
	virtual void produce(void) {
		while (isRunning()) {
			(void)hash_value(m_value);
			++m_counter;
		}
	}
	
	/// stops the performance test
	virtual void stopTest(void) {
		PerformanceTest::stopTest();	// sets m_is_running = false
		m_consumer_ptr->join();		// ensures consumer thread has finished
		m_consumer_ptr.reset();
	}

	/// value used for hashing
	T		m_value;

	/// pointer to the consumer thread (needed for clean stop())
	ThreadPtr	m_consumer_ptr;
};


///
/// HashPionIdBlobTest: tests performance of HashPionIdBlob(PionBlob)
///
template <typename T>
class HashPionIdBlobTest :
	public HashValueTest<T>
{
public:

	/// default constructor
	HashPionIdBlobTest(void) {
		PerformanceTest::setTestDescription("HashPionIdBlobTest");
		PerformanceTest::setCountDescription("ops");
	}

	/// constructs with value to use for hashing
	template <typename X>
	HashPionIdBlobTest(const X& x) :
		HashValueTest<T>(x)
	{
		PerformanceTest::setTestDescription("HashPionIdBlobTest");
		PerformanceTest::setCountDescription("ops");
	}

	/// virtual destructor
	virtual ~HashPionIdBlobTest() { PerformanceTest::stop(); }

protected:

	/// produces hash values
	virtual void produce(void) {
		HashPionIdBlob hasher;
		while (HashValueTest<T>::isRunning()) {
			(void)hasher(HashValueTest<T>::m_value);
			++HashValueTest<T>::m_counter;
		}
	}
};


///
/// AllocTest: tests the performance of simultaneously generating and consuming
///            objects using 1 or more producers and 0 or more consumers
///
template <unsigned int ProducerThreads = 1, unsigned int ConsumerThreads = 0>
class AllocTest :
	public PerformanceTest
{
public:

	/// default constructor
	AllocTest(void) {
		for (unsigned int n = 0; n < ProducerThreads; ++n)
			m_producer_counters.push_back(0);
		for (unsigned int n = 0; n < ConsumerThreads; ++n)
			m_consumer_counters.push_back(0);
		PerformanceTest::setTestDescription("AllocTest");
		PerformanceTest::setCountDescription("events");
	}

	/// virtual destructor
	virtual ~AllocTest() { PerformanceTest::stop(); }

	/// starts the performance test
	virtual void start(void) {
		for (unsigned int n = 0; n < ConsumerThreads; ++n) {
			m_consumer_threads.push_back(ThreadPtr(new boost::thread(std::bind(&AllocTest::consume,
				this, std::ref(m_consumer_counters[n])))));
		}
		for (unsigned int n = 0; n < ProducerThreads; ++n) {
			m_producer_threads.push_back(ThreadPtr(new boost::thread(std::bind(&AllocTest::produce,
				this, std::ref(m_producer_counters[n])))));
		}
	}
	
	/// sets the test description, including the number of threads used
	inline void setTestDescriptionWithThreads(const std::string& description) {
		std::string d(description);
		d += " (";
		d += boost::lexical_cast<std::string>(ProducerThreads);
		if (ConsumerThreads > 0) {
			d += '+';
			d += boost::lexical_cast<std::string>(ConsumerThreads);
		}
		d += " threads)";
		PerformanceTest::setTestDescription(d);
	}

	/// work function that does nothing
	void doNothing(void) {}
    
	/// work function that just bumps the counter
	void bumpCounter(boost::uint64_t& thread_counter) { ++thread_counter; }
    

protected:

	/// returns the current counter value
	virtual boost::uint64_t getCurrentCount(void) {
		boost::uint64_t result = 0;
		if (ConsumerThreads == 0) {
			for (unsigned int n = 0; n < ProducerThreads; ++n)
				result += m_producer_counters[n];
		} else {
			for (unsigned int n = 0; n < ConsumerThreads; ++n)
				result += m_consumer_counters[n];
		}
		return result;
	}

	/// stops the performance test
	virtual void stopTest(void) {
		PerformanceTest::stopTest();	// sets m_is_running = false
		for (unsigned int n = 0; n < ProducerThreads; ++n) {
			if (m_producer_threads[n]) m_producer_threads[n]->join();
		}
		m_producer_threads.clear();
		for (unsigned int n = 0; n < ConsumerThreads; ++n) {
			if (m_consumer_threads[n]) m_consumer_threads[n]->join();
		}
		m_consumer_threads.clear();
	}

	/// thread function used to produce objects
	virtual void produce(boost::uint64_t& thread_counter) = 0;

	/// thread function used to consume objects
	virtual void consume(boost::uint64_t& thread_counter) = 0;

	
	/// require that there is at least one producer thread
	/// (it's ok to have 0 Consumer threads)
	BOOST_STATIC_ASSERT(ProducerThreads > 0);
	
	/// an array of counters used by the producer threads
	CounterContainer			m_producer_counters;

	/// an array of counters used by the consumer threads
	CounterContainer			m_consumer_counters;
	
	/// a collection of threads used to produce objects
	ThreadContainer				m_producer_threads;

	/// a collection of threads used to consume objects
	ThreadContainer				m_consumer_threads;
};


/// data type used to represent an item of work
typedef std::function<void()>	WorkItem;


///
/// WorkTestIoService: tests the raw baseline performance of using asio's
///                    io_service class to distribute work items
///
template <unsigned int ProducerThreads = 1, unsigned int ConsumerThreads = 0>
class WorkTestIoService :
	public AllocTest<ProducerThreads, ConsumerThreads>
{
public:
	
	/// default constructor
	WorkTestIoService(void)
		: m_service(), m_timer(m_service)
	{
		AllocTest<ProducerThreads, ConsumerThreads>::setTestDescriptionWithThreads("WorkTestIoService");
		keepRunning();
	}
	
	/// virtual destructor
	virtual ~WorkTestIoService() { PerformanceTest::stop(); }
	
protected:
	
    /// number of seconds a timer should wait for to keep the IO services running
    enum { KEEP_RUNNING_TIMER_SECONDS = 100 };
	
	
	/// stops the performance test
	virtual void stopTest(void) {
		PerformanceTest::stopTest();	// sets m_is_running = false
		m_timer.cancel();
		AllocTest<ProducerThreads, ConsumerThreads>::stopTest();
	}
	
	/// thread function used to produce work
	virtual void produce(boost::uint64_t& thread_counter) {
		WorkItem work(std::bind(&AllocTest<ProducerThreads, ConsumerThreads>::bumpCounter, this,
								  std::ref(AllocTest<ProducerThreads, ConsumerThreads>::m_consumer_counters[0])));
		while (PerformanceTest::isRunning()) {
			if (ConsumerThreads > 0)
				m_service.post(work);
			++thread_counter;
		}
	}
	
	/// thread function used to consume work
	virtual void consume(boost::uint64_t& thread_counter) {
		m_service.run();
	}
	
	/// function used to keep the io_service running
    void keepRunning(void) {
        if (PerformanceTest::isRunning()) {
            // schedule this again to make sure the service doesn't complete
            m_timer.expires_from_now(boost::posix_time::seconds(KEEP_RUNNING_TIMER_SECONDS));
            m_timer.async_wait(std::bind(&WorkTestIoService<ProducerThreads, ConsumerThreads>::keepRunning, this));
        }
    }
	
	
    /// a service used to queue work objects
    boost::asio::io_service         m_service;
	
    /// timer used to periodically check for shutdown
    boost::asio::deadline_timer     m_timer;
};


///
/// WorkTestSleepingQueue: tests the raw baseline performance of using a queue 
///                        with sleeping consumers to distribute work items
///
template < TEST_TEMPLATE_PARAMS >
class WorkTestSleepingQueue :
	public AllocTest<ProducerThreads, ConsumerThreads>
{
public:
	
	/// default constructor
	WorkTestSleepingQueue(void) {
		AllocTest<ProducerThreads, ConsumerThreads>::setTestDescriptionWithThreads("WorkTestSleepingQueue");
	}
	
	/// virtual destructor
	virtual ~WorkTestSleepingQueue() { PerformanceTest::stop(); }
	
protected:
	
	/// thread function used to produce work
	virtual void produce(boost::uint64_t& thread_counter) {
		WorkItem work(std::bind(&AllocTest<ProducerThreads, ConsumerThreads>::doNothing, this));
		while (PerformanceTest::isRunning()) {
			if (ConsumerThreads > 0)
				m_queue.push(work);
			++thread_counter;
		}
	}
	
	/// thread function used to consume work
	virtual void consume(boost::uint64_t& thread_counter) {
		WorkItem work;
		while (PerformanceTest::isRunning()) {
			// sleep 1/8 second if the queue is empty
			while (PerformanceTest::isRunning() && !m_queue.pop(work))
				PionScheduler::sleep(0, 125000000);
			if (! PerformanceTest::isRunning()) break;
			work();
			++thread_counter;
		}
		// consume the rest of the queue to make sure generate() doesn't hang
		while (m_queue.pop(work)) ;
	}
	
	
	/// a shared queue of work
	QueueType<WorkItem>			m_queue;
};


///
/// WorkTestBlockingQueue: tests the raw baseline performance of using a queue 
///                        with blocking consumers to distribute work items
///
template < TEST_TEMPLATE_PARAMS >
class WorkTestBlockingQueue :
	public WorkTestSleepingQueue<ProducerThreads, ConsumerThreads>
{
public:
	
	/// default constructor
	WorkTestBlockingQueue(void) {
		AllocTest<ProducerThreads, ConsumerThreads>::setTestDescriptionWithThreads("WorkTestBlockingQueue");
	}
	
	/// virtual destructor
	virtual ~WorkTestBlockingQueue() { PerformanceTest::stop(); }
	
protected:
	
	/// stops the performance test
	virtual void stopTest(void) {
		PerformanceTest::stopTest();	// sets m_is_running = false

		// make sure the consumer gets work after m_is_running == false
		if (ConsumerThreads > 0)
			WorkTestSleepingQueue<ProducerThreads, ConsumerThreads>::m_queue.push(
				std::bind(&AllocTest<ProducerThreads, ConsumerThreads>::doNothing, this));
		
		// stop the threads
		AllocTest<ProducerThreads, ConsumerThreads>::stopTest();
	}

	/// thread function used to consume objects
	virtual void consume(boost::uint64_t& thread_counter) {
		WorkItem work;
		typename QueueType<WorkItem>::ConsumerThread thread_info;
		while (PerformanceTest::isRunning()) {
			WorkTestSleepingQueue<ProducerThreads, ConsumerThreads>::m_queue.pop(work, thread_info);
			if (! PerformanceTest::isRunning()) break;
			++thread_counter;
		}
		// consume the rest of the queue to make sure generate() doesn't hang
		while (WorkTestSleepingQueue<ProducerThreads, ConsumerThreads>::m_queue.pop(work)) ;
	}
};


///
/// EventAllocTest: tests the performance of simultaneously generating
///                 Event objects in 1 or more threads using new/delete
///
template < TEST_TEMPLATE_PARAMS >
class EventAllocTest :
	public AllocTest<ProducerThreads, ConsumerThreads>
{
public:

	/// default constructor
	EventAllocTest(void) {
		AllocTest<ProducerThreads, ConsumerThreads>::setTestDescriptionWithThreads("EventAllocTest");
	}

	/// virtual destructor
	virtual ~EventAllocTest() { PerformanceTest::stop(); }


protected:

	/// thread function used to produce objects
	virtual void produce(boost::uint64_t& thread_counter) {
		Event *event_ptr(NULL);
		while (PerformanceTest::isRunning()) {
			event_ptr = new Event(Vocabulary::UNDEFINED_TERM_REF, NULL);
			if (ConsumerThreads == 0) {
				delete event_ptr;
			} else {
				m_queue.push(event_ptr);
			}
			++thread_counter;
		}
	}
	
	/// thread function used to consume objects
	virtual void consume(boost::uint64_t& thread_counter) {
		Event *event_ptr(NULL);
		while (PerformanceTest::isRunning()) {
			// sleep 1/8 second if the queue is empty
			while (PerformanceTest::isRunning() && !m_queue.pop(event_ptr))
				PionScheduler::sleep(0, 125000000);
			if (event_ptr) {
				delete event_ptr;
				event_ptr = NULL;	// prevent double-delete
			}
			if (! PerformanceTest::isRunning()) break;
			++thread_counter;
		}
		// consume the rest of the queue to make sure generate() doesn't hang
		while (m_queue.pop(event_ptr)) {
			delete event_ptr;
		}
	}
	
private:
	
	/// a shared queue of Event pointers
	QueueType<Event*>			m_queue;
};


///
/// EventSharedBoostPoolAllocTest: tests the performance of simultaneously
///                                generating Event objects in 1 or more threads
///                                using a shared boost::fast_pool_allocator
///
template < TEST_TEMPLATE_PARAMS >
class EventSharedBoostPoolAllocTest :
	public AllocTest<ProducerThreads, ConsumerThreads>
{
public:

	/// default constructor
	EventSharedBoostPoolAllocTest(void) {
		AllocTest<ProducerThreads, ConsumerThreads>::setTestDescriptionWithThreads("EventSharedBoostPoolAllocTest");
	}

	/// virtual destructor
	virtual ~EventSharedBoostPoolAllocTest() { PerformanceTest::stop(); }


protected:

	/// thread function used to produce objects
	virtual void produce(boost::uint64_t& thread_counter) {
		void *mem_ptr(NULL);
		Event *event_ptr(NULL);
		while (PerformanceTest::isRunning()) {
			mem_ptr = EventBoostPoolAlloc::allocate();
			event_ptr = new (mem_ptr) Event(Vocabulary::UNDEFINED_TERM_REF, NULL);
			if (ConsumerThreads == 0) {
				event_ptr->~Event();
				EventBoostPoolAlloc::deallocate(event_ptr);
			} else {
				m_queue.push(event_ptr);
			}
			++thread_counter;
		}
	}

	/// thread function used to consume objects
	virtual void consume(boost::uint64_t& thread_counter) {
		Event *event_ptr(NULL);
		while (PerformanceTest::isRunning()) {
			// sleep 1/8 second if the queue is empty
			while (PerformanceTest::isRunning() && !m_queue.pop(event_ptr))
				PionScheduler::sleep(0, 125000000);
			if (event_ptr) {
				event_ptr->~Event();
				EventBoostPoolAlloc::deallocate(event_ptr);
				event_ptr = NULL;	// prevent double-delete
			}
			if (! PerformanceTest::isRunning()) break;
			++thread_counter;
		}
		// consume the rest of the queue to make sure generate() doesn't hang
		while (m_queue.pop(event_ptr)) {
			event_ptr->~Event();
			EventBoostPoolAlloc::deallocate(event_ptr);
		}
	}
	
private:
	
	/// a shared queue of Event pointers
	QueueType<Event*>			m_queue;
};


///
/// EventUniqueBoostPoolAllocTest: tests the performance of simultaneously
///                                generating Event objects in 1 or more threads
///                                using unique instances of boost::fast_pool_allocator
///
template < TEST_TEMPLATE_PARAMS >
class EventUniqueBoostPoolAllocTest :
	public AllocTest<ProducerThreads, ConsumerThreads>
{
public:
	
	/**
	 * default constructor
	 *
	 * @param lock_pool if true, a lock will be acquired each time the memory pool is accessed
	 */
	explicit EventUniqueBoostPoolAllocTest(bool lock_pool = true)
		: m_lock_pool_access(lock_pool), m_pool_alloc(sizeof(Event))
	{
		if (lock_pool)
			AllocTest<ProducerThreads, ConsumerThreads>::setTestDescriptionWithThreads("EventUniqueBoostPoolAllocTest (with locks)");
		else
			AllocTest<ProducerThreads, ConsumerThreads>::setTestDescriptionWithThreads("EventUniqueBoostPoolAllocTest (no locks)");
	}
	
	/// virtual destructor
	virtual ~EventUniqueBoostPoolAllocTest() { PerformanceTest::stop(); }
	
	
protected:
	
	/// allocates a new Event object
	inline Event *allocateEvent(void) {
		void *mem_ptr(NULL);
		if (m_lock_pool_access) {
			boost::mutex::scoped_lock pool_malloc_lock(m_pool_mutex);
			mem_ptr = m_pool_alloc.malloc();
		} else {
			mem_ptr = m_pool_alloc.malloc();
		}
		return new (mem_ptr) Event(Vocabulary::UNDEFINED_TERM_REF, NULL);
	}

	/// deallocates an Event object
	inline void deallocateEvent(Event *event_ptr) {
		event_ptr->~Event();
		if (m_lock_pool_access) {
			boost::mutex::scoped_lock pool_free_lock(m_pool_mutex);
			m_pool_alloc.free(event_ptr);
		} else {
			m_pool_alloc.free(event_ptr);
		}
	}
	
	/// thread function used to produce objects
	virtual void produce(boost::uint64_t& thread_counter) {
		Event *event_ptr(NULL);
		while (PerformanceTest::isRunning()) {
			event_ptr = allocateEvent();
			if (ConsumerThreads == 0) {
				deallocateEvent(event_ptr);
			} else {
				m_queue.push(event_ptr);
			}
			++thread_counter;
		}
	}
	
	/// thread function used to consume objects
	virtual void consume(boost::uint64_t& thread_counter) {
		Event *event_ptr(NULL);
		while (PerformanceTest::isRunning()) {
			// sleep 1/8 second if the queue is empty
			while (PerformanceTest::isRunning() && !m_queue.pop(event_ptr))
				PionScheduler::sleep(0, 125000000);
			if (event_ptr) {
				deallocateEvent(event_ptr);
				event_ptr = NULL;	// prevent double-delete
			}
			if (! PerformanceTest::isRunning()) break;
			++thread_counter;
		}
		// consume the rest of the queue to make sure generate() doesn't hang
		while (m_queue.pop(event_ptr))
			deallocateEvent(event_ptr);
	}
	
private:
	
	/// a shared queue of Event pointers
	QueueType<Event*>			m_queue;

	/// true if a lock will be acquired each time the memory pool is accessed
	bool						m_lock_pool_access;

	/// memory pool used for allocations
	boost::pool<>				m_pool_alloc;

	/// mutex used to protect the memory pool
	boost::mutex				m_pool_mutex;
};


#if 0
///
/// EventSharedGCCPoolAllocTest: tests the performance of simultaneously 
///                              generating Event objects in 1 or more threads
///                              using a shared GCC "multithread" allocator
///
template < TEST_TEMPLATE_PARAMS >
class EventSharedGCCPoolAllocTest :
	public AllocTest<ProducerThreads, ConsumerThreads>
{
public:
	
	/// default constructor
	explicit EventSharedGCCPoolAllocTest(void) {
		AllocTest<ProducerThreads, ConsumerThreads>::setTestDescriptionWithThreads("EventSharedGCCPoolAllocTest");
		// tune the allocator
		__gnu_cxx::__pool_base::_Tune mt_tune(8, 5120, 8, 4096 - 4 * sizeof(void*), 128, 10, false);
		m_pool_alloc._M_set_options(mt_tune);
	}
	
	/// virtual destructor
	virtual ~EventSharedGCCPoolAllocTest() { PerformanceTest::stop(); }
	
protected:
	
	/// thread function used to produce objects
	virtual void produce(boost::uint64_t& thread_counter) {
		void *mem_ptr(NULL);
		Event *event_ptr(NULL);
		while (PerformanceTest::isRunning()) {
			mem_ptr = m_pool_alloc.allocate(1);
			event_ptr = new (mem_ptr) Event(Vocabulary::UNDEFINED_TERM_REF, NULL);
			if (ConsumerThreads == 0) {
				event_ptr->~Event();
				m_pool_alloc.deallocate(event_ptr, 1);
			} else {
				m_queue.push(event_ptr);
			}
			++thread_counter;
		}
	}
	
	/// thread function used to consume objects
	virtual void consume(boost::uint64_t& thread_counter) {
		Event *event_ptr(NULL);
		while (PerformanceTest::isRunning()) {
			// sleep 1/8 second if the queue is empty
			while (PerformanceTest::isRunning() && !m_queue.pop(event_ptr))
				PionScheduler::sleep(0, 125000000);
			if (event_ptr) {
				event_ptr->~Event();
				m_pool_alloc.deallocate(event_ptr, 1);
				event_ptr = NULL;	// prevent double-delete
			}
			if (! PerformanceTest::isRunning()) break;
			++thread_counter;
		}
		// consume the rest of the queue to make sure generate() doesn't hang
		while (m_queue.pop(event_ptr)) {
			event_ptr->~Event();
			m_pool_alloc.deallocate(event_ptr, 1);
		}
	}

private:
	
	/// a shared queue of Event pointers
	QueueType<Event*>			m_queue;

	/// memory pool allocator
	EventGCCPoolAlloc			m_pool_alloc;
};
#endif


///
/// EventPtrAllocTest: tests the performance of creating empty EventPtr objects
///                    using 1 or more threads
///
template < TEST_TEMPLATE_PARAMS >
class EventPtrAllocTest :
	public AllocTest<ProducerThreads, ConsumerThreads>
{
public:

	/// default constructor
	EventPtrAllocTest(void) {
		AllocTest<ProducerThreads, ConsumerThreads>::setTestDescriptionWithThreads("EventPtrAllocTest");
	}

	/// virtual destructor
	virtual ~EventPtrAllocTest() { PerformanceTest::stop(); }

protected:

	/// updates the contents of new events created
	virtual void updateEvent(EventPtr& e) {}

	/// thread function used to produce objects
	virtual void produce(boost::uint64_t& thread_counter) {
		EventPtr e;
		PerformanceTest::EventAllocatorPtr alloc_ptr(new EventAllocator);
		this->addAllocator(alloc_ptr);
		EventFactory f(*alloc_ptr);
		while (PerformanceTest::isRunning()) {
			e = f.create(Vocabulary::UNDEFINED_TERM_REF);
			updateEvent(e);
			if (ConsumerThreads > 0)
				m_queue.push(e);
			++thread_counter;
		}
	}

	/// thread function used to consume objects
	virtual void consume(boost::uint64_t& thread_counter) {
		EventPtr event_ptr;
		while (PerformanceTest::isRunning()) {
			// sleep 1/8 second if the queue is empty
			while (PerformanceTest::isRunning() && !m_queue.pop(event_ptr))
				PionScheduler::sleep(0, 125000000);
			if (! PerformanceTest::isRunning()) break;
			++thread_counter;
		}
		// consume the rest of the queue to make sure generate() doesn't hang
		while (m_queue.pop(event_ptr)) ;
	}

protected:

	/// adds an allocator pointer for later release
	void addAllocator(PerformanceTest::EventAllocatorPtr& ptr) {
		boost::mutex::scoped_lock alloc_lock(m_alloc_mutex);
		m_allocs.push_back(ptr);
	}

	
	/// used to allocate memory for Events (avoids memory bloat of using EventFactory standalone)
	std::vector<PerformanceTest::EventAllocatorPtr>		m_allocs;

	/// mutex used to protect the event allocator pointers
	boost::mutex				m_alloc_mutex;

	/// a shared queue of Event pointers
	QueueType<EventPtr>			m_queue;
};


///
/// CLFEventPtrAllocTest: tests the performance of creating EventPtr objects that
///                       contain common log format data using 1 or more threads
///
template < TEST_TEMPLATE_PARAMS >
class CLFEventPtrAllocTest :
	public EventPtrAllocTest<ProducerThreads, ConsumerThreads, QueueType>
{
public:

	/// default constructor
	CLFEventPtrAllocTest(void) {
		AllocTest<ProducerThreads, ConsumerThreads>::setTestDescriptionWithThreads("CLFEventPtrAllocTest");
	}

	/// virtual destructor
	virtual ~CLFEventPtrAllocTest() { PerformanceTest::stop(); }

protected:

	/// updates the contents of new events created
	virtual void updateEvent(EventPtr& e) {
		e->setString(1, "10.0.19.111");
		e->setString(2, "05/Apr/2007:05:37:11 -0600");
		e->setString(3, "GET /robots.txt HTTP/1.0");
		e->setUInt(4, 404);
		e->setUInt(5, 208);
	}
};


///
/// CLFEventPtrCopyTest: tests the performance of copying EventPtr objects that
///                      contain common log format data using 1 or more threads
///
template < TEST_TEMPLATE_PARAMS >
class CLFEventPtrCopyTest :
	public CLFEventPtrAllocTest<ProducerThreads, ConsumerThreads, QueueType>
{
public:

	/// default constructor
	CLFEventPtrCopyTest(void) {
		AllocTest<ProducerThreads, ConsumerThreads>::setTestDescriptionWithThreads("CLFEventPtrCopyTest");
	}

	/// virtual destructor
	virtual ~CLFEventPtrCopyTest() { PerformanceTest::stop(); }

protected:

	/// thread function used to produce objects
	virtual void produce(boost::uint64_t& thread_counter) {
		PerformanceTest::EventAllocatorPtr alloc_ptr(new EventAllocator);
		this->addAllocator(alloc_ptr);
		EventFactory f(*alloc_ptr);
		EventPtr copy;
		EventPtr e = f.create(Vocabulary::UNDEFINED_TERM_REF);
		this->updateEvent(e);
		while (PerformanceTest::isRunning()) {
			copy = f.create(Vocabulary::UNDEFINED_TERM_REF);
			*copy += *e;
			if (ConsumerThreads > 0)
				this->m_queue.push(copy);
			++thread_counter;
		}
	}
};


///
/// main control function for Pion performance tests
///
int main(void) {

	boost::scoped_ptr<PerformanceTest> test_ptr;
/*
	// run the EventAllocTest with one thread
	test_ptr.reset(new EventAllocTest<1>());
	test_ptr->run();

	// run the EventAllocTest with two threads
	test_ptr.reset(new EventAllocTest<2>());
	test_ptr->run();

	// run the EventSharedBoostPoolAllocTest with one thread
	test_ptr.reset(new EventSharedBoostPoolAllocTest<1>());
	test_ptr->run();

	// run the EventSharedBoostPoolAllocTest with two threads
	test_ptr.reset(new EventSharedBoostPoolAllocTest<2>());
	test_ptr->run();

	// run the EventUniqueBoostPoolAllocTest with one thread
	test_ptr.reset(new EventUniqueBoostPoolAllocTest<1>(false));
	test_ptr->run();

#if 0
	// run the EventSharedGCCPoolAllocTest with one thread
	test_ptr.reset(new EventSharedGCCPoolAllocTest<1>());
	test_ptr->run();

	// run the EventSharedGCCPoolAllocTest with two threads
	test_ptr.reset(new EventSharedGCCPoolAllocTest<2>());
	test_ptr->run();
#endif

	// run the EventPtrAllocTest with one thread
	test_ptr.reset(new EventPtrAllocTest<1>());
	test_ptr->run();

	// run the EventPtrAllocTest with two threads
	test_ptr.reset(new EventPtrAllocTest<2>());
	test_ptr->run();

	// run the EventPtrAllocTest with three threads
	test_ptr.reset(new EventPtrAllocTest<3>());
	test_ptr->run();

	// run the EventPtrAllocTest with four threads
	test_ptr.reset(new EventPtrAllocTest<4>());
	test_ptr->run();

*/
	{
	// run the HashValueTest for a PionBlob
	typedef PionBlob<char,PionPoolAllocator<> >	BlobType;
	PionPoolAllocator<> pool_alloc;
	BlobType b(pool_alloc, "bb49b9ca-e733-47c0-9a26-0f8f53ea1660");
	test_ptr.reset(new HashValueTest<BlobType>(b));
	test_ptr->run();

	// run the HashValueTest again but using HashPionIdBlob
	test_ptr.reset(new HashPionIdBlobTest<BlobType>(b));
	test_ptr->run();
	test_ptr.reset();	// needed to release the PionBlob reference
	}

	// run the HashValueTest for a PionId
	test_ptr.reset(new HashValueTest<PionId>("bb49b9ca-e733-47c0-9a26-0f8f53ea1660"));
	test_ptr->run();

	// run the CLFEventPtrAllocTest with one thread
	test_ptr.reset(new CLFEventPtrAllocTest<1>());
	test_ptr->run();

	// run the CLFEventPtrAllocTest with two threads
	test_ptr.reset(new CLFEventPtrAllocTest<2>());
	test_ptr->run();

	// run the CLFEventPtrAllocTest with three threads
	test_ptr.reset(new CLFEventPtrAllocTest<3>());
	test_ptr->run();

	// run the CLFEventPtrAllocTest with four threads
	test_ptr.reset(new CLFEventPtrAllocTest<4>());
	test_ptr->run();
/* */
	// run the CLFEventPtrCopyTest with one threads
	test_ptr.reset(new CLFEventPtrCopyTest<1>());
	test_ptr->run();

	// run the CLFEventPtrCopyTest with four threads
	test_ptr.reset(new CLFEventPtrCopyTest<4>());
	test_ptr->run();

	// run the WorkTestIoService with 1 producer & 1 consumer
	test_ptr.reset(new WorkTestIoService<1, 1>());
	test_ptr->run();
 
	// run the WorkTestSleepingQueue with 1 producer & 1 consumer
	test_ptr.reset(new WorkTestSleepingQueue<1, 1>());
	test_ptr->run();
	
	// run the WorkTestBlockingQueue with 1 producer & 1 consumer
	test_ptr.reset(new WorkTestBlockingQueue<1, 1>());
	test_ptr->run();
	
	// run the WorkTestIoService with 2 producers & 2 consumers
	test_ptr.reset(new WorkTestIoService<2, 2>());
	test_ptr->run();

	// run the WorkTestSleepingQueue with 2 producers & 2 consumers
	test_ptr.reset(new WorkTestSleepingQueue<2, 2>());
	test_ptr->run();
	
	// run the WorkTestBlockingQueue with 2 producers & 2 consumers
	test_ptr.reset(new WorkTestBlockingQueue<2, 2>());
	test_ptr->run();
	
	// run the EventAllocTest with 1 producer & 1 consumer
	test_ptr.reset(new EventAllocTest<1, 1>());
	test_ptr->run();

	// run the EventAllocTest with 2 producers & 2 consumers
	test_ptr.reset(new EventAllocTest<2, 2>());
	test_ptr->run();

	// run the CLFEventPtrAllocTest with 1 producer & 1 consumer
	test_ptr.reset(new CLFEventPtrAllocTest<1, 1>());
	test_ptr->run();
	
	// run the CLFEventPtrAllocTest with 2 producers & 2 consumers
	test_ptr.reset(new CLFEventPtrAllocTest<2, 2>());
	test_ptr->run();
	
	// run the CLFEventPtrAllocTest with 3 producers & 3 consumers
	test_ptr.reset(new CLFEventPtrAllocTest<3, 3>());
	test_ptr->run();
	
	// run the CLFEventPtrAllocTest with 4 producers & 4 consumers
	test_ptr.reset(new CLFEventPtrAllocTest<4, 4>());
	test_ptr->run();

	return 0;
}
