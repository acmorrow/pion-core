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

#include <pion/platform/ConfigManager.hpp>
#include <pion/PionAdminRights.hpp>
#include <boost/filesystem/operations.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


// static members of ConfigManager

const std::string		ConfigManager::XML_FILE_EXTENSION = ".xml";
const std::string		ConfigManager::BACKUP_FILE_EXTENSION = ".bak";
const std::string		ConfigManager::CONFIG_NAMESPACE_URL = "http://purl.org/pion/config";
const std::string		ConfigManager::ROOT_ELEMENT_NAME = "PionConfig";
const std::string		ConfigManager::STATS_ELEMENT_NAME = "PionStats";
const std::string		ConfigManager::PLUGIN_ELEMENT_NAME = "Plugin";
const std::string		ConfigManager::NAME_ELEMENT_NAME = "Name";
const std::string		ConfigManager::COMMENT_ELEMENT_NAME = "Comment";
const std::string		ConfigManager::PION_VERSION_ATTRIBUTE_NAME = "pion_version";
const std::string		ConfigManager::ID_ATTRIBUTE_NAME = "id";

		
// ConfigManager member functions

void ConfigManager::createConfigFile(void)
{
	// make sure the config file is not already open
	if (m_config_doc_ptr != NULL)
		throw ConfigAlreadyOpenException(m_config_file);

	// make sure the config file does not already exist
	if (boost::filesystem::exists(m_config_file))
		throw ConfigFileExistsException(m_config_file);

	// create a new (empty) XML config tree
	if ((m_config_doc_ptr = xmlNewDoc(reinterpret_cast<const xmlChar*>("1.0"))) == NULL)
		throw InitializeRootConfigException(m_config_file);
	
	// add the root Pion "config" element
	if ((m_config_node_ptr = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>(ROOT_ELEMENT_NAME.c_str()))) == NULL)
		throw InitializeRootConfigException(m_config_file);
	xmlDocSetRootElement(m_config_doc_ptr, m_config_node_ptr);

	// add the namespace for the "config" element
	if (xmlNewProp(m_config_node_ptr,
				   reinterpret_cast<const xmlChar*>("xmlns"),
				   reinterpret_cast<const xmlChar*>(CONFIG_NAMESPACE_URL.c_str())) == NULL)
		throw InitializeRootConfigException(m_config_file);

	// add the version for the "config" element
	if (xmlNewProp(m_config_node_ptr,
				   reinterpret_cast<const xmlChar*>(PION_VERSION_ATTRIBUTE_NAME.c_str()),
				   reinterpret_cast<const xmlChar*>(PION_VERSION)) == NULL)
		throw InitializeRootConfigException(m_config_file);

	// save the new config file
	saveConfigFile();
}

void ConfigManager::openConfigFile(void)
{
	// make sure the config file is not already open
	if (m_config_doc_ptr != NULL)
		throw ConfigAlreadyOpenException(m_config_file);

	m_config_doc_ptr = getConfigFromFile(m_config_file, ROOT_ELEMENT_NAME, m_config_node_ptr, m_logger);
	if (m_config_doc_ptr == NULL)
		throw ConfigManager::ReadConfigException(m_config_file);
}

