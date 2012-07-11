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

#ifndef __PION_VOCABULARYMANAGER_HEADER__
#define __PION_VOCABULARYMANAGER_HEADER__

#include <string>
#include <boost/bind.hpp>
#include <boost/signal.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/PionHashMap.hpp>
#include <pion/platform/ConfigManager.hpp>
#include <pion/platform/VocabularyConfig.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


///
/// VocabularyManager: maps URI identifiers to VocabularyConfig objects
///
class PION_PLATFORM_API VocabularyManager
	: public ConfigManager
{
public:
	
	/// exception thrown if the config file contains an empty VocabularyConfig element
	class EmptyVocabularyConfigException : public PionException {
	public:
		EmptyVocabularyConfigException(const std::string& config_file)
			: PionException("Configuration file defines an empty Vocabulary config element: ", config_file) {}
	};

	/// exception thrown if the config file does not contain a VocabularyPath element
	class MissingVocabularyPathException : public PionException {
	public:
		MissingVocabularyPathException(const std::string& config_file)
			: PionException("Configuration file does not define a Vocabulary path: ", config_file) {}
	};

	/// exception thrown if you try loading a duplicate Vocabulary
	class DuplicateVocabularyException : public PionException {
	public:
		DuplicateVocabularyException(const std::string& vocab_id)
			: PionException("Vocabulary has already been loaded: ", vocab_id) {}
	};

	/// exception thrown if you try referencing an unknown Vocabulary
	class VocabularyNotFoundException : public PionException {
	public:
		VocabularyNotFoundException(const std::string& vocab_id)
			: PionException("Vocabulary has not been defined: ", vocab_id) {}
	};

	/// exception thrown if there is an error adding a Vocabulary to the config file
	class AddVocabularyConfigException : public PionException {
	public:
		AddVocabularyConfigException(const std::string& vocab_id)
			: PionException("Unable to add a Vocabulary to the configuration file: ", vocab_id) {}
	};

	/// exception thrown if there is an error removing a Vocabulary from the config file
	class RemoveVocabularyConfigException : public PionException {
	public:
		RemoveVocabularyConfigException(const std::string& vocab_id)
			: PionException("Unable to remove a Vocabulary from the configuration file: ", vocab_id) {}
	};
	
	/// exception thrown if there is an error updating the Vocabulary config file path
	class UpdateVocabularyPathException : public PionException {
	public:
		UpdateVocabularyPathException(const std::string& config_file)
			: PionException("Unable to update Vocabulary path in the configuration file: ", config_file) {}
	};

	
	/// constructs a new VocabularyManager instance
	VocabularyManager(void);
	
	/// virtual destructor
	virtual ~VocabularyManager() {}
	
	/// creates a new Vocabulary config file
	virtual void createConfigFile(void);
	
	/// opens an existing Vocabulary config file and loads the data it contains
	virtual void openConfigFile(void);
	
	/**
	 * writes the entire configuration tree to an output stream (as XML)
	 *
	 * @param out the ostream to write the configuration tree into
	 */
	virtual void writeConfigXML(std::ostream& out) const {
		boost::mutex::scoped_lock manager_lock(m_mutex);
		ConfigManager::writeConfigXMLHeader(out);
		ConfigManager::writeConfigXML(out, m_config_node_ptr, true);
	}

	/**
	 * writes the configuration data for a particular Vocabulary (as XML)
	 *
	 * @param out the ostream to write the configuration tree into
	 * @param vocab_id the unique identifier for the Vocabulary
	 *
	 * @return true if the Vocabulary was successfully found
	 */
	bool writeConfigXML(std::ostream& out, const std::string& vocab_id) const;

	/**
	 * writes Term configuration info to an output stream (as XML)
	 *
	 * @param out the ostream to write the Term configuration info into
	 * @param term_id include only the Term that matches this unique identifier,
	 *                or include all Terms if it is empty
	 *
	 * @return true if the Term was successfully found
	 */
	bool writeTermConfigXML(std::ostream& out, const std::string& term_id) const;
	
	/**
	 * writes configuration info for all Terms to an output stream (as XML)
	 *
	 * @param out the ostream to write the Term configuration info into
	 */
	void writeTermConfigXML(std::ostream& out) const;

	/**
	 * creates a new, empty vocabulary
	 *
	 * @param vocab_id the unique identifier for the Vocabulary to create
	 * @param vocab_name the descriptive name to assign to the Vocabulary
	 * @param vocab_comment the descriptive comment to assign to the Vocabulary
	 */
	void addVocabulary(const std::string& vocab_id,
					   const std::string& vocab_name,
					   const std::string& vocab_comment);
	
	/**
	 * creates a new, empty vocabulary
	 *
	 * @param vocab_id the unique identifier for the Vocabulary to create
	 * @param content_buf pointer to buffer containing XML config for the Vocabulary
	 * @param content_length size of the content buffer, in bytes
	 */
	void addVocabulary(const std::string& vocab_id, const char *content_buf,
					   std::size_t content_length);

	/**
	 * removes an existing vocabulary and deletes the associated config file
	 *
	 * @param vocab_id the unique identifier for the Vocabulary to remove
	 */
	void removeVocabulary(const std::string& vocab_id);
	
	/**
	 * sets the default path where new vocabulary config files will be created
	 *
	 * @param vocab_path the new path where config files will be created
	 */
	void setVocabularyPath(const std::string& vocab_path);
	
	/**
	 * sets configuration parameters for an existing vocabulary
	 *
	 * @param vocab_id the unique identifier for the Vocabulary to update
	 * @param config_ptr pointer to a list of XML nodes containing vocabulary
	 *                   configuration parameters
	 */
	void setVocabularyConfig(const std::string& vocab_id,
							 const xmlNodePtr config_ptr)
	{
		updateVocabulary(vocab_id, boost::bind(&VocabularyConfig::setConfig,
											   _1, config_ptr));
	}
	
	/**
	 * changes the descriptive name assigned to a Vocabulary
	 *
	 * @param vocab_id the unique identifier for the Vocabulary to modify
	 * @param new_name the new descriptive name to assign to the Vocabulary
	 */
	inline void setName(const std::string& vocab_id,
						const std::string& new_name)
	{
		updateVocabulary(vocab_id, boost::bind(&VocabularyConfig::setName,
											   _1, boost::cref(new_name)));
	}
	
	/**
	 * changes the comment that describes a Vocabulary
	 *
	 * @param vocab_id the unique identifier for the Vocabulary to modify
	 * @param new_comment the new comment to assign to the Vocabulary
	 */
	inline void setComment(const std::string& vocab_id,
						   const std::string& new_comment)
	{
		updateVocabulary(vocab_id, boost::bind(&VocabularyConfig::setComment,
											   _1, boost::cref(new_comment)));
	}
	
	/**
	 * changes the locked setting for a Vocabulary
	 *
	 * @param vocab_id the unique identifier for the Vocabulary to modify
	 * @param locked_setting the new value to assign to the locked setting
	 */
	inline void setLocked(const std::string& vocab_id, bool locked_setting)
	{
		updateVocabulary(vocab_id, boost::bind(&VocabularyConfig::setLocked,
											   _1, locked_setting));
	}
	
	/**
	 * adds a new Term to a Vocabulary
	 *
	 * @param vocab_id the unique identifier for the Vocabulary to modify
	 * @param new_term the new Term to define
	 */
	inline void addTerm(const std::string& vocab_id,
						const Vocabulary::Term& new_term)
	{
		updateVocabulary(vocab_id, boost::bind(&VocabularyConfig::addTerm,
											   _1, boost::cref(new_term)));
	}
	
	/**
	 * adds a new Term to a Vocabulary
	 *
	 * @param vocab_id the unique identifier for the Vocabulary to modify
	 * @param term_id the unique identifier for the Term to add
	 * @param config_ptr pointer to a list of XML nodes containing term
	 *                   configuration parameters
	 */
	inline void addTerm(const std::string& vocab_id,
						const std::string& term_id,
						const xmlNodePtr config_ptr)
	{
		updateVocabulary(vocab_id, boost::bind(&VocabularyConfig::addTerm,
											   _1, boost::cref(term_id), config_ptr));
	}
	
	/**
	 * update the settings for a Term
	 *
	 * @param vocab_id the unique identifier for the Vocabulary to modify
	 * @param t the Term to update (t.term_id is used to find the Term to change)
	 */
	inline void updateTerm(const std::string& vocab_id,
						   const Vocabulary::Term& t)
	{
		updateVocabulary(vocab_id, boost::bind(&VocabularyConfig::updateTerm,
											   _1, boost::cref(t)));
	}
	
	/**
	 * update the settings for a Term
	 *
	 * @param vocab_id the unique identifier for the Vocabulary to modify
	 * @param term_id the unique identifier for the Term to modify
	 * @param config_ptr pointer to a list of XML nodes containing term
	 *                   configuration parameters
	 */
	inline void updateTerm(const std::string& vocab_id,
						   const std::string& term_id,
						   const xmlNodePtr config_ptr)
	{
		updateVocabulary(vocab_id, boost::bind(&VocabularyConfig::updateTerm,
											   _1, boost::cref(term_id), config_ptr));
	}
	
	/**
	 * removes a Term from the Vocabulary (use with caution!!!)
	 *
	 * @param vocab_id the unique identifier for the Vocabulary to modify
	 * @param term_id unique identifier for the Term to remove
	 */
	inline void removeTerm(const std::string& vocab_id,
						   const std::string& term_id)
	{
		updateVocabulary(vocab_id, boost::bind(&VocabularyConfig::removeTerm,
											   _1, boost::cref(term_id)));
	}
	
	/**
	 * registers a callback function to be executed whenever a Vocabulary is updated
	 *
	 * @param f the callback function to register
	 * @return boost::signals::connection object representing the signal connection
	 */
	template <typename VocabularyUpdateFunction>
	inline boost::signals::connection registerForUpdates(VocabularyUpdateFunction f) const {
		boost::mutex::scoped_lock signal_lock(m_signal_mutex);
		return m_signal_vocabulary_updated.connect(f);
	}
	
	/**
	 * returns the descriptive name assigned to a Vocabulary
	 * 
	 * @param vocab_id the unique identifier for the Vocabulary to search for
	 * @return const std::string& the descriptive name assigned to the Vocabulary
	 */
	inline const std::string& getName(const std::string& vocab_id) const {
		boost::mutex::scoped_lock manager_lock(m_mutex);
		VocabularyMap::const_iterator i = m_vocab_map.find(vocab_id);
		if (i == m_vocab_map.end())
			throw VocabularyNotFoundException(vocab_id);
		return i->second->getName();
	}

	/**
	 * returns the descriptive comment assigned to a Vocabulary
	 * 
	 * @param vocab_id the unique identifier for the Vocabulary to search for
	 * @return const std::string& the comment assigned to the Vocabulary
	 */
	inline const std::string& getComment(const std::string& vocab_id) const {
		boost::mutex::scoped_lock manager_lock(m_mutex);
		VocabularyMap::const_iterator i = m_vocab_map.find(vocab_id);
		if (i == m_vocab_map.end())
			throw VocabularyNotFoundException(vocab_id);
		return i->second->getComment();
	}

	/// returns true if a particular Vocabulary is defined
	inline bool hasVocabulary(const std::string& vocab_id) const {
		boost::mutex::scoped_lock manager_lock(m_mutex);
		return (m_vocab_map.find(vocab_id) != m_vocab_map.end());
	}
	
	/// returns true if a particular Vocabulary Term is defined
	inline bool hasTerm(const std::string& term_id) const {
		boost::mutex::scoped_lock manager_lock(m_mutex);
		return (m_vocabulary.findTerm(term_id) != Vocabulary::UNDEFINED_TERM_REF);
	}
	
	/// returns a copy of the universal Vocabulary
	inline VocabularyPtr getVocabulary(void) const {
		boost::mutex::scoped_lock manager_lock(m_mutex);
		return VocabularyPtr(new Vocabulary(m_vocabulary));
	}

	/// returns the path where new vocabulary config files are created
	inline std::string getVocabularyPath(void) const { return m_vocab_path; }

	/// returns the type attribute used for an XML Permission node pertaining to Vocabularies
	std::string getPermissionType(void) const { return VOCABULARIES_PERMISSION_TYPE; }

private:

	/**
	 * updates a Vocabulary by calling an update function for the
	 * VocabularyConfig object.  The function should return void and take
	 * a Vocabulary* as its first and only parameter.
	 * 
	 * @param vocab_id the unique identifier for the Vocabulary to modify
	 * @param f the Vocabulary update function to run (after it is found)
	 */
	template <typename Function>
	inline void updateVocabulary(const std::string& vocab_id, Function f) {
		boost::mutex::scoped_lock manager_lock(m_mutex);
		VocabularyMap::iterator i = m_vocab_map.find(vocab_id);
		if (i == m_vocab_map.end())
			throw VocabularyNotFoundException(vocab_id);
		f(i->second);
		manager_lock.unlock();
		boost::mutex::scoped_lock signal_lock(m_signal_mutex);
		m_signal_vocabulary_updated();
	}
	
	
	/// data type for a pointer to a VocabularyConfig object
	typedef boost::shared_ptr<VocabularyConfig>		VocabularyConfigPtr;
	
	/// data type that maps Vocabulary identifiers to configuration pointers
	typedef std::unordered_map<std::string, VocabularyConfigPtr>	VocabularyMap;
	
	
	/// default name of the vocabulary config file
	static const std::string			DEFAULT_CONFIG_FILE;
	
	/// default path where new vocabulary config files are created
	static const std::string			DEFAULT_VOCABULARY_PATH;
	
	/// name of the vocabulary path element for Pion XML config files
	static const std::string			VOCABULARY_PATH_ELEMENT_NAME;

	/// name of the vocabulary config element for Pion XML config files
	static const std::string			VOCABULARY_CONFIG_ELEMENT_NAME;

	/// type identifier for Vocabularies permission type
	static const std::string			VOCABULARIES_PERMISSION_TYPE;

	/// the path where new vocabulary config files are created
	std::string							m_vocab_path;
	
	/// used to map Vocabulary identifiers to configuration pointers
	VocabularyMap						m_vocab_map;
	
	/// this includes a union of all Vocabularies that have been loaded
	Vocabulary							m_vocabulary;

	/// signal triggered whenever a Vocabulary is modified
	mutable boost::signal0<void>		m_signal_vocabulary_updated;

	/// mutex used to protect the updated signal handler
	mutable boost::mutex				m_signal_mutex;	
	
	/// mutex to make class thread-safe
	mutable boost::mutex				m_mutex;	
};


}	// end namespace platform
}	// end namespace pion

#endif
