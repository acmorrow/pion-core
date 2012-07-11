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
#include "QueryService.hpp"
#include <pion/net/HTTPResponseWriter.hpp>
#include <pion/net/PionUser.hpp>
#include "PlatformConfig.hpp"

using namespace pion;
using namespace pion::net;

namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins

	
// QueryService member functions

/// handles requests for QueryService
void QueryService::operator()(HTTPRequestPtr& request, TCPConnectionPtr& tcp_conn)
{
	// split out the path branches from the HTTP request
	PathBranches branches;
	splitPathBranches(branches, request->getResource());

	// use a response in case we want to change any of the headers/etc.
	// while processing the request
	HTTPResponsePtr response_ptr(new HTTPResponse(*request));

	// use a stringstream for the response content
	// since HTTPResponseWriter does not yet have a stream wrapper available
	std::ostringstream ss;

/*
	for (int i = 0; i < branches.size() ; i++ )
		xml += branches[i] + "::";

	xml += "\r\n";
*/

	if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {
		if (!branches.empty() && branches.front() == "reactors") {
			/*
			 *	branches[0]	== reactors
			 *	branches[1] == UUID
			 *	branches[2] == aggregate/example/info
			 */

			if (branches.size() >= 2) {
				const std::string reactor_id(branches.at(1));
				if (getConfig().getReactionEngine().hasPlugin(reactor_id)) {

					// Check whether the User has permission for this Reactor.
					bool reactor_allowed = getConfig().getUserManagerPtr()->accessAllowed(request->getUser(), getConfig().getReactionEngine(), reactor_id);
					if (! reactor_allowed) {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission for Reactor " + reactor_id + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

					getConfig().getReactionEngine().query(
						reactor_id, ss, branches,
						request->getQueryParams());
				} else {
					// Log an error and send a 404 (Not Found) response.
					handleNotFoundRequest(request, tcp_conn);
					return;
				}
			} else {
				// send detailed statistics for all Reactors
				getConfig().getReactionEngine().writeStatsXML(ss, "", true);
			}
		} else if (! branches.empty() && branches.front() == "permissions") {
			getConfig().getUserManagerPtr()->writePermissionsXML(ss, request->getUser()->getUsername());
		} else {
			throw UnknownQueryException();
		}
	} else {
		// Log an error and send a 405 (Method Not Allowed) response.
		handleMethodNotAllowed(request, tcp_conn, "GET");
		return;
	}

	// Set Content-type to "text/xml"
	HTTPResponseWriterPtr writer(HTTPResponseWriter::create(tcp_conn, response_ptr,
															std::bind(&TCPConnection::finish, tcp_conn)));
	writer->getResponse().setContentType(HTTPTypes::CONTENT_TYPE_XML);

	writer->write(ss.str());
	
	// send the writer
	writer->send();
}


}	// end namespace plugins
}	// end namespace pion


/// creates new QueryService objects
extern "C" PION_SERVICE_API pion::server::PlatformService *pion_create_QueryService(void)
{
	return new pion::plugins::QueryService();
}

/// destroys QueryService objects
extern "C" PION_SERVICE_API void pion_destroy_QueryService(pion::plugins::QueryService *service_ptr)
{
	delete service_ptr;
}
