// ------------------------------------------------------------------------
// Pion is a development platform for building Reactors that process Events
// ------------------------------------------------------------------------
// Copyright (C) 2007-2011 Atomic Labs, Inc.  (http://www.atomiclabs.com)
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

#ifndef __PION_EVENT_HEADER__
#define __PION_EVENT_HEADER__

#include <functional>
#include <list>
#include <vector>
#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable: 4181) // qualifier applied to reference type
#endif
#include <boost/variant.hpp>
#ifdef _MSC_VER
	#pragma warning(pop)
#endif
#include <boost/thread/once.hpp>
#include <mutex>
#include <boost/utility/enable_if.hpp>
#include <boost/detail/atomic_count.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/intrusive/rbtree_algorithms.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionLogger.hpp>
#include <pion/PionBlob.hpp>
#include <pion/PionDateTime.hpp>
#include <pion/platform/Vocabulary.hpp>


/// uncomment the following to use pool allocators for Event memory management
#ifndef _WIN64
#define PION_EVENT_USE_POOL_ALLOCATORS
#endif

#ifdef PION_EVENT_USE_POOL_ALLOCATORS
	#include <boost/thread/tss.hpp>
	#include <pion/PionPoolAllocator.hpp>
#else
	#include <cstdlib>
#endif


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


#ifdef PION_EVENT_USE_POOL_ALLOCATORS

	/// default allocator used to handle memory operations for Pion Events
	typedef PionPoolAllocator<16, 256>			EventAllocator;

#else
	
	/// Event allocator type used if the use of memory pools is disabled
	struct DummyEventAllocator {
		/**
		 * allocates a block of memory
		 *
		 * @param n minimum size of the new memory block, in bytes
		 *
		 * @return void * raw pointer to the new memory block
		 */
		inline void *malloc(std::size_t n) { return ::malloc(n); }
		
		/**
		 * deallocates a block of memory
		 *
		 * @param ptr raw pointer to the block of memory
		 * @param n requested size of the memory block, in bytes (actual size may be larger)
		 */
		inline void free(void *ptr, std::size_t n) { ::free(ptr); }
	};
	
	/// default allocator used to handle memory operations for Pion Events
	typedef DummyEventAllocator			EventAllocator;

#endif


/// used to perform utf8 validation/cleansing on event content
class PION_PLATFORM_API EventValidator {
public:
	/// exception thrown when a NULL pointer was passed as the source for UTF-8 conversion
	class NullSourcePointerException : public PionException {
	public:
		NullSourcePointerException(void)
			: PionException("In EventValidator, a NULL pointer was passed as the source for UTF-8 conversion") {}
	};

	/// exception thrown if an ICU function returns an unexpected error code
	class UnexpectedICUErrorCodeException : public PionException {
	public:
		UnexpectedICUErrorCodeException(const std::string& icu_func, const std::string& u_error_code_str)
			: PionException("Unexpected ICU error code in EventValidator: ", icu_func + " () returned " + u_error_code_str) {}
	};

	/// exception thrown when an std::bad_alloc exception is caught
	class BadAllocException : public PionException {
	public:
		BadAllocException(const std::string& error_msg)
			: PionException("std::bad_alloc in EventValidator: ", error_msg) {}
	};

	/**
	 * Validates UTF-8 data.
	 * Passing a non-NULL trimmed_len allows efficiently dealing with the case where ptr + len
	 * is in the middle of a UTF-8 code point (presumably due to truncating).
	 *
	 * @param ptr pointer to buffer of UTF-8 data to check
	 * @param len length of buffer
	 * @param trimmed_len if not NULL, and data is valid except for a possibly incomplete final code point, gets offset to end of last complete code point 
	 *
	 * @return if trimmed_len is NULL, true iff buffer is valid UTF-8, else true iff buffer of length *trimmed_len is valid UTF-8
	 */
	static bool isValidUTF8(const char* ptr, std::size_t len, std::size_t* trimmed_len);

	/**
	 * Returns an upper bound on the length needed for a string that replaces invalid UTF-8 characters
	 * in ptr with replacement characters (U+FFFD)
	 *
	 * @param ptr pointer to buffer of UTF-8 data to check
	 * @param len length of buffer
	 *
	 * @return upper bound on length of buffer for cleansed data
	 */
	static size_t getCleansedUTF8Length(const char* ptr, std::size_t len);

	/**
	 * Copies an input buffer into another buffer, replacing invalid UTF-8 characters with
	 * replacement characters (U+FFFD).
	 *
	 * @param blob_alloc memory allocator to use for intermediate buffer
	 * @param ptr pointer to buffer of UTF-8 data to check
	 * @param len length of buffer 
	 * @param buf pointer to output buffer (assumed to be long enough - see getCleansedUTF8Length())
	 * @param buf_len pointer to length of output buffer actually used (could be less than what getCleansedUTF8Length() returned)
	 */
	static void cleanseUTF8(EventAllocator& blob_alloc, const char* ptr, std::size_t len, char* buf, size_t* buf_len);

	/**
	 * Copies an input buffer into another buffer, replacing invalid UTF-8 characters with
	 * replacement characters (U+FFFD).  (Temporary until calls to this function can be replaced 
	 * with calls to cleanseUTF8(), which allows more efficient memory allocation.)
	 *
	 * @param ptr pointer to buffer of UTF-8 data to check
	 * @param len length of buffer 
	 * @param buf pointer to output buffer (assumed to be long enough - see getCleansedUTF8Length())
	 * @param buf_len pointer to length of output buffer actually used (could be less than what getCleansedUTF8Length() returned)
	 */
	static void cleanseUTF8_TEMP(const char* ptr, std::size_t len, char* buf, size_t* buf_len);
};


/// used for logging of Event related messages
class PION_PLATFORM_API EventMessageLogger {
public:
	/**
	 * Gets the logger used for Event messages.
	 *
	 * @return PionLogger& the logger used for Event messages
	 */
	static PionLogger& get(void) {
		if (m_logger_ptr == NULL) {
			m_logger_ptr = new Logger();
		}
		return m_logger_ptr->m_logger;
	}

private:
	// Helper class to avoid m_logger potentially being initialized before the logging library itself is initialized,
	// which it might be if m_logger were a static member of EventMessageLogger.
	struct Logger {
		Logger(void) : m_logger(PION_GET_LOGGER("pion.platform.Event")) {};

		PionLogger		m_logger;
	};

	static Logger* m_logger_ptr;
};