xmlDocPtr ConfigManager::getConfigFromFile(const std::string& config_file, const std::string& root_element_name, xmlNodePtr& config_ptr, PionLogger& logger)
{
	// make sure the config file exists
	if (! boost::filesystem::exists(config_file))
		throw MissingConfigFileException(config_file);

	// read the existing config file
	xmlDocPtr xml_doc_ptr = xmlReadFile(config_file.c_str(), NULL, XML_PARSE_NOBLANKS);
	if (xml_doc_ptr == NULL)
		throw ReadConfigException(config_file);

	// make sure that the root element is what it should be
	if ( (config_ptr = xmlDocGetRootElement(xml_doc_ptr)) == NULL
		|| xmlStrcmp(config_ptr->name,
					 reinterpret_cast<const xmlChar*>(root_element_name.c_str())) )
	{
		// config file is missing the expected root element
		throw MissingRootElementException(config_file);
	}

	// If the version number in the file is different than the current one:
	// a) if major or minor is different, throw fatal exception and refuse to run
	// b) if build number is different, update the version attribute in memory and in the file
	xmlChar* xml_char_ptr = xmlGetProp(config_ptr,
									   reinterpret_cast<const xmlChar*>(PION_VERSION_ATTRIBUTE_NAME.c_str()));
	if (xml_char_ptr == NULL)
		throw ConfigFileVersionException(config_file);
	std::string cfg_file_version = reinterpret_cast<char*>(xml_char_ptr);
	xmlFree(xml_char_ptr);
	if (cfg_file_version != PION_VERSION && cfg_file_version != "tests") {	// use "tests" to disregard check for unit tests
		// incrementally compare version number in config file to PION_VERSION
		unsigned int num_dots = 0;
		for (size_t pos = 0; pos < cfg_file_version.size() && num_dots < 2; ++pos) {
			if (cfg_file_version[pos] == '.') ++num_dots;	// count dots
			if (cfg_file_version[pos] != PION_VERSION[pos])	// compare chars
				throw ConfigFileVersionException(config_file);
		}
		if (num_dots != 2)	// check if stopped prematurely
			throw ConfigFileVersionException(config_file);
		// major and minor match, but build number does not
		if (xmlSetProp(config_ptr,
					   reinterpret_cast<const xmlChar*>(PION_VERSION_ATTRIBUTE_NAME.c_str()),
					   reinterpret_cast<const xmlChar*>(PION_VERSION)) == NULL)
			throw UpdateConfigException(root_element_name);
		if (xmlSaveFormatFileEnc(config_file.c_str(), xml_doc_ptr, "UTF-8", 1) == -1)
			throw WriteConfigException(config_file);
	}

	return xml_doc_ptr;
}

void ConfigManager::closeConfigFile(void)
{
	xmlFreeDoc(m_config_doc_ptr);
	m_config_doc_ptr = NULL;
	m_config_node_ptr = NULL;
}

void ConfigManager::saveConfigFile(void)
{
	// always create a backup copy of the config file first
	backupConfigFile();
	
	// save the latest XML config document to the file
	if (xmlSaveFormatFileEnc(m_config_file.c_str(), m_config_doc_ptr, "UTF-8", 1) == -1)
		throw WriteConfigException(m_config_file);
}
	
void ConfigManager::removeConfigFile(void)
{
	backupConfigFile();
	boost::filesystem::remove(m_config_file);
}

void ConfigManager::backupConfigFile(void)
{
	try {

		// make sure that the config file exists
		if (boost::filesystem::exists(m_config_file)) {
			const std::string backup_filename(m_config_file + BACKUP_FILE_EXTENSION);
			if (boost::filesystem::exists(backup_filename))
				boost::filesystem::remove(backup_filename);
			boost::filesystem::copy_file(m_config_file, backup_filename);
		}
	} catch (...) {
		PION_LOG_WARN(m_logger, "Failed to backup configuration file: " << m_config_file);
	}
}

void ConfigManager::writeConfigXML(std::ostream& out,
								   xmlNodePtr config_node,
								   bool include_siblings)
{
	// this should not be so hard!
	// there has to be a better way to do this....
	
	while (config_node != NULL) {
		
		// write data for the current node
		if (config_node->type == XML_ELEMENT_NODE) {
			
			// write new element tag (begin)
			out << '<' << xml_encode(reinterpret_cast<const char*>(config_node->name));
			
			// write attribute properties
			for (_xmlAttr *attr_ptr = config_node->properties;
				 attr_ptr != NULL; attr_ptr = attr_ptr->next)
			{
				xmlChar *xml_char_ptr = xmlGetProp(config_node, attr_ptr->name);
				if (xml_char_ptr != NULL) {
					out << ' ' << xml_encode(reinterpret_cast<const char*>(attr_ptr->name))
						<< "=\"" << xml_encode(reinterpret_cast<const char*>(xml_char_ptr)) << '"';
					xmlFree(xml_char_ptr);
				}
			}

			// write new XML element node
			if (config_node->children != NULL) {
				// write new element tag (end)
				out << '>';

				// recursive into children nodes
				writeConfigXML(out, config_node->children, true);
				
				// write close element tag (end)
				out << "</" << xml_encode(reinterpret_cast<const char*>(config_node->name)) << '>';
			} else {
				// write new element tag (end, no children)
				out << "/>";
			}
			
			// break if siblings should not be included
			if (! include_siblings)
				break;
			
		} else if (config_node->type == XML_TEXT_NODE) {

			// write contents of text node
			out << xml_encode(reinterpret_cast<const char*>(config_node->content));
		}

		// step to the next sibling of config_node
		config_node = config_node->next;
	}
}

