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

#ifndef __PION_VOCABULARYCONFIG_HEADER__
#define __PION_VOCABULARYCONFIG_HEADER__

#include <functional>
#include <string>
#include <libxml/tree.h>
#include <boost/signal.hpp>
#include <pion/PionConfig.hpp>
#include <pion/PionException.hpp>
#include <pion/PionHashMap.hpp>
#include <pion/platform/Vocabulary.hpp>
#include <pion/platform/ConfigManager.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


///
/// VocabularyConfig: manages Vocabulary configuration & XML config files
///
class PION_PLATFORM_API VocabularyConfig :
	public ConfigManager
{
public:

	/// exception thrown if the config file does not contain a vocabulary element
	class VocabularyIsLockedException : public PionException {
	public:
		VocabularyIsLockedException(const std::string& vocab_id)
			: PionException("Unable to modify a locked Vocabulary: ", vocab_id) {}
	};

	/// exception thrown if the config file does not contain a vocabulary element
	class MissingVocabularyException : public PionException {
	public:
		MissingVocabularyException(const std::string& config_file)
			: PionException("Configuration file does not define a Vocabulary: ", config_file) {}
	};
	
	/// exception thrown if an error occurs when trying to update Vocabulary configuration options
	class UpdateVocabularyException : public PionException {
	public:
		UpdateVocabularyException(const std::string& config_file)
			: PionException("Unable to update Vocabulary configuration file: ", config_file) {}
	};
	
	/// exception thrown if the config file contains a vocabulary element with an empty or missing identifier
	class EmptyVocabularyIdException : public PionException {
	public:
		EmptyVocabularyIdException(const std::string& config_file)
			: PionException("Vocabulary configuration file does not define a unique identifier: ", config_file) {}
	};
	
	/// exception thrown if the config file contains an empty data type definition
	class EmptyTypeException : public PionException {
	public:
		EmptyTypeException(const std::string& term_id)
			: PionException("Vocabulary configuration has an empty data type for Term: ", term_id) {}
	};
	
	/// exception thrown if there is an error initializing a Vocabulary config file
	class InitializeConfigException : public PionException {
	public:
		InitializeConfigException(const std::string& file_name)
			: PionException("Unable to initialize Vocabulary configuration file: ", file_name) {}
	};
	
	/// exception thrown if there is an error adding a term element to the config file
	class AddTermConfigException : public PionException {
	public:
		AddTermConfigException(const std::string& term_id)
			: PionException("Unable to add a Term to the Vocabulary configuration file: ", term_id) {}
	};

	/// exception thrown if there is an error removing a Term from the config file
	class RemoveTermConfigException : public PionException {
	public:
		RemoveTermConfigException(const std::string& term_id)
			: PionException("Unable to remove a Term from the Vocabulary configuration file: ", term_id) {}
	};

	/// exception thrown if there is an error updating a Term in the config file
	class UpdateTermConfigException : public PionException {
	public:
		UpdateTermConfigException(const std::string& term_id)
			: PionException("Unable to update Term in the Vocabulary configuration file: ", term_id) {}
	};
	
	
	/// constructs a new VocabularyConfig instance
	VocabularyConfig(void);
	
	/// virtual destructor
	virtual ~VocabularyConfig() {}
	
	/// creates a new Vocabulary config file that includes the Pion "config" and "vocabulary" elements
	virtual void createConfigFile(void);
	
	/// opens an existing Vocabulary config file and loads the data it contains
	virtual void openConfigFile(void);
	
	/// sets the URI used to uniquely identify this Vocabulary
	void setId(const std::string& new_id);
	
	/// sets the descriptive name assigned to this Vocabulary
	void setName(const std::string& new_name);
	
	/// sets the comment that describes this Vocabulary
	void setComment(const std::string& new_comment);

	/// sets the locked boolean for this Vocabulary
	void setLocked(bool b);
	
	/// sets the general configuration for this Vocabulary
	void setConfig(const xmlNodePtr config_ptr);
	
	/**
	 * adds a new Term to the Vocabulary
	 *
	 * @param new_term the new Term to define
	 */
	void addTerm(const Vocabulary::Term& new_term);
	
	/**
	 * adds a new Term to the Vocabulary
	 *
	 * @param term_id unique identifier for the Term to add
	 * @param config_ptr pointer to a list of XML nodes containing term
	 *                   configuration parameters
	 */
	inline void addTerm(const std::string& term_id, const xmlNodePtr config_ptr) {
		Vocabulary::Term new_term(term_id);
		parseTermConfig(new_term, config_ptr);
		addTerm(new_term);
	}

	/**
	 * update the settings for a Term
	 *
	 * @param t the Term to update (t.term_id is used to find the Term to change)
	 */
	void updateTerm(const Vocabulary::Term& t);

	/**
	 * update the settings for a Term
	 *
	 * @param term_id unique identifier for the Term to modify
	 * @param config_ptr pointer to a list of XML nodes containing term
	 *                   configuration parameters
	 */
	inline void updateTerm(const std::string& term_id, const xmlNodePtr config_ptr) {
		Vocabulary::Term updated_term(term_id);
		parseTermConfig(updated_term, config_ptr);
		updateTerm(updated_term);
	}
	
	/**
	 * removes a Term from the Vocabulary (use with caution!!!)
	 *
	 * @param term_id unique identifier for the Term to remove
	 */
	void removeTerm(const std::string& term_id);
	
	/**
	 * binds a Vocabulary to this configuration manager and copies over terms
	 *
	 * @param v the Vocabulary object to bind
	 */
	inline void bind(Vocabulary& v) {
		m_signal_add_term.connect(std::bind(&Vocabulary::addTerm, &v, std::placeholders::_1));
		m_signal_update_term.connect(std::bind(&Vocabulary::updateTerm, &v, std::placeholders::_1));
		m_signal_remove_term.connect(std::bind(&Vocabulary::removeTerm, &v, std::placeholders::_1));
		v += m_vocabulary;
	}

	/// returns the URI used to uniquely identify this Vocabulary
	inline const std::string& getId(void) const { return m_vocabulary_id; }
	
	/// returns the descriptive name assigned to this Vocabulary
	inline const std::string& getName(void) const { return m_name; }
	
	/// returns the comment that describes this Vocabulary
	inline const std::string& getComment(void) const { return m_comment; }
	
	/// returns true if the Vocabulary is currently locked; false if it is not
	inline bool getLocked(void) const { return m_is_locked; }
	
	/// returns a reference to the local Vocabulary configuration
	inline const Vocabulary& getVocabulary(void) const { return m_vocabulary; }

	/// returns the vocabulary (root) XML element name
	static inline const std::string& getVocabularyElementName(void) {
		return VOCABULARY_ELEMENT_NAME;
	}
	
	/**
	 * uses a memory buffer to generate XML configuration data for a Vocabulary
	 *
	 * @param buf pointer to a memory buffer containing configuration data
	 * @param len number of bytes available in the memory buffer
	 *
	 * @return xmlNodePtr XML configuration list for the Vocabulary
	 */
	static xmlNodePtr createVocabularyConfig(const char *buf, std::size_t len) {
		return ConfigManager::createResourceConfig(VOCABULARY_ELEMENT_NAME, buf, len);
	}
	
	/**
	 * uses a memory buffer to generate XML configuration data for a Vocabulary Term
	 *
	 * @param buf pointer to a memory buffer containing configuration data
	 * @param len number of bytes available in the memory buffer
	 *
	 * @return xmlNodePtr XML configuration list for the Vocabulary Term
	 */
	static xmlNodePtr createTermConfig(const char *buf, std::size_t len) {
		return ConfigManager::createResourceConfig(TERM_ELEMENT_NAME, buf, len);
	}
	
	/**
	 * writes Term configuration info to an output stream (as XML)
	 *
	 * @param out the ostream to write the Term configuration info into
	 * @param t the term to use for configuration information
	 */
	static void writeTermConfigXML(std::ostream& out, const Vocabulary::Term& t);

	/**
	 * parses Term configuration information from an XML tree
	 *
	 * @param new_term this Term will be updated with the XML configuration info
	 * @param config_ptr pointer to a list of XML nodes containing term
	 *                   configuration parameters
	 */
	static void parseTermConfig(Vocabulary::Term& new_term,
								const xmlNodePtr config_ptr);
	
	
private:

	/**
	 * adds a new "type" element for a Term to the Vocabulary config file
	 *
	 * @param term_node pointer to the XML node for the Term
	 * @param t the term to add a type element for
	 *
	 * @return bool true if the element was added successfully; false if not
	 */
	bool addNewTermTypeConfig(xmlNodePtr term_node, const Vocabulary::Term& t);

	
	/// default name of the vocabulary config file
	static const std::string		DEFAULT_CONFIG_FILE;

	/// name of the vocabulary (root) element for Pion XML config files
	static const std::string		VOCABULARY_ELEMENT_NAME;

	/// name of the locked element for Pion XML config files
	static const std::string		LOCKED_ELEMENT_NAME;
	
	/// name of the Term element for Pion XML config files
	static const std::string		TERM_ELEMENT_NAME;
	
	/// name of the Data Type element for Pion XML config files
	static const std::string		TYPE_ELEMENT_NAME;

	/// name of the size (term_size) attribute for Pion XML config files
	static const std::string		SIZE_ATTRIBUTE_NAME;	
		
	/// name of the format (term_format) attribute for Pion XML config files
	static const std::string		FORMAT_ATTRIBUTE_NAME;	

	
	/// pointer to the vocabulary element node in the XML document tree
	xmlNodePtr 						m_vocabulary_node;
	
	/// local Vocabulary used to track the managed terms
	Vocabulary						m_vocabulary;
	
	/// the URI used to uniquely identify this Vocabulary
	std::string						m_vocabulary_id;
	
	/// the descriptive name assigned to this Vocabulary
	std::string						m_name;
	
	/// an option comment that describes this Vocabulary
	std::string						m_comment;
	
	/// true if this Vocabulary is currently locked
	bool							m_is_locked;
	
	/// signal triggered when a new Term is added
	boost::signal1<void,const Vocabulary::Term&>				m_signal_add_term;

	/// signal triggered when a Term is updated
	boost::signal1<void,const Vocabulary::Term&>				m_signal_update_term;

	/// signal triggered when a Term is removed
	boost::signal1<void,const std::string&>						m_signal_remove_term;
};


}	// end namespace platform
}	// end namespace pion

#endif