///
/// Event: an item of structured data that represents something of interest
///
template <typename CharType, typename AllocType>
class BasicEvent
	: private boost::noncopyable
{
public:

	/// forward declaration for use by IteratorBase
	struct ParameterNode;
	
	/// exception thrown if you try to serialize a term that cannot be serialized
	class TermTypeNotSerializableException : public std::exception {
	public:
		virtual const char* what() const throw() {
			return "Term type is not serializable";
		}
	};

	/// A PionException wrapper used to include a term_ref for error reporting higher up the call chain.
	class PionExceptionWithTermRef : public PionException {
	public:
		PionExceptionWithTermRef(const std::string& msg, Vocabulary::TermRef term_ref)
			: PionException(msg), m_term_ref(term_ref)
		{}

		Vocabulary::TermRef m_term_ref;
	};

protected:

	/// base class used to define Event parameter iterator types
	template <typename NodeType>
	class IteratorBase :
		public boost::iterator_facade<IteratorBase<NodeType>, NodeType,
			boost::bidirectional_traversal_tag>
	{
	private:
		
		/// used by boost::enable_if
		struct enabler {};
		
	public:
		
		/// default constructor
		IteratorBase(void) { tree_algo::init(&m_param_ptr); }
		
		/// constructs an iterator using a parameter node pointer
		explicit IteratorBase(NodeType *p) : m_param_ptr(p) {}
		
		/// templatized copy constructor allows mutable->const conversion
		template <class OtherNodeType>
		IteratorBase(IteratorBase<OtherNodeType> const& other,
					 typename boost::enable_if<
						boost::is_convertible<OtherNodeType*,NodeType*>,
					 enabler>::type = enabler()
					 )
			: m_param_ptr(other.m_param_ptr)
		{}
		
	private:
		
		/// increments the parameter node iterator (steps to next node)
		inline void increment(void) {
			m_param_ptr = tree_algo::next_node(const_cast<ParameterNode*>(m_param_ptr));
		}
		
		/// decrements the parameter node iterator (steps to previous node)
		inline void decrement(void) {
			m_param_ptr = tree_algo::prev_node(const_cast<ParameterNode*>(m_param_ptr));
		}
		
		/// returns true if this iterator references the same node as other
		template <class OtherValue>
		inline bool equal(IteratorBase<OtherValue> const& other) const {
			return this->m_param_ptr == other.m_param_ptr;
		}
		
		/// returns a reference to the parameter node
		NodeType& dereference(void) const {
			return *m_param_ptr;
		}
		
		/// bypass security for the boost iterator core access class
		friend class boost::iterator_core_access;
		
		/// bypass security for IteratorBase with other Node Types
		/// NOTE: MSVC doesn't seem to work with friend templates
		friend class IteratorBase<const ParameterNode>;
		friend class IteratorBase<ParameterNode>;

		/// pointer to an Event parameter node
		NodeType *		m_param_ptr;
	};


public:

	/// used to identify the type of Event (TermRef maps to Terms of type OBJECT)
	typedef Vocabulary::TermRef				EventType;
	
	/// data type used for storing strings & BLOBs as Event parameter values
	typedef PionBlob<CharType,AllocType>	BlobType;

	/// data type used for parameters that construct BLOBs
	typedef typename BlobType::BlobParams	BlobParams;
	
	/// data type that holds the values for Event parameters.
	/// a variant is used to avoid unnecessary heap allocations;
	/// it works similar to a union in that the allocation of a
	/// ParameterValue reserves enough space for any of the
	/// possible values assigned to it
	typedef boost::variant<std::int32_t, std::uint32_t,
		std::int64_t, std::uint64_t, float, double, long double,
		PionDateTime, BlobType>				ParameterValue;
	
	/// data type used to contain the information for a single Event parameter.
	/// it is made public only for use with iterators.  note that only the
	/// "term_ref" and "value" members should only be used; all other members
	/// are for internal use by the Event class ("intrusive" pointers are
	/// used here to reduce and also have more control over memory operations)
	struct ParameterNode {
		/// default constructor
		ParameterNode(void) {}
		
		/// constructs ParameterNode initialized with a value
		template <typename T>
		ParameterNode(const Vocabulary::TermRef& tr, const T& v) :
			term_ref(tr), value(v)
		{}

		/// pointer to parent node (used by rbtree algorithms)
		ParameterNode *			m_parent_ptr;
		/// pointer to left node (used by rbtree algorithms)
		ParameterNode *			m_left_ptr;
		/// pointer to right node (used by rbtree algorithms)
		ParameterNode *			m_right_ptr;
		/// color value for this node (used by rbtree algorithms)
		std::uint8_t			m_tree_color;

		/// Term reference id for the parameter
		Vocabulary::TermRef		term_ref;

		/// value for the parameter
		ParameterValue			value;
	};
	
	/// data type used to iterate mutable Event parameters
	typedef IteratorBase<ParameterNode>			Iterator;

	/// data type used to iterate const Event parameters
	typedef IteratorBase<ParameterNode const>	ConstIterator;
	
	/// data type for a range of values assigned to a Vocabulary Term
	typedef std::pair<ConstIterator, ConstIterator>
												ValuesRange;
	
	/**
	 * constructs a new BasicEvent object
	 *
	 * @param type the type of Event that is being created
	 * @param alloc_ptr pointer to the Event's allocator
	 */
	BasicEvent(const EventType t, AllocType *alloc_ptr)
		: m_event_type(t), m_alloc_ptr(alloc_ptr), m_references(0)
	{
		// create an empty parameter tree
		tree_algo::init_header(&m_param_tree);
	}
	
	/// non-virtual destructor: this class should not be extended
	~BasicEvent() { clear(); }
	
	/// returns the type of Event that this is
	inline EventType getType(void) const { return m_event_type; }

	/// returns true if the Event has no parameters defined
	inline bool empty(void) const {
		return (tree_algo::begin_node(&m_param_tree) == &m_param_tree);
	}
	
	/// returns an iterator for the first Event parameter defined
	inline ConstIterator begin(void) const {
		return ConstIterator(tree_algo::begin_node(&m_param_tree));
	}
	
	/// returns an const iterator representing the end of Event parameters
	inline ConstIterator end(void) const {
		return ConstIterator(tree_algo::end_node(&m_param_tree));
	}

	/// clear all data contained within the Event
	inline void clear(void) {
		tree_algo::clear_and_dispose(&m_param_tree,
			std::bind(&BasicEvent<CharType,AllocType>::destroyParameter, this, std::placeholders::_1));
	}

	/**
	 * clear all parameters defined for a particular Term
	 * 
	 * @param term_ref numeric identifier for the term
	 */
	inline void clear(const Vocabulary::TermRef& term_ref) {
		ParameterNode *node_ptr;
		std::pair<ParameterNode*, ParameterNode*> range =
			tree_algo::equal_range(&m_param_tree, term_ref, m_key_compare);
		while (range.first != range.second) {
			node_ptr = range.first;
			range.first = tree_algo::next_node(range.first);
			tree_algo::erase(&m_param_tree, node_ptr);
			destroyParameter(node_ptr);
		}
	}

	/**
	 * returns an iterator pointing to a parameter for a given Term
	 *
	 * @param term_ref numeric identifier for the term
	 * @return Iterator references the given Term
	 */
	inline ConstIterator find(const Vocabulary::TermRef& term_ref) const {
		return ConstIterator(tree_algo::find(&m_param_tree, term_ref, m_key_compare));
	}
	
	/**
	 * returns a range of Event parameters defined for a Term
	 * NOTE: if none found, first does not necessarily equal end() -> check first == second!
	 *
	 * @param term_ref numeric identifier for the term
	 * @return std::pair<Iterator,Iterator> range of parameter iterators for the term
	 */
	inline ValuesRange equal_range(const Vocabulary::TermRef& term_ref) const
	{
		std::pair<ParameterNode*, ParameterNode*> range =
			tree_algo::equal_range(&m_param_tree, term_ref, m_key_compare);
		return std::make_pair(Iterator(range.first), Iterator(range.second));
	}

	/**
	 * triggers function object for each parameter in the Event
	 *
	 * @param f callback function triggered for each parameter
	 */
	inline void for_each(std::function<void(Vocabulary::TermRef, const ParameterValue&)> f) const {
		for (ParameterNode *node_ptr = tree_algo::begin_node(&m_param_tree);
			node_ptr != tree_algo::end_node(&m_param_tree);
			node_ptr = tree_algo::next_node(node_ptr))
		{
			f(node_ptr->term_ref, node_ptr->value);
		}
	}

	/**
	 * adds all the terms from another Event into this one
	 *
	 * @param e the event to copy terms from
	 * @return BasicEvent& reference to this event
	 */
	inline BasicEvent& operator+=(const BasicEvent& e) {
		ParameterNode *new_param_ptr;
		for (ParameterNode *node_ptr = tree_algo::begin_node(&e.m_param_tree);
			node_ptr != tree_algo::end_node(&e.m_param_tree);
			node_ptr = tree_algo::next_node(node_ptr))
		{
			new_param_ptr = createParameter(node_ptr->term_ref, node_ptr->value);
			tree_algo::insert_equal_upper_bound(&m_param_tree,
				new_param_ptr, m_item_compare);
		}
		return *this;
	}

	/**
	 * copies all values for a particular term from one event into this one
	 *
	 * @param e the event to copy terms from
	 * @param term_ref numeric identifier for the term to copy
	 */
	inline void copyValues(const BasicEvent& e, const Vocabulary::TermRef& term_ref)
	{
		std::pair<ParameterNode*, ParameterNode*> range =
			tree_algo::equal_range(&e.m_param_tree, term_ref, e.m_key_compare);
		ParameterNode *new_param_ptr;
		while (range.first != range.second) {
			new_param_ptr = createParameter(range.first->term_ref, range.first->value);
			tree_algo::insert_equal_upper_bound(&m_param_tree,
				new_param_ptr, m_item_compare);
			range.first = tree_algo::next_node(range.first);
		}
	}
	
	/**
	 * returns the value for a particular term, or null if it does not exist
	 * 
	 * @param term_ref numeric identifier for the term
	 * @return const ParameterValue* pointer to the value currently assigned to
	 *                               the term (null if not found)
	 */
	inline const ParameterValue *getPointer(const Vocabulary::TermRef& term_ref) const {
		const ParameterNode *node_ptr = tree_algo::find(&m_param_tree, term_ref, m_key_compare);
		return (node_ptr==&m_param_tree ? NULL : &(node_ptr->value));
	}
	
	/**
	 * returns true if a Term has at least one definition
	 * 
	 * @param term_ref numeric identifier for the term
	 * @return true if the Term has at least one definition
	 */
	inline bool isDefined(const Vocabulary::TermRef& term_ref) const {
		return (tree_algo::find(&m_param_tree, term_ref, m_key_compare) != &m_param_tree);
	}
		
	/**
	 * shorthand for retrieving the (const) value of an integer field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @return const std::int32_t& the value of the field
	 */
	inline const std::int32_t& getInt(const Vocabulary::TermRef& term_ref) const {
		PION_ASSERT(getPointer(term_ref)!=NULL && boost::get<std::int32_t>(getPointer(term_ref)));
		return boost::get<const std::int32_t&>(*getPointer(term_ref));
	}
	
	/**
	 * shorthand for retrieving the (const) value of an unsigned integer field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @return const std::uint32_t& the value of the field
	 */
	inline const std::uint32_t& getUInt(const Vocabulary::TermRef& term_ref) const {
		PION_ASSERT(getPointer(term_ref)!=NULL && boost::get<std::uint32_t>(getPointer(term_ref)));
		return boost::get<const std::uint32_t&>(*getPointer(term_ref));
	}
	
	/**
	 * shorthand for retrieving the (const) value of a big integer field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @return const std::int64_t& the value of the field
	 */
	inline const std::int64_t& getBigInt(const Vocabulary::TermRef& term_ref) const {
		PION_ASSERT(getPointer(term_ref)!=NULL && boost::get<std::int64_t>(getPointer(term_ref)));
		return boost::get<const std::int64_t&>(*getPointer(term_ref));
	}
	
	/**
	 * shorthand for retrieving the (const) value of an unsigned big integer field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @return const std::uint64_t& the value of the field
	 */
	inline const std::uint64_t& getUBigInt(const Vocabulary::TermRef& term_ref) const {
		PION_ASSERT(getPointer(term_ref)!=NULL && boost::get<std::uint64_t>(getPointer(term_ref)));
		return boost::get<const std::uint64_t&>(*getPointer(term_ref));
	}
	
	/**
	 * shorthand for retrieving the (const) value of a floating point number field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @return const float& the value of the field
	 */
	inline const float& getFloat(const Vocabulary::TermRef& term_ref) const {
		PION_ASSERT(getPointer(term_ref)!=NULL && boost::get<float>(getPointer(term_ref)));
		return boost::get<const float&>(*getPointer(term_ref));
	}

	/**
	 * shorthand for retrieving the (const) value of a double floating point number field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @return const double& the value of the field
	 */
	inline const double& getDouble(const Vocabulary::TermRef& term_ref) const {
		PION_ASSERT(getPointer(term_ref)!=NULL && boost::get<double>(getPointer(term_ref)));
		return boost::get<const double&>(*getPointer(term_ref));
	}
	
	/**
	 * shorthand for retrieving the (const) value of a long double floating point number field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @return const long double& the value of the field
	 */
	inline const long double& getLongDouble(const Vocabulary::TermRef& term_ref) const {
		PION_ASSERT(getPointer(term_ref)!=NULL && boost::get<long double>(getPointer(term_ref)));
		return boost::get<const long double&>(*getPointer(term_ref));
	}
	
	/**
	 * shorthand for retrieving the (const) value date_time field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @return const PionDateTime& the value of the field
	 */
	inline const PionDateTime& getDateTime(const Vocabulary::TermRef& term_ref) const {
		PION_ASSERT(getPointer(term_ref)!=NULL && boost::get<const PionDateTime&>(getPointer(term_ref)));
		return boost::get<const PionDateTime&>(*getPointer(term_ref));
	}
	
	/**
	 * shorthand for retrieving the (const) value of a string field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @return const CharType * the value of the field
	 */
	inline const CharType *getString(const Vocabulary::TermRef& term_ref) const {
		PION_ASSERT(getPointer(term_ref)!=NULL && boost::get<const BlobType&>(getPointer(term_ref)));
		return boost::get<const BlobType&>(*getPointer(term_ref)).get();
	}
	
	/**
	 * shorthand for retrieving the (const) BlobType value of field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @return const BlobType& the value of the field
	 */
	inline const BlobType& getBlob(const Vocabulary::TermRef& term_ref) const {
		PION_ASSERT(getPointer(term_ref)!=NULL && boost::get<const BlobType&>(getPointer(term_ref)));
		return boost::get<const BlobType&>(*getPointer(term_ref));
	}
	
	/**
	 * shorthand for retrieving the (const) value of an integer field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @param v will be set to the value of the term if it is defined
	 *
	 * @return true if the term is defined and v was set to its value, or false if not
	 */
	template <typename T>
	inline bool getInt(const Vocabulary::TermRef& term_ref, T& v) const {
		const ParameterValue *param_ptr = getPointer(term_ref);
		if (param_ptr) {
			v = boost::get<const std::int32_t&>(*param_ptr);
			return true;
		}
		return false;
	}
	
	/**
	 * shorthand for retrieving the (const) value of an unsigned integer field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @param v will be set to the value of the term if it is defined
	 *
	 * @return true if the term is defined and v was set to its value, or false if not
	 */
	template <typename T>
	inline bool getUInt(const Vocabulary::TermRef& term_ref, T& v) const {
		const ParameterValue *param_ptr = getPointer(term_ref);
		if (param_ptr) {
			v = boost::get<const std::uint32_t&>(*param_ptr);
			return true;
		}
		return false;
	}
	
	/**
	 * shorthand for retrieving the (const) value of a big integer field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @param v will be set to the value of the term if it is defined
	 *
	 * @return true if the term is defined and v was set to its value, or false if not
	 */
	template <typename T>
	inline bool getBigInt(const Vocabulary::TermRef& term_ref, T& v) const {
		const ParameterValue *param_ptr = getPointer(term_ref);
		if (param_ptr) {
			v = boost::get<const std::int64_t&>(*param_ptr);
			return true;
		}
		return false;
	}
	
	/**
	 * shorthand for retrieving the (const) value of an unsigned big integer field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @param v will be set to the value of the term if it is defined
	 *
	 * @return true if the term is defined and v was set to its value, or false if not
	 */
	template <typename T>
	inline bool getUBigInt(const Vocabulary::TermRef& term_ref, T& v) const {
		const ParameterValue *param_ptr = getPointer(term_ref);
		if (param_ptr) {
			v = boost::get<const std::uint64_t&>(*param_ptr);
			return true;
		}
		return false;
	}
	
	/**
	 * shorthand for retrieving the (const) value of a floating point number field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @param v will be set to the value of the term if it is defined
	 *
	 * @return true if the term is defined and v was set to its value, or false if not
	 */
	template <typename T>
	inline bool getFloat(const Vocabulary::TermRef& term_ref, T& v) const {
		const ParameterValue *param_ptr = getPointer(term_ref);
		if (param_ptr) {
			v = boost::get<const float&>(*param_ptr);
			return true;
		}
		return false;
	}

	/**
	 * shorthand for retrieving the (const) value of a double floating point number field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @param v will be set to the value of the term if it is defined
	 *
	 * @return true if the term is defined and v was set to its value, or false if not
	 */
	template <typename T>
	inline bool getDouble(const Vocabulary::TermRef& term_ref, T& v) const {
		const ParameterValue *param_ptr = getPointer(term_ref);
		if (param_ptr) {
			v = boost::get<const double&>(*param_ptr);
			return true;
		}
		return false;
	}
	
	/**
	 * shorthand for retrieving the (const) value of a long double floating point number field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @param v will be set to the value of the term if it is defined
	 *
	 * @return true if the term is defined and v was set to its value, or false if not
	 */
	template <typename T>
	inline bool getLongDouble(const Vocabulary::TermRef& term_ref, T& v) const {
		const ParameterValue *param_ptr = getPointer(term_ref);
		if (param_ptr) {
			v = boost::get<const long double&>(*param_ptr);
			return true;
		}
		return false;
	}
	
	/**
	 * shorthand for retrieving the (const) value date_time field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @param v will be set to the value of the term if it is defined
	 *
	 * @return true if the term is defined and v was set to its value, or false if not
	 */
	template <typename T>
	inline bool getDateTime(const Vocabulary::TermRef& term_ref, T& v) const {
		const ParameterValue *param_ptr = getPointer(term_ref);
		if (param_ptr) {
			v = boost::get<const PionDateTime&>(*param_ptr);
			return true;
		}
		return false;
	}
	
	/**
	 * shorthand for retrieving the (const) BlobType value of field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @param v will be set to the value of the term if it is defined
	 *
	 * @return true if the term is defined and v was set to its value, or false if not
	 */
	template <typename T>
	inline bool getBlob(const Vocabulary::TermRef& term_ref, T& v) const {
		const ParameterValue *param_ptr = getPointer(term_ref);
		if (param_ptr) {
			v = boost::get<const BlobType&>(*param_ptr);
			return true;
		}
		return false;
	}

	/**
	 * shorthand for retrieving the (const) BlobType value of field (specialization for std::string)
	 * 
	 * @param term_ref numeric identifier for the term
	 * @param str will be set to the value of the term if it is defined
	 *
	 * @return true if the term is defined and str was set to its value, or false if not
	 */
	inline bool getBlob(const Vocabulary::TermRef& term_ref, std::string& str) const {
		const ParameterValue *param_ptr = getPointer(term_ref);
		if (param_ptr) {
			str = boost::get<const BlobType&>(*param_ptr).get();
			return true;
		}
		return false;
	}

	/**
	 * shorthand for retrieving the (const) value of a string field
	 * 
	 * @param term_ref numeric identifier for the term
	 * @param v will be set to the value of the term if it is defined
	 *
	 * @return true if the term is defined and v was set to its value, or false if not
	 */
	template <typename T>
	inline bool getString(const Vocabulary::TermRef& term_ref, T& v) const {
		return getBlob(term_ref, v);
	}

	/**
	 * shorthand for retrieving the (const) value of a string field (specialization for std::string)
	 * 
	 * @param term_ref numeric identifier for the term
	 * @param str will be set to the value of the term if it is defined
	 *
	 * @return true if the term is defined and str was set to its value, or false if not
	 */
	inline bool getString(const Vocabulary::TermRef& term_ref, std::string& str) const {
		return getBlob(term_ref, str);
	}

	/**
	 * sets the value for a particular term to an integer
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	template <typename T>
	inline void setInt(const Vocabulary::TermRef& term_ref, T value) {
		insert(term_ref, std::int32_t(value));
	}

	/**
	 * sets the value for a particular term to an integer
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setInt(const Vocabulary::TermRef& term_ref, const std::string& value) {
		insert(term_ref, boost::lexical_cast<std::int32_t>(value));
	}

	/**
	 * sets the value for a particular term to an unsigned integer
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	template <typename T>
	inline void setUInt(const Vocabulary::TermRef& term_ref, T value) {
		insert(term_ref, std::uint32_t(value));
	}
	
	/**
	 * sets the value for a particular term to an unsigned integer
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setUInt(const Vocabulary::TermRef& term_ref, const std::string& value) {
		insert(term_ref, boost::lexical_cast<std::uint32_t>(value));
	}
	
	/**
	 * sets the value for a particular term to a big integer
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	template <typename T>
	inline void setBigInt(const Vocabulary::TermRef& term_ref, T value) {
		insert(term_ref, std::int64_t(value));
	}
	
	/**
	 * sets the value for a particular term to a big integer
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setBigInt(const Vocabulary::TermRef& term_ref, const std::string& value) {
		insert(term_ref, boost::lexical_cast<std::int64_t>(value));
	}
	
	/**
	 * sets the value for a particular term to an unsigned big integer
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	template <typename T>
	inline void setUBigInt(const Vocabulary::TermRef& term_ref, T value) {
		insert(term_ref, std::uint64_t(value));
	}
	
	/**
	 * sets the value for a particular term to an unsigned big integer
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setUBigInt(const Vocabulary::TermRef& term_ref, const std::string& value) {
		insert(term_ref, boost::lexical_cast<std::uint64_t>(value));
	}
	
	/**
	 * sets the value for a particular term to a floating point number
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setFloat(const Vocabulary::TermRef& term_ref, const float value) {
		insert(term_ref, value);
	}
	
	/**
	 * sets the value for a particular term to a floating point number
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setFloat(const Vocabulary::TermRef& term_ref, const std::string& value) {
		insert(term_ref, boost::lexical_cast<float>(value));
	}
	
	/**
	 * sets the value for a particular term to a double floating point number
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setDouble(const Vocabulary::TermRef& term_ref, const double value) {
		insert(term_ref, value);
	}

	/**
	 * sets the value for a particular term to a double floating point number
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setDouble(const Vocabulary::TermRef& term_ref, const std::string& value) {
		insert(term_ref, boost::lexical_cast<double>(value));
	}

	/**
	 * sets the value for a particular term to a long double floating point number
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setLongDouble(const Vocabulary::TermRef& term_ref, const long double value) {
		insert(term_ref, value);
	}
	
	/**
	 * sets the value for a particular term to a long double floating point number
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setLongDouble(const Vocabulary::TermRef& term_ref, const std::string& value) {
		insert(term_ref, boost::lexical_cast<long double>(value));
	}
	
	/**
	 * sets the value for a particular term to a date_time value
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setDateTime(const Vocabulary::TermRef& term_ref, const PionDateTime& value) {
		insert(term_ref, value);
	}
	
	/**
	 * sets the value for a particular term to a string using a character array
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 * @param len length of the string object, in bytes
	 */
	inline void setBlob(const Vocabulary::TermRef& term_ref,
						  const CharType *value, std::size_t len)
	{
		insert(term_ref, make_blob(value, len));
	}
	
	/**
	 * sets the value for a particular term to a string using a character array
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setBlob(const Vocabulary::TermRef& term_ref, const CharType *value) {
		insert(term_ref, make_blob(value, strlen(value)));
	}
	
	/**
	 * sets the value for a particular term to a string using a std::string
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setBlob(const Vocabulary::TermRef& term_ref, const std::string& value) {
		insert(term_ref, make_blob(value));
	}
	
	/**
	 * sets the value for a particular term to an existing BlobType value
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setBlob(const Vocabulary::TermRef& term_ref, const BlobType& value) {
		insert(term_ref, value);
	}
	
	/**
	 * sets the value for a particular term to a string using a character array (ensures UTF8 encoding)
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 * @param len length of the string object, in bytes
	 */
	inline void setString(const Vocabulary::TermRef& term_ref,
						  const CharType *value, std::size_t len)
	{
		try {
			insert(term_ref, make_utf8_blob(value, len));
		} catch (std::exception& e) {
			PION_LOG_ERROR(EventMessageLogger::get(), "term_ref: " << term_ref << " - " << e.what() << " - rethrowing");
			throw PionExceptionWithTermRef(e.what(), term_ref);
		}
	}
	
	/**
	 * sets the value for a particular term to a string using a character array (ensures UTF8 encoding)
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setString(const Vocabulary::TermRef& term_ref, const CharType *value) {
		try {
			insert(term_ref, make_utf8_blob(value));
		} catch (std::exception& e) {
			PION_LOG_ERROR(EventMessageLogger::get(), "term_ref: " << term_ref << " - " << e.what() << " - rethrowing");
			throw PionExceptionWithTermRef(e.what(), term_ref);
		}
	}
	
	/**
	 * sets the value for a particular term to a string using a std::string (ensures UTF8 encoding)
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setString(const Vocabulary::TermRef& term_ref, const std::string& value) {
		try {
			insert(term_ref, make_utf8_blob(value));
		} catch (std::exception& e) {
			PION_LOG_ERROR(EventMessageLogger::get(), "term_ref: " << term_ref << " - " << e.what() << " - rethrowing");
			throw PionExceptionWithTermRef(e.what(), term_ref);
		}
	}
	
	/**
	 * sets the value for a particular term to an existing BlobType value (ensures UTF8 encoding)
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	inline void setString(const Vocabulary::TermRef& term_ref, const BlobType& value) {
		try {
			insert(term_ref, make_utf8_blob(value));
		} catch (std::exception& e) {
			PION_LOG_ERROR(EventMessageLogger::get(), "term_ref: " << term_ref << " - " << e.what() << " - rethrowing");
			throw PionExceptionWithTermRef(e.what(), term_ref);
		}
	}
	
	/**
	 * sets the value for a particular term (casts if necessary, may throw)
	 *
	 * @param term_ref numeric identifier for the term
	 * @param term_type data type for the term
	 * @param value new value assigned to the term
	 */
	inline void set(const Vocabulary::Term& t, const std::string& value)
	{
		switch (t.term_type) {
		case Vocabulary::TYPE_NULL:
		case Vocabulary::TYPE_OBJECT:
			// not serializable
			throw TermTypeNotSerializableException();
			break;
		case Vocabulary::TYPE_INT8:
		case Vocabulary::TYPE_INT16:
		case Vocabulary::TYPE_INT32:
			setInt(t.term_ref, value);
			break;
		case Vocabulary::TYPE_UINT8:
		case Vocabulary::TYPE_UINT16:
		case Vocabulary::TYPE_UINT32:
			setUInt(t.term_ref, value);
			break;
		case Vocabulary::TYPE_INT64:
			setBigInt(t.term_ref, value);
			break;
		case Vocabulary::TYPE_UINT64:
			setUBigInt(t.term_ref, value);
			break;
		case Vocabulary::TYPE_FLOAT:
			setFloat(t.term_ref, value);
			break;
		case Vocabulary::TYPE_DOUBLE:
			setDouble(t.term_ref, value);
			break;
		case Vocabulary::TYPE_LONG_DOUBLE:
			setLongDouble(t.term_ref, value);
			break;
		case Vocabulary::TYPE_DATE_TIME:
		case Vocabulary::TYPE_DATE:
		case Vocabulary::TYPE_TIME:
			{
			pion::PionTimeFacet f(t.term_format);
			const pion::PionDateTime pdt(f.fromString(value));
			setDateTime(t.term_ref, pdt);
			break;
			}
		case Vocabulary::TYPE_SHORT_STRING:
		case Vocabulary::TYPE_STRING:
		case Vocabulary::TYPE_LONG_STRING:
		case Vocabulary::TYPE_CHAR:
			setString(t.term_ref, value);
			break;
		case Vocabulary::TYPE_BLOB:
		case Vocabulary::TYPE_ZBLOB:
			setBlob(t.term_ref, value);
			break;
		}
	}
	
	/**
	 * writes a parameter value to a stream
	 *
	 * @param str the output stream to write to
	 * @param value the ParameterValue to write
	 * @param t the Vocabulary Term associated with the ParameterValue
	 */
	static inline bool write(std::ostream& str, const ParameterValue& value,
		const Vocabulary::Term& t)
	{
		switch (t.term_type) {
		case Vocabulary::TYPE_NULL:
		case Vocabulary::TYPE_OBJECT:
			// not serializable
			throw TermTypeNotSerializableException();
			break;
		case Vocabulary::TYPE_INT8:
		case Vocabulary::TYPE_INT16:
		case Vocabulary::TYPE_INT32:
			str << boost::get<const std::int32_t&>(value);
			break;
		case Vocabulary::TYPE_UINT8:
		case Vocabulary::TYPE_UINT16:
		case Vocabulary::TYPE_UINT32:
			str << boost::get<const std::uint32_t&>(value);
			break;
		case Vocabulary::TYPE_INT64:
			str << boost::get<const std::int64_t&>(value);
			break;
		case Vocabulary::TYPE_UINT64:
			str << boost::get<const std::uint64_t&>(value);
			break;
		case Vocabulary::TYPE_FLOAT:
			str << boost::get<const float&>(value);
			break;
		case Vocabulary::TYPE_DOUBLE:
			str << boost::get<const double&>(value);
			break;
		case Vocabulary::TYPE_LONG_DOUBLE:
			str << boost::get<const long double&>(value);
			break;
		case Vocabulary::TYPE_SHORT_STRING:
		case Vocabulary::TYPE_STRING:
		case Vocabulary::TYPE_LONG_STRING:
		case Vocabulary::TYPE_CHAR:
		case Vocabulary::TYPE_BLOB:
		case Vocabulary::TYPE_ZBLOB:
			str << boost::get<const BlobType&>(value).get();
			break;
		case Vocabulary::TYPE_DATE_TIME:
		case Vocabulary::TYPE_DATE:
		case Vocabulary::TYPE_TIME:
			{
			pion::PionTimeFacet f(t.term_format);
			f.write(str, boost::get<const PionDateTime&>(value));
			break;
			}
		}
	}
	
	/**
	 * writes a parameter value to a string
	 *
	 * @param str the output string to write to
	 * @param value the ParameterValue to write
	 * @param t the Vocabulary Term associated with the ParameterValue
	 */
	static inline std::string& write(std::string& str, const ParameterValue& value,
		const Vocabulary::Term& t)
	{
		switch (t.term_type) {
		case Vocabulary::TYPE_NULL:
		case Vocabulary::TYPE_OBJECT:
			// not serializable
			throw TermTypeNotSerializableException();
			break;
		case Vocabulary::TYPE_INT8:
		case Vocabulary::TYPE_INT16:
		case Vocabulary::TYPE_INT32:
			str = boost::lexical_cast<std::string>( boost::get<const std::int32_t&>(value) );
			break;
		case Vocabulary::TYPE_UINT8:
		case Vocabulary::TYPE_UINT16:
		case Vocabulary::TYPE_UINT32:
			str = boost::lexical_cast<std::string>( boost::get<const std::uint32_t&>(value) );
			break;
		case Vocabulary::TYPE_INT64:
			str = boost::lexical_cast<std::string>( boost::get<const std::int64_t&>(value) );
			break;
		case Vocabulary::TYPE_UINT64:
			str = boost::lexical_cast<std::string>( boost::get<const std::uint64_t&>(value) );
			break;
		case Vocabulary::TYPE_FLOAT:
			str = boost::lexical_cast<std::string>( boost::get<const float&>(value) );
			break;
		case Vocabulary::TYPE_DOUBLE:
			str = boost::lexical_cast<std::string>( boost::get<const double&>(value) );
			break;
		case Vocabulary::TYPE_LONG_DOUBLE:
			str = boost::lexical_cast<std::string>( boost::get<const long double&>(value) );
			break;
		case Vocabulary::TYPE_SHORT_STRING:
		case Vocabulary::TYPE_STRING:
		case Vocabulary::TYPE_LONG_STRING:
		case Vocabulary::TYPE_CHAR:
		case Vocabulary::TYPE_BLOB:
		case Vocabulary::TYPE_ZBLOB:
			str = boost::get<const BlobType&>(value).get();
			break;
		case Vocabulary::TYPE_DATE_TIME:
		case Vocabulary::TYPE_DATE:
		case Vocabulary::TYPE_TIME:
			{
			pion::PionTimeFacet f(t.term_format);
			str = f.toString( boost::get<const PionDateTime&>(value) );
			break;
			}
		}
		return str;
	}
	
	/// can be used to construct a new BLOB object based upon an existing std::string
	inline BlobParams make_blob(const std::string& str) const {
		return BlobParams(*m_alloc_ptr, str.c_str(), str.size());
	}
	
	/// can be used to construct a new BLOB object based upon an existing memory buffer
	inline BlobParams make_blob(const CharType *ptr, const std::size_t len) const {
		return BlobParams(*m_alloc_ptr, ptr, len);
	}

	/// can be used to construct a new BLOB object based upon a c-style string
	inline BlobParams make_blob(const CharType *ptr) const {
		return BlobParams(*m_alloc_ptr, ptr, strlen(ptr));
	}

	/// can be used to construct a new UTF8 BLOB object based upon an existing std::string
	inline BlobType make_utf8_blob(const std::string& str) const {
		BlobType result;
		std::size_t trimmed_len;
		if (EventValidator::isValidUTF8(str.c_str(), str.size(), &trimmed_len)) {
			result.set(*m_alloc_ptr, str.c_str(), trimmed_len);
		} else {
			std::size_t buf_len = EventValidator::getCleansedUTF8Length(str.c_str(), str.size());
			char* bptr = result.reserve(*m_alloc_ptr, buf_len);
			EventValidator::cleanseUTF8(*m_alloc_ptr, str.c_str(), str.size(), bptr, &buf_len);
		}
		return result;
	}
	
	/// can be used to construct a new UTF8 BLOB object based upon an existing memory buffer
	inline BlobType make_utf8_blob(const CharType *ptr, const std::size_t len) const {
		BlobType result;
		std::size_t trimmed_len;
		if (EventValidator::isValidUTF8(ptr, len, &trimmed_len)) {
			result.set(*m_alloc_ptr, ptr, trimmed_len);
		} else {
			std::size_t buf_len = EventValidator::getCleansedUTF8Length(ptr, len);
			char* bptr = result.reserve(*m_alloc_ptr, buf_len);
			EventValidator::cleanseUTF8(*m_alloc_ptr, ptr, len, bptr, &buf_len);
		}
		return result;
	}

	/// can be used to construct a new UTF8 BLOB object based upon a c-style string
	inline BlobType make_utf8_blob(const CharType *ptr) const {
		BlobType result;
		const std::size_t len = strlen(ptr);
		std::size_t trimmed_len;
		if (EventValidator::isValidUTF8(ptr, len, &trimmed_len)) {
			result.set(*m_alloc_ptr, ptr, trimmed_len);
		} else {
			std::size_t buf_len = EventValidator::getCleansedUTF8Length(ptr, len);
			char* bptr = result.reserve(*m_alloc_ptr, buf_len);
			EventValidator::cleanseUTF8(*m_alloc_ptr, ptr, len, bptr, &buf_len);
		}
		return result;
	}

	/// can be used to construct a new UTF8 BLOB object based upon an existing BLOB
	inline BlobType make_utf8_blob(const BlobType& b) const {
		if (EventValidator::isValidUTF8(b.get(), b.size(), NULL)) {
			return b;
		} else {
			std::size_t buf_len = EventValidator::getCleansedUTF8Length(b.get(), b.size());
			BlobType result;
			char* bptr = result.reserve(*m_alloc_ptr, buf_len);
			EventValidator::cleanseUTF8(*m_alloc_ptr, b.get(), b.size(), bptr, &buf_len);
			return result;
		}
	}

	/// returns the number of references to this Event
	inline std::uint32_t getReferences(void) const { return m_references; }
	
	
protected:
	
	/// allow the EventPtr class to update references, etc.
	friend class EventPtr;
	
	/// allow the EventFactory class to access the Event's allocator
	friend class EventFactory;

	/// increments the reference count for this Event
	inline void addReference(void) { ++m_references; }
	
	/// decrements the reference count for this Event
	inline std::uint32_t removeReference(void) { return --m_references; }
	
	/// returns a pointer to the Event's allocator
	inline AllocType *getAllocator(void) { return m_alloc_ptr; }
	
	/**
	 * inserts a new parameter into the Event
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 */
	template <typename T>	
	inline void insert(const Vocabulary::TermRef& term_ref, const T& value) {
		PION_ASSERT(term_ref != Vocabulary::UNDEFINED_TERM_REF);
		ParameterNode *new_param_ptr = createParameter(term_ref, value);
		tree_algo::insert_equal_upper_bound(&m_param_tree,
			new_param_ptr, m_item_compare);
	}
	
	/**
	 * allocates a new parameter node object
	 *
	 * @param term_ref numeric identifier for the term
	 * @param value new value assigned to the term
	 *
	 * @return ParameterNode* pointer to a new parameter node object
	 */
	template <typename T>	
	inline ParameterNode *createParameter(const Vocabulary::TermRef& term_ref, const T& value) {
		void *mem_ptr = m_alloc_ptr->malloc(sizeof(ParameterNode));
		return new (mem_ptr) ParameterNode(term_ref, value);
	}
	
	/**
	 * deallocates a parameter node object
	 *
	 * @param param_ptr pointer to the parameter node object
	 */
	inline void destroyParameter(ParameterNode *param_ptr) {
		param_ptr->~ParameterNode();
		m_alloc_ptr->free(param_ptr, sizeof(ParameterNode));
	}
	

	/// function object used to compare two parameter items for ordering
	struct ParameterNodeCompare {
		/// returns true if a < b
		inline bool operator()(const ParameterNode *a, const ParameterNode *b) const {
			return (a->term_ref < b->term_ref);
		}
	};	
	
	/// function object used to compare two parameter items for ordering
	struct ParameterKeyCompare {
		/// returns true if the key for node a is less than key b
		inline bool operator()(const ParameterNode *a, const Vocabulary::TermRef& b) const {
			return (a->term_ref < b);
		}
		/// returns true if key a is less than the key for node b
		inline bool operator()(const Vocabulary::TermRef& a, const ParameterNode *b) const {
			return (a < b->term_ref);
		}
	};	
	
	/// traits class used by the boost.intrusive rbtree algorithms
	struct ParameterNodeTraits {
		typedef ParameterNode				node;
		typedef ParameterNode *				node_ptr;
		typedef const ParameterNode *		const_node_ptr;
		typedef std::uint8_t				color;
		static node_ptr get_parent(const_node_ptr n)       {  return n->m_parent_ptr;   }  
		static void set_parent(node_ptr n, node_ptr parent){  n->m_parent_ptr = parent; }  
		static node_ptr get_left(const_node_ptr n)         {  return n->m_left_ptr;     }  
		static void set_left(node_ptr n, node_ptr left)    {  n->m_left_ptr = left;     }  
		static node_ptr get_right(const_node_ptr n)        {  return n->m_right_ptr;    }  
		static void set_right(node_ptr n, node_ptr right)  {  n->m_right_ptr = right;   }  
		static color get_color(const_node_ptr n)           {  return n->m_tree_color;   }  
		static void set_color(node_ptr n, color c)         {  n->m_tree_color = c;      }
		static color black()                               {  return color(0);     }
		static color red()                                 {  return color(1);     }		
	};
	
	/// algorithms used to manipulate the parameter binary tree
	typedef boost::intrusive::rbtree_algorithms<ParameterNodeTraits>	tree_algo;

	
private:
	
	/// used to identify what type of Event this is (using Terms of type OBJECT)
	const EventType					m_event_type;
	
	/// event parameters: maps numeric term identifiers to values
	ParameterNode					m_param_tree;
	
	/// comparison function used for parameter items
	ParameterNodeCompare			m_item_compare;
	
	/// comparison function used for parameter keys (Term reference id's)
	ParameterKeyCompare				m_key_compare;
	
	/// pointer to the allocator used by this Event
	AllocType * const				m_alloc_ptr;
	
	/// the number of EventPtr references to this Event
	boost::detail::atomic_count		m_references;
};