void ConfigManager::writeConfigXMLHeader(std::ostream& out)
{
	out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
}
	
void ConfigManager::writeBeginPionConfigXML(std::ostream& out)
{
	writeConfigXMLHeader(out);
	out << '<' << ROOT_ELEMENT_NAME << " xmlns=\"" << CONFIG_NAMESPACE_URL << "\" "
		<< PION_VERSION_ATTRIBUTE_NAME << "=\"" << PION_VERSION << "\">" << std::endl;
}

void ConfigManager::writeEndPionConfigXML(std::ostream& out)
{
	out << "</" << ROOT_ELEMENT_NAME << '>' << std::endl;
}
	
void ConfigManager::writeBeginPionStatsXML(std::ostream& out)
{
	writeConfigXMLHeader(out);
	out << '<' << STATS_ELEMENT_NAME << " xmlns=\""
		<< CONFIG_NAMESPACE_URL << "\">" << std::endl;
}

void ConfigManager::writeEndPionStatsXML(std::ostream& out)
{
	out << "</" << STATS_ELEMENT_NAME << '>' << std::endl;
}
	
std::string ConfigManager::xml_encode(const std::string& str)
{
	std::string result;
	result.reserve(str.size() + 20);	// Assume ~5 characters converted (length increases)
	const unsigned char *ptr = reinterpret_cast<const unsigned char*>(str.c_str());
	const unsigned char *end_ptr = ptr + str.size();
	while (ptr < end_ptr) {
		// check byte ranges for valid UTF-8
		// see http://en.wikipedia.org/wiki/UTF-8
		// also, see http://www.w3.org/TR/REC-xml/#charsets
		// this implementation is the strictest subset of both
		if ((*ptr >= 0x20 && *ptr <= 0x7F) || *ptr == 0x9 || *ptr == 0xa || *ptr == 0xd) {
			// regular ASCII character
			switch(*ptr) {
			// Escape special XML characters.
			case '&':
				result += "&amp;";
				break;
			case '<':
				result += "&lt;";
				break;
			case '>':
				result += "&gt;";
				break;
			case '\"':
				result += "&quot;";
				break;
			case '\'':
				result += "&apos;";
				break;
			default:
				result += *ptr;
			}
		} else if (*ptr >= 0xC2 && *ptr <= 0xDF) {
			// two-byte sequence
			if (*(ptr+1) >= 0x80 && *(ptr+1) <= 0xBF) {
				result += *ptr;
				result += *(++ptr);
			} else {
				// insert replacement char
				result += 0xef;
				result += 0xbf;
				result += 0xbd;
			}
		} else if (*ptr >= 0xE0 && *ptr <= 0xEF) {
			// three-byte sequence
			if (*(ptr+1) >= 0x80 && *(ptr+1) <= 0xBF
				&& *(ptr+2) >= 0x80 && *(ptr+2) <= 0xBF) {
				result += *ptr;
				result += *(++ptr);
				result += *(++ptr);
			} else {
				// insert replacement char
				result += 0xef;
				result += 0xbf;
				result += 0xbd;
			}
		} else if (*ptr >= 0xF0 && *ptr <= 0xF4) {
			// four-byte sequence
			if (*(ptr+1) >= 0x80 && *(ptr+1) <= 0xBF
				&& *(ptr+2) >= 0x80 && *(ptr+2) <= 0xBF
				&& *(ptr+3) >= 0x80 && *(ptr+3) <= 0xBF) {
				result += *ptr;
				result += *(++ptr);
				result += *(++ptr);
				result += *(++ptr);
			} else {
				// insert replacement char
				result += 0xef;
				result += 0xbf;
				result += 0xbd;
			}
		} else {
			// insert replacement char
			result += 0xef;
			result += 0xbf;
			result += 0xbd;
		}
		++ptr;
	}
	
	return result;
}

