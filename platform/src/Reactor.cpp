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

#include <boost/lexical_cast.hpp>
#include <pion/platform/ConfigManager.hpp>
#include <pion/platform/Reactor.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


// static members of Reactor

const std::string			Reactor::WORKSPACE_ELEMENT_NAME = "Workspace";
const std::string			Reactor::X_COORDINATE_ELEMENT_NAME = "X";
const std::string			Reactor::Y_COORDINATE_ELEMENT_NAME = "Y";
	
	
// Reactor member functions

void Reactor::setConfig(const Vocabulary& v, const xmlNodePtr config_ptr)
{
	PlatformPlugin::setConfig(v, config_ptr);
	
	// get the Reactor's Workspace
	ConfigManager::getConfigOption(WORKSPACE_ELEMENT_NAME, m_workspace,
								   config_ptr);

	// get the Reactor's X coordinate
	std::string coordinate_str;
	ConfigManager::getConfigOption(X_COORDINATE_ELEMENT_NAME, coordinate_str,
								   config_ptr);
	m_x_coordinate = (coordinate_str.empty() ? 0 : boost::lexical_cast<unsigned int>(coordinate_str));
	
	// get the Reactor's Y coordinate
	ConfigManager::getConfigOption(Y_COORDINATE_ELEMENT_NAME, coordinate_str,
								   config_ptr);
	m_y_coordinate = (coordinate_str.empty() ? 0 : boost::lexical_cast<unsigned int>(coordinate_str));
}
	
void Reactor::updateVocabulary(const Vocabulary& v)
{
	PlatformPlugin::updateVocabulary(v);
}	
	
void Reactor::addConnection(const std::string& connection_id,
							EventHandler connection_handler)
{
	boost::mutex::scoped_lock reactor_lock(m_mutex);

	// check if it already connected
	if (m_connections.find(connection_id) != m_connections.end())
		throw AlreadyConnectedException(connection_id);
	
	// add the new connection
	m_connections.insert(std::make_pair(connection_id, connection_handler));
}

void Reactor::removeConnection(const std::string& connection_id)
{
	boost::mutex::scoped_lock reactor_lock(m_mutex);
	
	// find the connection to remove
	OutputConnections::iterator i = m_connections.find(connection_id);
	if (i == m_connections.end())
		throw ConnectionNotFoundException(connection_id);
	
	// remove the connection
	m_connections.erase(i);
}
	
}	// end namespace platform
}	// end namespace pion