/// default data type used for Pion Events
typedef BasicEvent<char, EventAllocator>	Event;


///
/// EventPtr: Event smart pointer objects generated by the EventFactory
///
class EventPtr {
public:
	
	/// non-virtual destructor
	~EventPtr() { reset(); }
	
	/// default constructor
	EventPtr(void) : m_event_ptr(NULL) {}

	/// copy constructor
	EventPtr(const EventPtr& ptr)
		: m_event_ptr(ptr.m_event_ptr)
	{
		if (m_event_ptr != NULL)
			m_event_ptr->addReference();
	}
	
	/// assignment operator
	inline EventPtr& operator=(const EventPtr& ptr) { reset(ptr); return *this; }
	
	/// returns reference to the Event
	inline Event& operator*() const { return *m_event_ptr; }
	
	/// returns pointer to the Event
	inline Event* operator->() const { return m_event_ptr; }

	/// returns pointer to the Event
	inline Event *get(void) const { return m_event_ptr; }
	
	/// returns true if the pointer contains an Event object with a single reference
	inline bool is_safe(void) const {
		return (m_event_ptr != NULL && m_event_ptr->getReferences()==1);
	}
	
	/// resets the pointer to be undefined
	inline void reset(void) {
		if (m_event_ptr != NULL) {
			if (m_event_ptr->removeReference() == 0) {
				EventAllocator *alloc_ptr = m_event_ptr->getAllocator();
				m_event_ptr->~Event();
				alloc_ptr->free(m_event_ptr, sizeof(Event));
			}
			m_event_ptr = NULL;
		}
	}
	
