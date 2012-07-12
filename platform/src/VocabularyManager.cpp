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

#include <pion/platform/VocabularyManager.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


// static members of VocabularyManager

const std::string			VocabularyManager::DEFAULT_CONFIG_FILE = "vocabularies.xml";
const std::string			VocabularyManager::DEFAULT_VOCABULARY_PATH = ".";
const std::string			VocabularyManager::VOCABULARY_PATH_ELEMENT_NAME = "VocabularyPath";
const std::string			VocabularyManager::VOCABULARY_CONFIG_ELEMENT_NAME = "VocabularyConfig";
const std::string			VocabularyManager::VOCABULARIES_PERMISSION_TYPE = "Vocabularies";


// VocabularyManager member functions

VocabularyManager::VocabularyManager(void)
	: ConfigManager(DEFAULT_CONFIG_FILE),
	m_vocab_path(DEFAULT_VOCABULARY_PATH)
{
	setLogger(PION_GET_LOGGER("pion.platform.VocabularyManager"));
}

void VocabularyManager::createConfigFile(void)
{
	std::lock_guard<std::mutex> manager_lock(m_mutex);
	
	// just return if it's already open
	if (configIsOpen())
		return;
	
	// create the file with "config" root element
	ConfigManager::createConfigFile();
	
	PION_LOG_INFO(m_logger, "Initializing new global Vocabulary configuration file: " << m_config_file);

	// update the vocabulary path now that we know where the config file is located
	// this is just to resolve relative vocabulary paths
	m_vocab_path = ConfigManager::resolveRelativePath(getVocabularyPath());
	
	// add the current vocabulary path to the config file
	ConfigManager::updateConfigOption(VOCABULARY_PATH_ELEMENT_NAME,
									  getVocabularyPath(),
									  m_config_node_ptr);

	// save the new XML config file
	saveConfigFile();
}
	
void VocabularyManager::openConfigFile(void)
{
	std::unique_lock<std::mutex> manager_lock(m_mutex);
	
	// just return if it's already open
	if (configIsOpen())
		return;
	
	// open the file and find the "config" root element
	ConfigManager::openConfigFile();

	// get the vocabulary path
	if (! ConfigManager::getConfigOption(VOCABULARY_PATH_ELEMENT_NAME, m_vocab_path,
										 m_config_node_ptr->children))
		throw MissingVocabularyPathException(getConfigFile());
	
	// resolve relative vocabulary paths
	m_vocab_path = ConfigManager::resolveRelativePath(getVocabularyPath());
	
	// Step through and load Vocabulary config files
	xmlNodePtr config_node = m_config_node_ptr->children;
	while ( (config_node = ConfigManager::findConfigNodeByName(VOCABULARY_CONFIG_ELEMENT_NAME, config_node)) != NULL)
	{
		// get the unique identifier for the Vocabulary
		std::string vocab_id;
		if (! getNodeId(config_node, vocab_id))
			throw VocabularyConfig::EmptyVocabularyIdException(getConfigFile());

		// make sure it has not already been loaded
		if (m_vocab_map.find(vocab_id) != m_vocab_map.end())
			throw DuplicateVocabularyException(vocab_id);

		// get the name of the file that contains the Vocabulary's configuration
		xmlChar *xml_char_ptr = xmlNodeGetContent(config_node);
		if (xml_char_ptr == NULL || xml_char_ptr[0]=='\0') {
			if (xml_char_ptr != NULL)
				xmlFree(xml_char_ptr);
			throw EmptyVocabularyConfigException(getConfigFile());
		}
		const std::string vocab_file(reinterpret_cast<char*>(xml_char_ptr));
		xmlFree(xml_char_ptr);

		// open and parse the Vocabulary configuration file
		VocabularyConfigPtr new_config(new VocabularyConfig());
		new_config->setConfigFile(resolveRelativePath(vocab_file));
		new_config->openConfigFile();
		
		// add it to the vocabulary map and bind it
		m_vocab_map.insert(std::make_pair(vocab_id, new_config));
		new_config->bind(m_vocabulary);
		
		// step to the next Vocabulary configuration
		config_node = config_node->next;
	}
	
	// unlock class mutex to prevent deadlock
	manager_lock.unlock();

	// notify everyone that the vocabulary was updated
	PION_LOG_INFO(m_logger, "Loaded global Vocabulary configuration file: " << m_config_file);
	std::lock_guard<std::mutex> signal_lock(m_signal_mutex);
	m_signal_vocabulary_updated();
}
	
bool VocabularyManager::writeConfigXML(std::ostream& out,
									   const std::string& vocab_id) const
{
	std::lock_guard<std::mutex> manager_lock(m_mutex);
	
	// find the VocabularyConfig object
	VocabularyMap::const_iterator vocab_iterator = m_vocab_map.find(vocab_id);
	if (vocab_iterator == m_vocab_map.end())
		return false;
	
	// found it
	ConfigManager::writeConfigXMLHeader(out);
	vocab_iterator->second->writeConfigXML(out);
	return true;
}
	
bool VocabularyManager::writeTermConfigXML(std::ostream& out,
										   const std::string& term_id) const
{
	std::lock_guard<std::mutex> manager_lock(m_mutex);

	Vocabulary::TermRef term_ref = m_vocabulary.findTerm(term_id);
	if (term_ref == Vocabulary::UNDEFINED_TERM_REF)
		return false;

	ConfigManager::writeBeginPionConfigXML(out);
	VocabularyConfig::writeTermConfigXML(out, m_vocabulary[term_ref]);
	ConfigManager::writeEndPionConfigXML(out);

	return true;
}
	