std::string ConfigManager::createFilename(void)
{
	std::string file_name(createUUID());
	file_name += XML_FILE_EXTENSION;
	return file_name;
}
	
xmlNodePtr ConfigManager::createPluginConfig(const std::string& plugin_type)
{
	xmlNodePtr config_ptr = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>(PLUGIN_ELEMENT_NAME.c_str()));
	xmlNodeSetContent(config_ptr,  reinterpret_cast<const xmlChar*>(plugin_type.c_str()));
	return config_ptr;
}

xmlNodePtr ConfigManager::createResourceConfig(const std::string& resource_name,
											   const char *buf, std::size_t len)
{
	// sanity check
	if (buf == NULL || len == 0)
		throw BadXMLBufferException();
	
	// parse request payload content as XML
	xmlNodePtr node_ptr = NULL;
	xmlDocPtr doc_ptr = xmlParseMemory(buf, len);
	if (doc_ptr == NULL)
		throw XMLBufferParsingException(buf);

	// find the ROOT element
	if ( (node_ptr = xmlDocGetRootElement(doc_ptr)) == NULL ) {
		xmlFreeDoc(doc_ptr);
		// buf is missing the root "PionConfig" element 
		throw MissingRootElementException(buf);
	}

	// check if root is the "PionConfig" root element
	if (xmlStrcmp(node_ptr->name, reinterpret_cast<const xmlChar*>(ROOT_ELEMENT_NAME.c_str())) ) {
		// no, try to find the resource at top level
		node_ptr = findConfigNodeByName(resource_name, node_ptr);
	} else {
		// yes, try to find the resource in children
		node_ptr = findConfigNodeByName(resource_name, node_ptr->children);
	}
	if (node_ptr == NULL) {
		xmlFreeDoc(doc_ptr);
		throw MissingResourceElementException(resource_name);
	}

	// found the resource config -> make a copy of it
	node_ptr = xmlCopyNodeList(node_ptr->children);
	
	// free the temporary document
	xmlFreeDoc(doc_ptr);
	
	// return the copied configuration info
	return node_ptr;
}
	
std::string ConfigManager::createFilename(const std::string& file_path)
{
	boost::filesystem::path new_path(file_path);
	new_path /= createFilename();
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
    return boost::filesystem::system_complete(new_path).string();
#else
    return boost::filesystem::system_complete(new_path).file_string();
#endif 
}
	
std::string ConfigManager::resolveRelativePath(const std::string& base_path_to_file,
											   const std::string& orig_path)
{
	// return the original if it is not relative
	if (boost::filesystem::path(orig_path).is_complete())
		return orig_path;
	
	// build a new path using the config file location as a starting point
	boost::filesystem::path new_path(boost::filesystem::system_complete(base_path_to_file));
	new_path.remove_leaf();
	new_path /= orig_path;
	new_path.normalize();
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
    return new_path.string();
#else
    return new_path.file_string();
#endif 
}

std::string ConfigManager::resolveRelativeDataPath(const std::string& orig_path) const
{
	// return the original if it is not relative
	if (boost::filesystem::path(orig_path).is_complete())
		return orig_path;
	
	boost::filesystem::path new_path(boost::filesystem::system_complete(getDataDirectory()));
	new_path /= orig_path;
	new_path.normalize();
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
    return new_path.string();
#else
    return new_path.file_string();
#endif 
}

void ConfigManager::verifyDirectory(const std::string& element, std::string& dir) const
{
	// normalize potentially relative path
	dir = ConfigManager::resolveRelativePath(dir);

	// make sure that the directory exists
	if (! boost::filesystem::exists(dir) )
		throw DirectoryNotFoundException(element, dir);
		
	// make sure it's a directory
	if (! boost::filesystem::is_directory(dir) )
		throw NotADirectoryException(element, dir);
}

