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

#include <pion/platform/Reactor.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


// static members of Reactor
const std::string			Reactor::EVENT_ELEMENT_NAME = "event";
const std::string			Reactor::COMMENT_ELEMENT_NAME = "comment";
	
		
// Reactor member functions

void Reactor::setConfig(const Vocabulary& v, const xmlNodePtr config_ptr)
{
	PlatformPlugin::setConfig(v, config_ptr);
}

void Reactor::updateVocabulary(const Vocabulary& v)
{
	PlatformPlugin::updateVocabulary(v);
}	
	
	
}	// end namespace platform
}	// end namespace pion