void VocabularyManager::writeTermConfigXML(std::ostream& out) const
{
	ConfigManager::writeBeginPionConfigXML(out);
	std::lock_guard<std::mutex> manager_lock(m_mutex);
	for (Vocabulary::TermRef ref = 1; ref <= m_vocabulary.size(); ++ref) {
		if (m_vocabulary[ref].term_ref != Vocabulary::UNDEFINED_TERM_REF) {
			VocabularyConfig::writeTermConfigXML(out, m_vocabulary[ref]);
		}
	}
	ConfigManager::writeEndPionConfigXML(out);
}
	
void VocabularyManager::addVocabulary(const std::string& vocab_id,
									  const std::string& vocab_name,
									  const std::string& vocab_comment)
{
	std::unique_lock<std::mutex> manager_lock(m_mutex);

	// make sure that the Vocabulary configuration file is open
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());

	// make sure it has not already been loaded
	if (m_vocab_map.find(vocab_id) != m_vocab_map.end())
		throw DuplicateVocabularyException(vocab_id);

	// create a unique filename and open it up
	const std::string file_name(ConfigManager::createFilename(m_vocab_path));
	VocabularyConfigPtr new_config(new VocabularyConfig());
	new_config->setId(vocab_id);
	new_config->setName(vocab_name);
	new_config->setComment(vocab_comment);
	new_config->setConfigFile(file_name);
	new_config->createConfigFile();
	
	// add it to the vocabulary map and bind it
	m_vocab_map.insert(std::make_pair(vocab_id, new_config));
	new_config->bind(m_vocabulary);

	// add an entry for it to the config file
	xmlNodePtr new_node = xmlNewTextChild(m_config_node_ptr, NULL,
										  reinterpret_cast<const xmlChar*>(VOCABULARY_CONFIG_ELEMENT_NAME.c_str()),
										  reinterpret_cast<const xmlChar*>(file_name.c_str()));
	if (new_node == NULL)
		throw AddVocabularyConfigException(vocab_id);
	
	// set the id attribute for the VocabularyConfig element
	if (xmlNewProp(new_node, reinterpret_cast<const xmlChar*>(ID_ATTRIBUTE_NAME.c_str()),
				   reinterpret_cast<const xmlChar*>(vocab_id.c_str())) == NULL)
		throw AddVocabularyConfigException(vocab_id);
	
	// save the new XML config file
	saveConfigFile();
	
	// unlock class mutex to prevent deadlock
	manager_lock.unlock();
	
	// notify everyone that the vocabulary was updated
	PION_LOG_DEBUG(m_logger, "Added new Vocabulary: " << vocab_id);
	std::lock_guard<std::mutex> signal_lock(m_signal_mutex);
	m_signal_vocabulary_updated();
}

void VocabularyManager::addVocabulary(const std::string& vocab_id,
									  const char *content_buf,
									  std::size_t content_length)
{
	std::string vocab_name;
	std::string vocab_comment;

	// extract the XML config info from the content buffer
	xmlNodePtr config_ptr = ConfigManager::createResourceConfig(VocabularyConfig::getVocabularyElementName(),
																content_buf, content_length);
	if (config_ptr != NULL) {
		// get the "Name" value
		ConfigManager::getConfigOption(NAME_ELEMENT_NAME, vocab_name, config_ptr);
		// get the "Comment" value
		ConfigManager::getConfigOption(COMMENT_ELEMENT_NAME, vocab_comment, config_ptr);
		xmlFreeNodeList(config_ptr);
	}
	
	// call addVocabulary() to do the real work
	addVocabulary(vocab_id, vocab_name, vocab_comment);
}

void VocabularyManager::removeVocabulary(const std::string& vocab_id)
{
	std::unique_lock<std::mutex> manager_lock(m_mutex);
	
	// make sure that the Vocabulary configuration file is open
	if (! configIsOpen())
		throw ConfigNotOpenException(getConfigFile());
	
	// remove the Vocabulary from memory & remove the config file
	VocabularyMap::iterator vocab_iterator = m_vocab_map.find(vocab_id);
	if (vocab_iterator == m_vocab_map.end())
		throw VocabularyNotFoundException(vocab_id);
	vocab_iterator->second->removeConfigFile();
	m_vocab_map.erase(vocab_iterator);
	
	// remove it from the Vocabulary config file
	xmlNodePtr config_node = findConfigNodeByAttr(VOCABULARY_CONFIG_ELEMENT_NAME,
												  ID_ATTRIBUTE_NAME,
												  vocab_id,
												  m_config_node_ptr->children);
	if (config_node == NULL)
		throw RemoveVocabularyConfigException(vocab_id);
	xmlUnlinkNode(config_node);
	xmlFreeNode(config_node);
	
	// save the new XML config file
	saveConfigFile();

	// unlock class mutex to prevent deadlock
	manager_lock.unlock();
	
	// notify everyone that the vocabulary was updated
	PION_LOG_DEBUG(m_logger, "Removed Vocabulary: " << vocab_id);
	std::lock_guard<std::mutex> signal_lock(m_signal_mutex);
	m_signal_vocabulary_updated();
}
	
void VocabularyManager::setVocabularyPath(const std::string& vocab_path)
{
	std::lock_guard<std::mutex> manager_lock(m_mutex);

	// resolve relative vocabulary paths
	m_vocab_path = ConfigManager::resolveRelativePath(vocab_path);
	
	// make sure that the Vocabulary configuration file is open
	if (configIsOpen()) {
		// update the vocabulary path in the config file
		if (! updateConfigOption(VOCABULARY_PATH_ELEMENT_NAME, getVocabularyPath(), m_config_node_ptr))
			throw UpdateVocabularyPathException(getConfigFile());

		// save the new XML config file
		saveConfigFile();
	}
	
	PION_LOG_DEBUG(m_logger, "Updated Vocabulary config file path: " << vocab_path);
}


}	// end namespace platform
}	// end namespace pion
