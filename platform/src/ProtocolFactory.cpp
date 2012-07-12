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

#include <pion/platform/ProtocolFactory.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


// static members of ProtocolFactory
const std::string			ProtocolFactory::DEFAULT_CONFIG_FILE = "protocols.xml";
const std::string			ProtocolFactory::PROTOCOL_ELEMENT_NAME = "Protocol";


// ProtocolFactory member functions

ProtocolFactory::ProtocolFactory(const VocabularyManager& vocab_mgr)
	: PluginConfig<Protocol>(vocab_mgr, DEFAULT_CONFIG_FILE, PROTOCOL_ELEMENT_NAME)
{
	setLogger(PION_GET_LOGGER("pion.platform.ProtocolFactory"));
}
	
ProtocolPtr ProtocolFactory::getProtocol(const std::string& protocol_id)
{
	std::lock_guard<std::mutex> factory_lock(m_mutex);
	Protocol *protocol_ptr = m_plugins.get(protocol_id);
	// throw an exception if the protocol was not found
	if (protocol_ptr == NULL)
		throw ProtocolNotFoundException(protocol_id);
	// return a cloned instance of the Protocol since its state may change
	// while encoding or decoding data streams
	return protocol_ptr->clone();
}

void ProtocolFactory::setProtocolConfig(const std::string& protocol_id,
								  const xmlNodePtr config_ptr)
{
	// convert PluginNotFound exceptions into ProtocolNotFound exceptions
	try {
		PluginConfig<Protocol>::setPluginConfig(protocol_id, config_ptr);
	} catch (PluginManager<Protocol>::PluginNotFoundException&) {
		throw ProtocolNotFoundException(protocol_id);
	}
}

std::string ProtocolFactory::addProtocol(const xmlNodePtr config_ptr)
{
	return PluginConfig<Protocol>::addPlugin(config_ptr);
}

void ProtocolFactory::removeProtocol(const std::string& protocol_id) {
	// convert PluginNotFound exceptions into ProtocolNotFound exceptions
	try {
		PluginConfig<Protocol>::removePlugin(protocol_id);
	} catch (PluginManager<Protocol>::PluginNotFoundException&) {
		throw ProtocolNotFoundException(protocol_id);
	}
}

	
}	// end namespace platform
}	// end namespace pion