void ConfigManager::runAsUserGroup(void)
{
	// get group to run Pion as
	// MUST BE PERFORMED BEFORE CHANGING USER
	// (since changing user may downgrade credentials)
	std::string tempStr;
	if (getConfigOption("Group", tempStr, m_config_node_ptr->children)) {
#ifdef _MSC_VER
		PION_LOG_ERROR(m_logger, "Windows not supported for group masquerading: " << tempStr);
#else
		long group_id = PionAdminRights::runAsGroup(tempStr);
		if (group_id >= 0) {
			PION_LOG_INFO(m_logger, "Running as group "
				<< tempStr << " (" << group_id << ")");
		} else {
			PION_LOG_ERROR(m_logger, "Unable to run as group "
				<< tempStr << " (" << group_id << ")");
		}
#endif
	}
	
	// get user to run Pion as
	if (getConfigOption("User", tempStr, m_config_node_ptr->children)) {
#ifdef _MSC_VER
		PION_LOG_ERROR(m_logger, "Windows not supported for user masquerading: " << tempStr);
#else
		long user_id = PionAdminRights::runAsUser(tempStr);
		if (user_id >= 0) {
			PION_LOG_INFO(m_logger, "Running as user "
				<< tempStr << " (" << user_id << ")");
		} else {
			PION_LOG_ERROR(m_logger, "Unable to run as user "
				<< tempStr << " (" << user_id << ")");
		}
#endif
	}
}

bool ConfigManager::getNodeId(xmlNodePtr config_node, std::string& node_id)
{
	node_id = "";
	xmlChar *xml_char_ptr = xmlGetProp(config_node,
									   reinterpret_cast<const xmlChar*>(ID_ATTRIBUTE_NAME.c_str()));
	if (xml_char_ptr != NULL && xml_char_ptr[0]!='\0')
		node_id = reinterpret_cast<char*>(xml_char_ptr);
	xmlFree(xml_char_ptr);
	return(! node_id.empty());
}
	
xmlNodePtr ConfigManager::findConfigNodeByName(const std::string& element_name,
											   xmlNodePtr starting_node)
{
	xmlNodePtr matching_node = NULL;
	
	// find the element node in the XML config document
	for (xmlNodePtr cur_node = starting_node;
		 cur_node != NULL; cur_node = cur_node->next)
	{
		if (cur_node->type == XML_ELEMENT_NODE
			&& xmlStrcmp(cur_node->name, reinterpret_cast<const xmlChar*>(element_name.c_str()))==0)
		{
			// found it!
			matching_node = cur_node;
			break;
		}
	}
	
	return matching_node;
}
	
xmlNodePtr ConfigManager::findConfigNodeByContent(const std::string& element_name,
												  const std::string& content_value,
												  xmlNodePtr starting_node)
{
	xmlNodePtr matching_node = NULL;

	// find the element node in the XML config document
	for (xmlNodePtr cur_node = starting_node;
		 cur_node != NULL; cur_node = cur_node->next)
	{
		if (cur_node->type == XML_ELEMENT_NODE
			&& xmlStrcmp(cur_node->name, reinterpret_cast<const xmlChar*>(element_name.c_str()))==0)
		{
			// found an element with matching name
			xmlChar *xml_char_ptr = xmlNodeGetContent(cur_node);
			if (xml_char_ptr != NULL) {
				if (content_value == reinterpret_cast<char*>(xml_char_ptr)) {
					// found it!
					matching_node = cur_node;
					break;
				}
				xmlFree(xml_char_ptr);
			}
		}
	}
		
	return matching_node;
}
	
xmlNodePtr ConfigManager::findConfigNodeByAttr(const std::string& element_name,
											   const std::string& attr_name,
											   const std::string& attr_value,
											   xmlNodePtr starting_node)
{
	xmlNodePtr matching_node = NULL;
	
	// find the element node in the XML config document
	for (xmlNodePtr cur_node = starting_node;
		 cur_node != NULL; cur_node = cur_node->next)
	{
		if (cur_node->type == XML_ELEMENT_NODE
			&& xmlStrcmp(cur_node->name, reinterpret_cast<const xmlChar*>(element_name.c_str()))==0)
		{
			// found an element with matching name
			xmlChar *xml_char_ptr = xmlGetProp(cur_node, reinterpret_cast<const xmlChar*>(attr_name.c_str()));
			if (xml_char_ptr != NULL) {
				if (attr_value == reinterpret_cast<char*>(xml_char_ptr)) {
					// found it!
					matching_node = cur_node;
					break;
				}
				xmlFree(xml_char_ptr);
			}
		}
	}
	
	return matching_node;
}
	