	/// resets the pointer to be equal to another one
	inline void reset(const EventPtr& ptr) {
		reset();
		m_event_ptr = ptr.m_event_ptr;
		if (m_event_ptr != NULL)
			m_event_ptr->addReference();
	}

	/// swaps the values of two pointers without changing reference counts
	inline void swap(EventPtr& ptr) {
		Event *tmp_event_ptr(ptr.m_event_ptr);
		ptr.m_event_ptr = m_event_ptr;
		m_event_ptr = tmp_event_ptr;
	}
	
	
protected:

	/// only EventFactory can construct new pointers that are not null
	friend class EventFactory;
	
	/// protected constructor: only allow EventFactory to create EventPtr objects
	EventPtr(Event *event_ptr)
		: m_event_ptr(event_ptr)
	{
		if (m_event_ptr != NULL)
			m_event_ptr->addReference();
	}
	
	
private:
	
	/// raw pointer to the Event object (NULL if uninitialized)
	Event *			m_event_ptr;
};
	
	
///
/// EventFactory: uses thread-specific Event allocators to create
///               and destroy dynamic EventPtr objects
///
class PION_PLATFORM_API EventFactory :
	private boost::noncopyable
{
public:

	/// non-virtual destructor
	~EventFactory() {}
	
	/// default constructor
	EventFactory(void)
		: m_event_alloc(EventAllocatorFactory::getAllocator())
	{}
	
	/// construct an EventFactory using an existing EventAllocator
	explicit EventFactory(EventAllocator& alloc)
		: m_event_alloc(alloc)
	{}

	/// construct an EventFactory using the EventAllocator of an existing Event
	explicit EventFactory(Event& e)
		: m_event_alloc(*e.getAllocator())
	{}
	
	/**
	 * creates and returns a new EventPtr
	 *
	 * @param type the type of Event to create
	 *
	 * @return EventPtr a reference-counting smart pointer that automatically
	 *                  destroys the Event when there are no more references
	 */
	inline EventPtr create(const Event::EventType t) {
		void *mem_ptr = m_event_alloc.malloc(sizeof(Event));
		return EventPtr(new (mem_ptr) Event(t, &m_event_alloc));
	}

	/**
	 * updates an EventPtr so that it contains an empty Event of type t
	 *
	 * @param ptr smart pointer to reset
	 * @param t the new type of Event
	 */
	inline void create(EventPtr& ptr, const Event::EventType t) {
		if (ptr.is_safe() && ptr->getType() == t)
			ptr->clear();
		else
			ptr = create(t);
	}
	
	/// returns the EventAllocator used by this factory
	inline EventAllocator& getAllocator(void) { return m_event_alloc; }
	
	/// can be used to construct a new BLOB object based upon an existing std::string
	inline Event::BlobParams make_blob(const std::string& str) const {
		return Event::BlobParams(m_event_alloc, str.c_str(), str.size());
	}
	
	/// can be used to construct a new BLOB object based upon an existing memory buffer
	inline Event::BlobParams make_blob(const char *ptr, const std::size_t len) const {
		return Event::BlobParams(m_event_alloc, ptr, len);
	}

	/// can be used to construct a new BLOB object based upon an existing c-style string
	inline Event::BlobParams make_blob(const char *ptr) const {
		return Event::BlobParams(m_event_alloc, ptr, strlen(ptr));
	}
	
	
private:
	
	///
	/// EventAllocatorFactory: used to create and manage thread-specific EventAllocators
	///
	class EventAllocatorFactory {
	public:
		
		/// non-virtual destructor
		~EventAllocatorFactory() {
#ifdef PION_EVENT_USE_POOL_ALLOCATORS
			// lock access to the Event allocators
			std::unique_lock<std::mutex> alloc_lock(m_alloc_mutex);
			// destruct all the EventAllocators that have been generated
			for (std::list<EventAllocator*>::iterator i = m_active_allocs.begin();
				 i != m_active_allocs.end(); ++i)
			{
				delete *i;
			}
			for (std::list<EventAllocator*>::iterator i = m_inactive_allocs.begin();
				 i != m_inactive_allocs.end(); ++i)
			{
				delete *i;
			}
			// release thread-specific storage for the last thread remaining,
			// the current thread, which is performing static global cleanup
			// (this is the only time this destructor would ever be called)
			m_thread_alloc.release();
#endif
		}
		
		/**
		 * return an EventAllocator to use for the current thread
		 * 
		 * @return EventAllocator& reference to the thread's EventAllocator
		 */
		inline static EventAllocator& getAllocator(void) {
			std::call_once(m_instance_flag, EventAllocatorFactory::createInstance);
			EventAllocator *alloc_ptr;
#ifdef PION_EVENT_USE_POOL_ALLOCATORS
			alloc_ptr = m_instance_ptr->m_thread_alloc.get();
			if (alloc_ptr == NULL) {
				// no allocator found for this thread
				std::unique_lock<std::mutex> alloc_lock(m_instance_ptr->m_alloc_mutex);
				if (m_instance_ptr->m_inactive_allocs.empty()) {
					// no inactive allocators we can reuse -> create a new one
					alloc_ptr = new EventAllocator();
				} else {
					// recycle an inactive allocator that already exists
					alloc_ptr = m_instance_ptr->m_inactive_allocs.front();
					m_instance_ptr->m_inactive_allocs.pop_front();
					// release unused memory
					alloc_ptr->release_memory();
				}
				// add the allocator to the active EventAllocator
				m_instance_ptr->m_active_allocs.push_back(alloc_ptr);
				// store the new thread-specific allocator
				m_instance_ptr->m_thread_alloc.reset(alloc_ptr);
			}
#else
			alloc_ptr = & m_instance_ptr->m_single_alloc;
#endif
			return *alloc_ptr;
		}

		
	private:
		
		/// private constructor for singleton pattern
		EventAllocatorFactory(void)
#ifdef PION_EVENT_USE_POOL_ALLOCATORS
			: m_thread_alloc(&EventAllocatorFactory::releaseAllocator)
#endif
		{}
		
		/// creates the singleton instance, protected by std::call_once
		static void createInstance(void);
		
		/// used by thread_specific_ptr to release allocators when threads exit
		static void releaseAllocator(EventAllocator *ptr) {
#ifdef PION_EVENT_USE_POOL_ALLOCATORS
			// do not free the EventAllocator since other threads may still need to dealloate Events!
			std::unique_lock<std::mutex> alloc_lock(m_instance_ptr->m_alloc_mutex);
			// remove allocator from active list
			for (std::list<EventAllocator*>::iterator i = m_instance_ptr->m_active_allocs.begin();
				 i != m_instance_ptr->m_active_allocs.end(); ++i)
			{
				if (ptr == *i) {
					m_instance_ptr->m_active_allocs.erase(i);
					break;
				}
			}
			// release unused memory
			ptr->release_memory();
			// add allocator to the inactive list
			m_instance_ptr->m_inactive_allocs.push_back(ptr);
#endif
		}
		
		
#ifdef PION_EVENT_USE_POOL_ALLOCATORS
		/// points to a thread-specific allocator used to create and destroy Events
		boost::thread_specific_ptr<EventAllocator>		m_thread_alloc;
		
		/// used to keep track of all of the active EventAllocators
		std::list<EventAllocator*>						m_active_allocs;
		
		/// used to keep track of all of the inactive EventAllocators
		std::list<EventAllocator*>						m_inactive_allocs;
		
		/// used to protect access to the EventAllocator containers
		std::mutex									m_alloc_mutex;
#else
		/// use a single EventAllocator instance if not using memory pools
		EventAllocator									m_single_alloc;
#endif
		
		/// points to the singleton instance after creation
		static EventAllocatorFactory *					m_instance_ptr;
		
		/// used for thread-safe singleton pattern
		static std::once_flag							m_instance_flag;
	};
	
	
	/// references a thread-specific allocator used to manage Event memory
	EventAllocator &		m_event_alloc;
};


///
/// EventContainer: naive (not very efficient) container for a collection of
///                 EventPtr objects
///
/// TODO: improve efficiency by eliminating unnecessary atomic counter
///       operations triggered whenever the vector is resized
///
typedef std::vector<EventPtr>	EventContainer;


}	// end namespace platform
}	// end namespace pion

#endif