bool ConfigManager::getConfigOption(const std::string& option_name,
									std::string& option_value,
									const xmlNodePtr starting_node)
{
	// find the option's element
	xmlNodePtr option_node = findConfigNodeByName(option_name, starting_node);
	if (option_node != NULL) {
		xmlChar *xml_char_ptr = xmlNodeGetContent(option_node);
		if (xml_char_ptr != NULL) {
			// found it
			if (xml_char_ptr[0] != '\0') {
				// it is not empty
				option_value = reinterpret_cast<char*>(xml_char_ptr);
				xmlFree(xml_char_ptr);
				return true;
			}
			xmlFree(xml_char_ptr);
		}
	}

	// option was not found or it had an empty value
	option_value.clear();
	return false;
}

bool ConfigManager::getConfigOptionEmptyOk(const std::string& option_name,
							std::string& option_value,
							const xmlNodePtr starting_node)
{
	if (findConfigNodeByName(option_name, starting_node)) {
		option_value.clear();
		getConfigOption(option_name, option_value, starting_node);
		return true;
	} else
		return false;
}

std::string ConfigManager::getAttribute(const char *name, const xmlNodePtr ptr)
{
	std::string id_str;
	xmlChar *xml_char_ptr = xmlGetProp(ptr, reinterpret_cast<const xmlChar*>(name));
	if (xml_char_ptr != NULL) {
		if (xml_char_ptr[0] != '\0')
			id_str = reinterpret_cast<char*>(xml_char_ptr);
		xmlFree(xml_char_ptr);
	}
	return id_str;
}

bool ConfigManager::updateConfigOption(const std::string& option_name,
									   const std::string& option_value,
									   xmlNodePtr parent_node)
{
	// find the option's element
	xmlNodePtr option_node = findConfigNodeByName(option_name, parent_node->children);
	if (option_node != NULL) {
		// an existing value is assigned to the option
		if (option_value.empty()) {
			// changing to an empty value -> remove the element node
			xmlUnlinkNode(option_node);
			xmlFreeNodeList(option_node);
		} else {
			// change the value of an existing element
			xmlNodeSetContent(option_node, reinterpret_cast<const xmlChar*>(xml_encode(option_value).c_str()));
		}
	} else {
		// no value is currently set for the option
		// only add an element if the new value is not empty
		if (! option_value.empty()) {
			if (xmlNewTextChild(parent_node, NULL,
								reinterpret_cast<const xmlChar*>(option_name.c_str()),
								reinterpret_cast<const xmlChar*>(option_value.c_str())) == NULL)
				return false;
		}
	}
	
	return true;
}

void ConfigManager::openPluginConfig(const std::string& plugin_name)
{
	// open the file and find the "config" root element
	ConfigManager::openConfigFile();
	
	xmlNodePtr plugin_node = m_config_node_ptr->children;
	while ( (plugin_node = findConfigNodeByName(plugin_name, plugin_node)) != NULL)
	{
		// parse new plug-in definition
		std::string new_plugin_id;
		if (! getNodeId(plugin_node, new_plugin_id))
			throw EmptyPluginIdException(getConfigFile());
		
		// find plug-in type
		std::string new_plugin_type;
		if (! getConfigOption(PLUGIN_ELEMENT_NAME, new_plugin_type, plugin_node->children))
			throw EmptyPluginElementException(new_plugin_id);

		// execute callback to actually add the plug-in
		addPluginNoLock(new_plugin_id, new_plugin_type, plugin_node->children);

		// look for more plug-in nodes
		plugin_node = plugin_node->next;
	}
}
	
bool ConfigManager::setPluginConfig(xmlNodePtr plugin_node_ptr, xmlNodePtr config_ptr)
{
	xmlNodePtr plugin_config_copy = xmlCopyNodeList(config_ptr);
	if (plugin_config_copy == NULL)
		return false;
	
	// remove the plugin type element, if it exists in the new config
	xmlNodePtr plugin_type_node = findConfigNodeByName(PLUGIN_ELEMENT_NAME,
													   plugin_config_copy);
	if (plugin_type_node != NULL) {
		// check if we are removing the first node in the list
		if (plugin_config_copy == plugin_type_node)
			plugin_config_copy = plugin_type_node->next;

		// remove the Plugin type node (this cannot be changed)
		xmlUnlinkNode(plugin_type_node);
		xmlFreeNode(plugin_type_node);
		
		// stop early if this is the only node in the configuration
		if (plugin_config_copy == NULL)
			return true;
	}
	
	// clean namespaces in config nodes: work-around for libxml namespace bugs
	for (xmlNodePtr tmp_node = plugin_config_copy; tmp_node != NULL;
		 tmp_node = tmp_node->next)
	{
		tmp_node->ns = tmp_node->nsDef = NULL;
	}
	
	// add the plugin config to the config file
	if (xmlAddChildList(plugin_node_ptr, plugin_config_copy) == NULL) {
		xmlFreeNodeList(plugin_config_copy);
		return false;
	}

	return true;
}
	
void ConfigManager::setPluginConfig(const std::string& plugin_name,
									const std::string& plugin_id,
									const xmlNodePtr config_ptr)
{
	// find the plug-in element in the XML config document
	xmlNodePtr plugin_node = findConfigNodeByAttr(plugin_name,
												  ID_ATTRIBUTE_NAME,
												  plugin_id,
												  m_config_node_ptr->children);
	if (plugin_node == NULL)
		throw UpdatePluginConfigException(plugin_id);
	
	// remove all children except the plugin type node
	xmlNodePtr cur_node;
	xmlNodePtr next_node = plugin_node->children;
	while (next_node != NULL) {
		cur_node = next_node;
		next_node = next_node->next;
		
		if (cur_node->type != XML_ELEMENT_NODE
			|| xmlStrcmp(cur_node->name, reinterpret_cast<const xmlChar*>(PLUGIN_ELEMENT_NAME.c_str()))!=0)
		{
			xmlUnlinkNode(cur_node);
			xmlFreeNode(cur_node);
		}
	}
	
	// update the plugin configuration parameters (if any)
	if (config_ptr != NULL) {
		if (! setPluginConfig(plugin_node, config_ptr))
			throw UpdatePluginConfigException(plugin_id);
	}
	
	// save the new XML config file
	saveConfigFile();
}

void ConfigManager::addPluginConfig(const std::string& plugin_name,
									const std::string& plugin_id,
									const std::string& plugin_type,
									const xmlNodePtr config_ptr)
{
	// create a new node for the plug-in and add it to the XML config document
	xmlNodePtr new_plugin_node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>(plugin_name.c_str()));
	if (new_plugin_node == NULL)
		throw AddPluginConfigException(plugin_type);
	if ((new_plugin_node=xmlAddChild(m_config_node_ptr, new_plugin_node)) == NULL) {
		xmlFreeNode(new_plugin_node);
		throw AddPluginConfigException(plugin_type);
	}
	
	// set the id attribute for the plug-in element
	if (xmlNewProp(new_plugin_node, reinterpret_cast<const xmlChar*>(ID_ATTRIBUTE_NAME.c_str()),
				   reinterpret_cast<const xmlChar*>(plugin_id.c_str())) == NULL)
		throw AddPluginConfigException(plugin_type);
	
	// add a plugin type child element to the plug-in element
	if (xmlNewTextChild(new_plugin_node, NULL,
						reinterpret_cast<const xmlChar*>(PLUGIN_ELEMENT_NAME.c_str()),
						reinterpret_cast<const xmlChar*>(plugin_type.c_str())) == NULL)
		throw AddPluginConfigException(plugin_type);
	
	// update the plug-in configuration parameters (if any)
	if (config_ptr != NULL) {
		if (! setPluginConfig(new_plugin_node, config_ptr))
			throw AddPluginConfigException(plugin_type);
	}
	
	// save the new XML config file
	saveConfigFile();
}
		
void ConfigManager::removePluginConfig(const std::string& plugin_name,
									   const std::string& plugin_id)
{
	// find the plug-in node to remove
	xmlNodePtr plugin_node = findConfigNodeByAttr(plugin_name,
												  ID_ATTRIBUTE_NAME,
												  plugin_id,
												  m_config_node_ptr->children);
	if (plugin_node == NULL)
		throw RemovePluginConfigException(plugin_id);
	
	// remove the plug-in element (and all of its children)
	xmlUnlinkNode(plugin_node);
	xmlFreeNode(plugin_node);
	
	// save the new XML config file
	saveConfigFile();
}

}	// end namespace platform
}	// end namespace pion
