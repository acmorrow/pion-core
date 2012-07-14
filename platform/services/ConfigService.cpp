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

#include <sstream>
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <pion/net/HTTPResponseWriter.hpp>
#include <pion/platform/Comparison.hpp>
#include "PlatformConfig.hpp"
#include "ConfigService.hpp"

using namespace pion::net;
using namespace pion::server;
using namespace pion::platform;


namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins


// static members of ConfigService
const std::string		ConfigService::UI_DIRECTORY_ELEMENT_NAME = "UIDirectory";


// ConfigService member functions

void ConfigService::setConfig(const pion::platform::Vocabulary& v,
							  const xmlNodePtr config_ptr)
{
	pion::server::PlatformService::setConfig(v, config_ptr);

	// get the UI directory
	if (! ConfigManager::getConfigOption(UI_DIRECTORY_ELEMENT_NAME, m_ui_directory, config_ptr))
		throw MissingUIDirectoryException();
}

void ConfigService::operator()(HTTPRequestPtr& request, TCPConnectionPtr& tcp_conn)
{
	// split out the path branches from the HTTP request
	PathBranches branches;
	splitPathBranches(branches, request->getResource());

	// use a response in case we want to change any of the headers/etc.
	// while processing the request
	HTTPResponsePtr response_ptr(new HTTPResponse(*request));

	// use a stringstream for the response content
	// since HTTPResponseWriter does not yet have a stream wrapper available
	std::stringstream ss;

	PlatformConfig& cfg = getConfig();

	try {

		if (branches.empty()) {

			// send platform configuration info
			cfg.writeConfigXML(ss);

		} else if (branches.front() == "vocabularies") {
			//
			// BEGIN VOCABULARIES CONFIG
			//
			if (branches.size() == 1) {

				// returns a list of all Vocabularies
				cfg.getVocabularyManager().writeConfigXML(ss);

			} else if (branches.size() == 2) {
				// branches[1] == vocabulary_id
				const std::string vocab_id(branches[1]);

				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {
					// retrieve an existing Vocabulary's configuration

					if (! cfg.getVocabularyManager().writeConfigXML(ss, vocab_id))
						throw VocabularyManager::VocabularyNotFoundException(vocab_id);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_POST) {

					// add (create) a new Vocabulary

					// Check whether the User has permission to create the specified Vocabulary.
					if (cfg.getUserManagerPtr()->creationAllowed(request->getUser(), cfg.getVocabularyManager(), NULL)) {
						cfg.getVocabularyManager().addVocabulary(vocab_id,
																 request->getContent(),
																 request->getContentLength());
						// send a 201 (Created) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_CREATED);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_CREATED);

						// respond with the new Vocabulary's configuration
						if (! cfg.getVocabularyManager().writeConfigXML(ss, vocab_id))
							throw VocabularyManager::VocabularyNotFoundException(vocab_id);
					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to create a Vocabulary.";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_PUT) {
					// update existing Vocabulary's configuration

					// convert request content into XML configuration info
					xmlNodePtr vocab_config_ptr =
						VocabularyConfig::createVocabularyConfig(request->getContent(),
																 request->getContentLength());

					// Check whether the User has permission to make the specified modification to the specified Vocabulary.
					if (cfg.getUserManagerPtr()->updateAllowed(request->getUser(), cfg.getVocabularyManager(), vocab_id, vocab_config_ptr)) {
						try {
							// push the new config into the VocabularyManager
							cfg.getVocabularyManager().setVocabularyConfig(vocab_id, vocab_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(vocab_config_ptr);
							throw;
						}
						xmlFreeNodeList(vocab_config_ptr);

						// respond with the Vocabulary's updated configuration
						if (! cfg.getVocabularyManager().writeConfigXML(ss, vocab_id))
							throw VocabularyManager::VocabularyNotFoundException(vocab_id);
					} else {
						xmlFreeNodeList(vocab_config_ptr);

						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to modify Vocabulary " + vocab_id + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_DELETE) {
					// delete an existing Vocabulary

					// Check whether the User has permission to remove the specified Vocabulary.
					if (cfg.getUserManagerPtr()->removalAllowed(request->getUser(), cfg.getVocabularyManager(), vocab_id)) {
						cfg.getVocabularyManager().removeVocabulary(vocab_id);

						// send a 204 (No Content) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_NO_CONTENT);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_NO_CONTENT);
					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to delete Vocabulary " + vocab_id + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, POST, PUT, DELETE");
					return;
				}

			} else {
				// Log an error and send a 404 (Not Found) response.
				handleNotFoundRequest(request, tcp_conn);
				return;
			}
			//
			// END VOCABULARIES CONFIG
			//
		} else if (branches.front() == "terms") {
			//
			// BEGIN TERMS CONFIG
			//
			if (branches.size() == 1) {

				// return a list of all Terms
				cfg.getVocabularyManager().writeTermConfigXML(ss);

			} else if (branches.size() == 2) {
				// branches[1] == term_id
				const std::string term_id(branches[1]);
				const std::string vocab_id(term_id.substr(0, term_id.find_last_of('#')));

				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {
					// retrieve an existing Vocabulary Term's configuration

					if (! cfg.getVocabularyManager().writeTermConfigXML(ss, term_id))
						throw Vocabulary::TermNotFoundException(term_id);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_POST) {

					// add (create) a new Vocabulary Term

					// convert request content into XML configuration info
					xmlNodePtr term_config_ptr =
						VocabularyConfig::createTermConfig(request->getContent(),
														   request->getContentLength());

					// Check whether the User has permission to create the specified Vocabulary Term.
					if (cfg.getUserManagerPtr()->creationAllowed(request->getUser(), cfg.getVocabularyManager(), term_config_ptr)) {

						// add the new Vocabulary Term to the VocabularyManager
						try {
							cfg.getVocabularyManager().addTerm(vocab_id, term_id, term_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(term_config_ptr);
							throw;
						}
						xmlFreeNodeList(term_config_ptr);

						// send a 201 (created) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_CREATED);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_CREATED);

						// respond with the new Vocabulary Term's configuration
						if (! cfg.getVocabularyManager().writeTermConfigXML(ss, term_id))
							throw Vocabulary::TermNotFoundException(term_id);

					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to create a Vocabulary Term.";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_PUT) {
					// update existing Vocabulary Term's configuration

					// convert request content into XML configuration info
					xmlNodePtr term_config_ptr =
						VocabularyConfig::createTermConfig(request->getContent(),
														   request->getContentLength());

					// Check whether the User has permission to make the specified modification to the specified Vocabulary Term.
					if (cfg.getUserManagerPtr()->updateAllowed(request->getUser(), cfg.getVocabularyManager(), term_id, term_config_ptr)) {
						try {
							// push the new config into the VocabularyManager
							cfg.getVocabularyManager().updateTerm(vocab_id, term_id, term_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(term_config_ptr);
							throw;
						}
						xmlFreeNodeList(term_config_ptr);

						// respond with the Vocabulary Term's updated configuration
						if (! cfg.getVocabularyManager().writeTermConfigXML(ss, term_id))
							throw Vocabulary::TermNotFoundException(term_id);

					} else {
						xmlFreeNodeList(term_config_ptr);

						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to modify Vocabulary Term " + term_id + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_DELETE) {
					// delete an existing Vocabulary Term

					// Check whether the User has permission to remove the specified Vocabulary Term.
					if (cfg.getUserManagerPtr()->removalAllowed(request->getUser(), cfg.getVocabularyManager(), term_id)) {
						cfg.getVocabularyManager().removeTerm(vocab_id, term_id);

						// send a 204 (No Content) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_NO_CONTENT);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_NO_CONTENT);
					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to delete Vocabulary Term " + term_id + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, POST, PUT, DELETE");
					return;
				}

			} else {
				// Log an error and send a 404 (Not Found) response.
				handleNotFoundRequest(request, tcp_conn);
				return;
			}
			//
			// END TERMS CONFIG
			//
		} else if (branches.front() == "comparisons") {
			//
			// BEGIN COMPARISONS CONFIG
			//
			if (branches.size() == 1) {

				// returns a list of all Comparisons
				ConfigManager::writeBeginPionConfigXML(ss);
				Comparison::writeComparisonsXML(ss);
				ConfigManager::writeEndPionConfigXML(ss);

			} else {
				// Log an error and send a 404 (Not Found) response.
				handleNotFoundRequest(request, tcp_conn);
				return;
			}
			//
			// END COMPARISONS CONFIG
			//
		} else if (branches.front() == "codecs") {
			//
			// BEGIN CODECS CONFIG
			//
			if (branches.size() == 1) {
				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {

					// retrieve configuration for all Codecs
					cfg.getCodecFactory().writeConfigXML(ss);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_POST) {

					// add (create) a new Codec

					// convert request content into XML configuration info
					xmlNodePtr codec_config_ptr =
						CodecFactory::createCodecConfig(request->getContent(),
														request->getContentLength());

					// Check whether the User has permission to create the specified Codec.
					if (cfg.getUserManagerPtr()->creationAllowed(request->getUser(), cfg.getCodecFactory(), codec_config_ptr)) {
						// add the new Codec to the CodecFactory
						std::string codec_id;
						try {
							codec_id = cfg.getCodecFactory().addCodec(codec_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(codec_config_ptr);
							throw;
						}

						xmlFreeNodeList(codec_config_ptr);

						// send a 201 (Created) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_CREATED);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_CREATED);

						// respond with the new Codec's configuration
						if (! cfg.getCodecFactory().writeConfigXML(ss, codec_id))
							throw CodecFactory::CodecNotFoundException(codec_id);
					} else {
						xmlFreeNodeList(codec_config_ptr);

						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to create a Codec.";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, POST");
					return;
				}

			} else if (branches.size() == 2) {
				// branches[1] == codec_id

				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {
					// retrieve an existing Codec's configuration

					if (! cfg.getCodecFactory().writeConfigXML(ss, branches[1]))
						throw CodecFactory::CodecNotFoundException(branches[1]);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_PUT) {
					// update existing Codec's configuration

					// convert request content into XML configuration info
					xmlNodePtr codec_config_ptr =
						CodecFactory::createCodecConfig(request->getContent(),
														request->getContentLength());

					// Check whether the User has permission to make the specified modification to the specified Codec.
					if (cfg.getUserManagerPtr()->updateAllowed(request->getUser(), cfg.getCodecFactory(), branches[1], codec_config_ptr)) {
						try {
							// push the new config into the CodecFactory
							cfg.getCodecFactory().setCodecConfig(branches[1], codec_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(codec_config_ptr);
							throw;
						}
						xmlFreeNodeList(codec_config_ptr);

						// respond with the Codec's updated configuration
						if (! cfg.getCodecFactory().writeConfigXML(ss, branches[1]))
							throw CodecFactory::CodecNotFoundException(branches[1]);
					} else {
						xmlFreeNodeList(codec_config_ptr);

						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to modify Codec " + branches[1] + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_DELETE) {
					// delete an existing Codec

					// Check whether the User has permission to remove the specified Codec.
					if (cfg.getUserManagerPtr()->removalAllowed(request->getUser(), cfg.getCodecFactory(), branches[1])) {
						cfg.getCodecFactory().removeCodec(branches[1]);

						// send a 204 (No Content) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_NO_CONTENT);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_NO_CONTENT);
					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to delete Codec " + branches[1] + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, PUT, DELETE");
					return;
				}

			} else {
				// Log an error and send a 404 (Not Found) response.
				handleNotFoundRequest(request, tcp_conn);
				return;
			}
			//
			// END CODECS CONFIG
			//
		} else if (branches.front() == "databases") {
			//
			// BEGIN DATABASES CONFIG
			//
			if (branches.size() == 1) {
				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {

					// retrieve configuration for all Databases
					cfg.getDatabaseManager().writeConfigXML(ss);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_POST) {

					// add (create) a new Database

					// convert request content into XML configuration info
					xmlNodePtr database_config_ptr =
						DatabaseManager::createDatabaseConfig(request->getContent(),
															  request->getContentLength());

					// Check whether the User has permission to create the specified Database.
					if (cfg.getUserManagerPtr()->creationAllowed(request->getUser(), cfg.getDatabaseManager(), database_config_ptr)) {
						// add the new Database to the DatabaseManager
						std::string database_id;
						try {
							database_id = cfg.getDatabaseManager().addDatabase(database_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(database_config_ptr);
							throw;
						}
						xmlFreeNodeList(database_config_ptr);

						// send a 201 (Created) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_CREATED);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_CREATED);

						// respond with the new Database's configuration
						if (! cfg.getDatabaseManager().writeConfigXML(ss, database_id))
							throw DatabaseManager::DatabaseNotFoundException(database_id);
					} else {
						xmlFreeNodeList(database_config_ptr);

						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to create a Database.";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, POST");
					return;
				}

			} else if (branches[1] == "plugins") {

				// Send a list of all Databases found in the UI directory

				ConfigManager::writeBeginPionConfigXML(ss);

				// Iterate through all the subdirectories of the Database directory (e.g. SQLiteDatabase).
				std::string database_directory = m_ui_directory + "/plugins/databases";
				boost::filesystem::directory_iterator end;
				for (boost::filesystem::directory_iterator it(database_directory); it != end; ++it) {
					if (boost::filesystem::is_directory(*it)) {
						// Skip directories starting with a '.'.
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
                        if (it->path().filename().string().substr(0, 1) == ".") continue;
                        ss << "<Database>"
                        << "<Plugin>" << it->path().filename().string() << "</Plugin>"
                        << "</Database>";
#else
                        if (it->path().leaf().substr(0, 1) == ".") continue;
                        ss << "<Database>"
                        << "<Plugin>" << it->path().leaf() << "</Plugin>"
                        << "</Database>";
#endif 

						
					}
				}

				ConfigManager::writeEndPionConfigXML(ss);

			} else if (branches.size() == 2) {
				// branches[1] == database_id

				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {
					// retrieve an existing Database's configuration

					if (! cfg.getDatabaseManager().writeConfigXML(ss, branches[1]))
						throw DatabaseManager::DatabaseNotFoundException(branches[1]);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_PUT) {
					// update existing Database's configuration

					// convert request content into XML configuration info
					xmlNodePtr database_config_ptr =
						DatabaseManager::createDatabaseConfig(request->getContent(),
															  request->getContentLength());

					// Check whether the User has permission to make the specified modification to the specified Database.
					if (cfg.getUserManagerPtr()->updateAllowed(request->getUser(), cfg.getDatabaseManager(), branches[1], database_config_ptr)) {
						try {
							// push the new config into the DatabaseManager
							cfg.getDatabaseManager().setDatabaseConfig(branches[1], database_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(database_config_ptr);
							throw;
						}
						xmlFreeNodeList(database_config_ptr);

						// respond with the Database's updated configuration
						if (! cfg.getDatabaseManager().writeConfigXML(ss, branches[1]))
							throw DatabaseManager::DatabaseNotFoundException(branches[1]);

					} else {
						xmlFreeNodeList(database_config_ptr);

						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to modify Database " + branches[1] + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}
				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_DELETE) {
					// delete an existing Database

					// Check whether the User has permission to remove the specified Database.
					if (cfg.getUserManagerPtr()->removalAllowed(request->getUser(), cfg.getDatabaseManager(), branches[1])) {
						cfg.getDatabaseManager().removeDatabase(branches[1]);

						// send a 204 (No Content) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_NO_CONTENT);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_NO_CONTENT);
					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to delete Database " + branches[1] + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, PUT, DELETE");
					return;
				}

			} else {
				// Log an error and send a 404 (Not Found) response.
				handleNotFoundRequest(request, tcp_conn);
				return;
			}
			//
			// END DATABASES CONFIG
			//
		} else if (branches.front() == "protocols") {
			//
			// BEGIN PROTOCOLS CONFIG
			//
			if (branches.size() == 1) {
				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {

					// retrieve configuration for all Protocols
					cfg.getProtocolFactory().writeConfigXML(ss);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_POST) {

					// add (create) a new Protocol

					// convert request content into XML configuration info
					xmlNodePtr protocol_config_ptr =
						ProtocolFactory::createProtocolConfig(request->getContent(),
															  request->getContentLength());

					// Check whether the User has permission to create the specified Protocol.
					if (cfg.getUserManagerPtr()->creationAllowed(request->getUser(), cfg.getProtocolFactory(), protocol_config_ptr)) {
						// add the new Protocol to the ProtocolFactory
						std::string protocol_id;
						try {
							protocol_id = cfg.getProtocolFactory().addProtocol(protocol_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(protocol_config_ptr);
							throw;
						}
						xmlFreeNodeList(protocol_config_ptr);

						// send a 201 (created) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_CREATED);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_CREATED);

						// respond with the new Protocol's configuration
						if (! cfg.getProtocolFactory().writeConfigXML(ss, protocol_id))
							throw ProtocolFactory::ProtocolNotFoundException(protocol_id);

					} else {
						xmlFreeNodeList(protocol_config_ptr);

						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to create a Protocol.";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}
				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, POST");
					return;
				}

			} else if (branches[1] == "plugins") {

				// Send a list of all Protocols found in the UI directory

				ConfigManager::writeBeginPionConfigXML(ss);

				// Iterate through all the subdirectories of the Protocol directory (e.g. HTTPProtocol).
				std::string protocol_directory = m_ui_directory + "/plugins/protocols";
				boost::filesystem::directory_iterator end;
				for (boost::filesystem::directory_iterator it(protocol_directory); it != end; ++it) {
					if (boost::filesystem::is_directory(*it)) {
						// Skip directories starting with a '.'.
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
                        if (it->path().filename().string().substr(0, 1) == ".") continue;
                        ss << "<Protocol>"
                        << "<Plugin>" << it->path().filename().string() << "</Plugin>"
                        << "</Protocol>";
#else
                        if (it->path().leaf().substr(0, 1) == ".") continue;
                        ss << "<Protocol>"
                        << "<Plugin>" << it->path().leaf() << "</Plugin>"
                        << "</Protocol>";
#endif 

					}
				}

				ConfigManager::writeEndPionConfigXML(ss);

			} else if (branches.size() == 2) {
				// branches[1] == protocol_id

				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {
					// retrieve an existing Protocol's configuration

					if (! cfg.getProtocolFactory().writeConfigXML(ss, branches[1]))
						throw ProtocolFactory::ProtocolNotFoundException(branches[1]);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_PUT) {
					// update existing Protocol's configuration

					// convert request content into XML configuration info
					xmlNodePtr protocol_config_ptr =
						ProtocolFactory::createProtocolConfig(request->getContent(),
															  request->getContentLength());

					// Check whether the User has permission to make the specified modification to the specified Protocol.
					if (cfg.getUserManagerPtr()->updateAllowed(request->getUser(), cfg.getProtocolFactory(), branches[1], protocol_config_ptr)) {
						try {
							// push the new config into the ProtocolFactory
							cfg.getProtocolFactory().setProtocolConfig(branches[1], protocol_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(protocol_config_ptr);
							throw;
						}
						xmlFreeNodeList(protocol_config_ptr);

						// respond with the Protocol's updated configuration
						if (! cfg.getProtocolFactory().writeConfigXML(ss, branches[1]))
							throw ProtocolFactory::ProtocolNotFoundException(branches[1]);
					} else {
						xmlFreeNodeList(protocol_config_ptr);

						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to modify Protocol " + branches[1] + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}
				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_DELETE) {
					// delete an existing Protocol

					// Check whether the User has permission to remove the specified Protocol.
					if (cfg.getUserManagerPtr()->removalAllowed(request->getUser(), cfg.getProtocolFactory(), branches[1])) {
						cfg.getProtocolFactory().removeProtocol(branches[1]);

						// send a 204 (No Content) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_NO_CONTENT);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_NO_CONTENT);
					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to delete Protocol " + branches[1] + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}
				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, PUT, DELETE");
					return;
				}

			} else {
				// Log an error and send a 404 (Not Found) response.
				handleNotFoundRequest(request, tcp_conn);
				return;
			}
			//
			// END PROTOCOLS CONFIG
			//
		} else if (branches.front() == "reactors") {
			//
			// BEGIN REACTORS CONFIG
			//

			// do not allow multiple UI threads to update reactor config at the same time
			// this protection is assumed by the current implementation of Reactor::ConfigWriteLock
			// because it allows "write locks" to be obtained multiple times by the same thread.
			// Since it doesn't ensure that the thread is the same, we need to try to ensure it here
			// by preventing multiple threads from modifying reactor configuration at the same time
			std::unique_lock<std::mutex> queue_lock(m_reactors_mutex);
			
			if (branches.size() == 1) {
				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {

					// retrieve configuration for all Reactors
					cfg.getReactionEngine().writeConfigXML(ss);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_POST) {

					// add (create) a new Reactor

					// convert request content into XML configuration info
					xmlNodePtr reactor_config_ptr =
						ReactionEngine::createReactorConfig(request->getContent(),
															request->getContentLength());

					// Check whether the User has permission to create the specified Reactor.
					if (cfg.getUserManagerPtr()->creationAllowed(request->getUser(), cfg.getReactionEngine(), reactor_config_ptr)) {
						// add the new Reactor to the ReactionEngine
						std::string reactor_id;
						try {
							reactor_id = cfg.getReactionEngine().addReactor(reactor_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(reactor_config_ptr);
							throw;
						}
						xmlFreeNodeList(reactor_config_ptr);

						// send a 201 (created) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_CREATED);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_CREATED);

						// respond with the new Reactor's configuration
						if (! cfg.getReactionEngine().writeConfigXML(ss, reactor_id))
							throw ReactionEngine::ReactorNotFoundException(reactor_id);

					} else {
						xmlFreeNodeList(reactor_config_ptr);

						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to create the specified Reactor.";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}
				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, POST");
					return;
				}

			} else if (branches[1] == "stats") {

				// send statistics for all Reactors
				cfg.getReactionEngine().writeStatsXML(ss);

			} else if (branches[1] == "plugins") {

				// Send a list of all Reactors found in the UI directory

				ConfigManager::writeBeginPionConfigXML(ss);

				// Iterate through all the subdirectories of the Reactor directory (e.g. collection, processing, storage).
				std::string reactor_directory = m_ui_directory + "/plugins/reactors";
				boost::filesystem::directory_iterator end;
				for (boost::filesystem::directory_iterator it(reactor_directory); it != end; ++it) {
					if (boost::filesystem::is_directory(*it)) {
						// Skip directories starting with a '.'.
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
                        if (it->path().filename().string().substr(0, 1) == ".") continue;
#else
                        if (it->path().leaf().substr(0, 1) == ".") continue;
#endif 

						// Iterate through all the subdirectories of the subdirectory (e.g. LogReactor).
						boost::filesystem::directory_iterator end_2;
						for (boost::filesystem::directory_iterator it2(*it); it2 != end_2; ++it2) {
							if (boost::filesystem::is_directory(*it2)) {
								// Skip directories starting with a '.'.
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
                                if (it2->path().filename().string().substr(0, 1) == ".") continue;
                                ss << "<Reactor>"
                                << "<ReactorType>" << it->path().filename().string() << "</ReactorType>"
                                << "<Plugin>" << it2->path().filename().string() << "</Plugin>"
                                << "</Reactor>";

#else
                                if (it2->path().leaf().substr(0, 1) == ".") continue;
                                ss << "<Reactor>"
                                << "<ReactorType>" << it->path().leaf() << "</ReactorType>"
                                << "<Plugin>" << it2->path().leaf() << "</Plugin>"
                                << "</Reactor>";

#endif 

							}
						}
					}
				}

				ConfigManager::writeEndPionConfigXML(ss);

			} else if (branches.size() == 2) {
				// branches[1] == reactor_id (or possibly workspace_id, for GET or DELETE)

				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {
					// retrieve an existing Reactor's configuration

					if (! cfg.getReactionEngine().writeConfigXML(ss, branches[1])) {
						// No Reactor found with that ID, so maybe it's a Workspace ID.
						if (! cfg.getReactionEngine().writeWorkspaceLimitedConfigXML(ss, branches[1]))
							throw ReactionEngine::ReactorNotFoundException(branches[1]);
					}

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_PUT) {
					// update existing Reactor's configuration

					// convert request content into XML configuration info
					xmlNodePtr reactor_config_ptr =
						ReactionEngine::createReactorConfig(request->getContent(),
															request->getContentLength());

					// Check whether the User has permission to make the specified modification to the specified Reactor.
					if (cfg.getUserManagerPtr()->updateAllowed(request->getUser(), cfg.getReactionEngine(), branches[1], reactor_config_ptr)) {
						try {
							// push the new config into the ReactionEngine
							cfg.getReactionEngine().setReactorConfig(branches[1], reactor_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(reactor_config_ptr);
							throw;
						}
						xmlFreeNodeList(reactor_config_ptr);

						// respond with the Reactor's updated configuration
						if (! cfg.getReactionEngine().writeConfigXML(ss, branches[1]))
							throw ReactionEngine::ReactorNotFoundException(branches[1]);
					} else {
						xmlFreeNodeList(reactor_config_ptr);

						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to modify Reactor " + branches[1] + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_DELETE) {
					// delete an existing Reactor (or Reactors)

					// Check whether the User has permission to remove the specified Reactor(s).
					if (cfg.getUserManagerPtr()->removalAllowed(request->getUser(), cfg.getReactionEngine(), branches[1])) {
						if (cfg.getReactionEngine().hasPlugin(branches[1])) {
							cfg.getReactionEngine().removeReactor(branches[1]);
						} else if (cfg.getReactionEngine().hasWorkspace(branches[1])) {
							cfg.getReactionEngine().removeReactorsFromWorkspace(branches[1]);
						} else
							throw ReactionEngine::ReactorNotFoundException(branches[1]);

						// send a 204 (No Content) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_NO_CONTENT);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_NO_CONTENT);
					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to delete Reactors with the specified ID or Workspace ID: " + branches[1] + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}
				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, PUT, DELETE");
					return;
				}

			} else if (branches.size() == 3) {
				// branches[1] == reactor_id

				if (branches[2] == "start") {

					// start a Reactor
					cfg.getReactionEngine().startReactor(branches[1]);

					// respond by sending all the Reactor stats
					cfg.getReactionEngine().writeStatsXML(ss);

				} else if (branches[2] == "stop") {

					// stop a Reactor
					cfg.getReactionEngine().stopReactor(branches[1]);

					// respond by sending all the Reactor stats
					cfg.getReactionEngine().writeStatsXML(ss);

				} else if (branches[2] == "move") {
					if (request->getMethod() == HTTPTypes::REQUEST_METHOD_PUT) {
						// update the specified Reactor's configuration (only UI location settings)

						// convert request content into XML configuration info
						xmlNodePtr reactor_config_ptr =
							ReactionEngine::createReactorConfig(request->getContent(),
																request->getContentLength());

						try {
							// push the new config settings into the ReactionEngine
							cfg.getReactionEngine().setReactorLocation(branches[1], reactor_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(reactor_config_ptr);
							throw;
						}
						xmlFreeNodeList(reactor_config_ptr);

						// respond with the Reactor's updated configuration
						if (! cfg.getReactionEngine().writeConfigXML(ss, branches[1]))
							throw ReactionEngine::ReactorNotFoundException(branches[1]);
					} else {
						// Log an error and send a 405 (Method Not Allowed) response.
						handleMethodNotAllowed(request, tcp_conn, "PUT");
						return;
					}
				} else {
					// Log an error and send a 404 (Not Found) response.
					handleNotFoundRequest(request, tcp_conn);
					return;
				}
			} else {
				// Log an error and send a 404 (Not Found) response.
				handleNotFoundRequest(request, tcp_conn);
				return;
			}
			//
			// END REACTORS CONFIG
			//
		} else if (branches.front() == "connections") {
			//
			// BEGIN CONNECTIONS CONFIG
			//
			if (branches.size() == 1) {
				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {

					// retrieve configuration for all Reactor connections
					cfg.getReactionEngine().writeConnectionsXML(ss);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_POST) {

					// create a new Reactor connection

					// convert request content into XML configuration info
					xmlNodePtr connection_config_ptr = 
						ReactionEngine::createConnectionConfig(request->getContent(), request->getContentLength());

					// Check whether the User has permission to create the specified Reactor connection.
					if (cfg.getUserManagerPtr()->creationAllowed(request->getUser(), cfg.getReactionEngine(), connection_config_ptr)) {
						// create a new Reactor connection
						std::string connection_id = cfg.getReactionEngine().addReactorConnection(connection_config_ptr);

						// send a 201 (Created) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_CREATED);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_CREATED);

						// respond with the new connection's configuration
						cfg.getReactionEngine().writeConnectionsXML(ss, connection_id);

					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to create the specified Reactor connection.";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}
				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, POST");
					return;
				}

			} else {
				// branches[1] == connection_id

				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {

					// retrieve configuration for specific Reactor connections
					cfg.getReactionEngine().writeConnectionsXML(ss, branches[1]);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_DELETE) {

					// remove an existing Reactor connection

					// Check whether the User has permission to remove the specified connection.
					if (cfg.getUserManagerPtr()->removalAllowed(request->getUser(), cfg.getReactionEngine(), branches[1])) {
						cfg.getReactionEngine().removeReactorConnection(branches[1]);

						// send a 204 (No Content) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_NO_CONTENT);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_NO_CONTENT);
					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to delete connection " + branches[1] + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}
				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, DELETE");
					return;
				}
			}
			//
			// END CONNECTIONS CONFIG
			//
		} else if (branches.front() == "workspaces") {
			//
			// BEGIN WORKSPACES CONFIG
			//
			if (branches.size() == 1) {
				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {

					// retrieve configuration for all Reactor Workspaces
					cfg.getReactionEngine().writeWorkspacesXML(ss);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_POST) {

					// add (create) a new Reactor Workspace

					// Check whether the User has permission to create a Reactor Workspace.
					if (cfg.getUserManagerPtr()->creationAllowed(request->getUser(), cfg.getReactionEngine(), NULL)) {
						// create a new Reactor Workspace
						std::string workspace_id = cfg.getReactionEngine().addWorkspace(request->getContent(),
																						request->getContentLength());
						// send a 201 (Created) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_CREATED);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_CREATED);

						// respond with the new Reactor Workspace's configuration
						cfg.getReactionEngine().writeWorkspaceXML(ss, workspace_id);

					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to create a Reactor Workspace.";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}
				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, POST");
					return;
				}

			} else {
				// branches[1] == workspace_id

				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {

					// retrieve configuration for the specified Reactor Workspace
					cfg.getReactionEngine().writeWorkspaceXML(ss, branches[1]);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_PUT) {
					// update existing Reactor Workspace's configuration

					// Check whether the User has permission to modify the specified Workspace.
					if (cfg.getUserManagerPtr()->updateAllowed(request->getUser(), cfg.getReactionEngine(), branches[1], NULL)) {
						// push the new config into the ReactionEngine
						cfg.getReactionEngine().setWorkspaceConfig(branches[1], request->getContent(),
																   request->getContentLength());

						// respond with the Workspace's updated configuration
						cfg.getReactionEngine().writeWorkspaceXML(ss, branches[1]);

					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to modify Workspace " + branches[1] + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_DELETE) {

					// remove an existing empty Reactor Workspace

					// Check whether the User has permission to remove the specified Workspace.
					if (cfg.getUserManagerPtr()->removalAllowed(request->getUser(), cfg.getReactionEngine(), branches[1])) {
						cfg.getReactionEngine().removeWorkspace(branches[1]);

						// send a 204 (No Content) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_NO_CONTENT);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_NO_CONTENT);
					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to delete Workspace " + branches[1] + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}

				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, PUT, DELETE");
					return;
				}
			}
			//
			// END WORKSPACES CONFIG
			//
		} else if (branches.front() == "servers") {
			//
			// BEGIN SERVERS CONFIG
			//
			if (branches.size() == 1) {
				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {

					// retrieve configuration for all Servers
					cfg.getServiceManager().writeServersXML(ss);

				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET");
					return;
				}
			} else if (branches.size() == 2) {
				// branches[1] == server_id

				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {
					// retrieve an existing Server's configuration
					if (! cfg.getServiceManager().writeServerXML(ss, branches[1]))
						throw PionPlugin::PluginNotFoundException(branches[1]);

				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET");
					return;
				}

			} else {
				// Log an error and send a 404 (Not Found) response.
				handleNotFoundRequest(request, tcp_conn);
				return;
			}
			//
			// END SERVERS CONFIG
			//
		} else if (branches.front() == "services") {
			//
			// BEGIN SERVICES CONFIG
			//
			if (branches.size() == 1) {
				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {

					// retrieve configuration for all PlatformServices
					cfg.getServiceManager().writeConfigXML(ss);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_POST) {

					// add (create) a new PlatformService

					// convert request content into XML configuration info
					xmlNodePtr service_config_ptr =
						ServiceManager::createPlatformServiceConfig(request->getContent(),
																	request->getContentLength());

					// Check whether the User has permission to create the specified PlatformService.
					if (cfg.getUserManagerPtr()->creationAllowed(request->getUser(), cfg.getServiceManager(), service_config_ptr)) {
						// add the new PlatformService to the ServiceManager
						std::string service_id;
						try {
							service_id = cfg.getServiceManager().addPlatformService(service_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(service_config_ptr);
							throw;
						}
						xmlFreeNodeList(service_config_ptr);

						// send a 201 (created) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_CREATED);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_CREATED);

						// respond with the new PlatformService's configuration
						if (! cfg.getServiceManager().writeConfigXML(ss, service_id))
							throw ServiceManager::PlatformServiceNotFoundException(service_id);

					} else {
						xmlFreeNodeList(service_config_ptr);

						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to create the specified PlatformService.";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}
				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, POST");
					return;
				}

			} else if (branches[1] == "plugins") {

				// Send a list of all Services found in the UI directory

				ConfigManager::writeBeginPionConfigXML(ss);

				// Iterate through all the subdirectories of the Service directory.
				std::string service_directory = m_ui_directory + "/plugins/services";
				boost::filesystem::directory_iterator end;
				for (boost::filesystem::directory_iterator it(service_directory); it != end; ++it) {
					if (boost::filesystem::is_directory(*it)) {
						// Skip directories starting with a '.'.
# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
                        if (it->path().filename().string().substr(0, 1) == ".") continue;
#else
                        if (it->path().leaf().substr(0, 1) == ".") continue;
#endif 

# if defined(BOOST_FILESYSTEM_VERSION) && BOOST_FILESYSTEM_VERSION >= 3
						ss << "<Service>"
						   << "<Plugin>" << it->path().filename().string() << "</Plugin>"
						   << "</Service>";
#else
                        ss << "<Service>"
                        << "<Plugin>" << it->path().leaf() << "</Plugin>"
                        << "</Service>";
#endif 
					}
				}

				ConfigManager::writeEndPionConfigXML(ss);

			} else if (branches.size() == 2) {
				// branches[1] == service_id

				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {
					// retrieve an existing PlatformService's configuration

					if (! cfg.getServiceManager().writeConfigXML(ss, branches[1]))
						throw ServiceManager::PlatformServiceNotFoundException(branches[1]);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_DELETE) {
					// delete an existing PlatformService

					// Check whether the User has permission to remove the specified PlatformService.
					if (cfg.getUserManagerPtr()->removalAllowed(request->getUser(), cfg.getServiceManager(), branches[1])) {
						cfg.getServiceManager().removePlatformService(branches[1]);

						// send a 204 (No Content) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_NO_CONTENT);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_NO_CONTENT);
					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to delete PlatformService " + branches[1] + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}
				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, DELETE");
					return;
				}

			} else {
				// Log an error and send a 404 (Not Found) response.
				handleNotFoundRequest(request, tcp_conn);
				return;
			}
			//
			// END SERVICES CONFIG
			//
		} else if (branches.front() == "users") {
			//
			// BEGIN USERS CONFIG
			//
			if (branches.size() == 1) {
				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {

					// retrieve configuration for all Users
					cfg.getUserManagerPtr()->writeConfigXML(ss);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_POST) {

					// add (create) a new user

					// convert request content into XML configuration info
					std::string user_id;
					xmlNodePtr user_config_ptr = UserManager::createUserConfig(user_id,
																			   request->getContent(),
																			   request->getContentLength());

					// Check whether the User has permission to create new Users.
					if (cfg.getUserManagerPtr()->creationAllowed(request->getUser(), *cfg.getUserManagerPtr(), NULL)) {
						// add the new User to the UserManager
						try {
							user_id = cfg.getUserManagerPtr()->addUser(user_id, user_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(user_config_ptr);
							throw;
						}
						xmlFreeNodeList(user_config_ptr);

						// send a 201 (Created) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_CREATED);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_CREATED);

						// respond with the new User's configuration
						if (! cfg.getUserManagerPtr()->writeConfigXML(ss, user_id))
							throw UserManager::UserNotFoundException(user_id);
					} else {
						xmlFreeNodeList(user_config_ptr);

						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to create a User.";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}
				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, POST");
					return;
				}

			} else {
				// branches[1] == user_id

				if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {

					// retrieve configuration for specific User
					cfg.getUserManagerPtr()->writeConfigXML(ss, branches[1]);

				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_PUT) {
					// update existing User's configuration

					// convert request content into XML configuration info
					std::string user_id; // should be assigned empty string on the next line, ignored in any case
					xmlNodePtr user_config_ptr = UserManager::createUserConfig(user_id,
																			   request->getContent(),
																			   request->getContentLength());

					// Check whether the User has permission to modify the specified User.
					if (cfg.getUserManagerPtr()->updateAllowed(request->getUser(), *cfg.getUserManagerPtr(), branches[1], NULL)) {
						// push the new config into the UserManager
						try {
							cfg.getUserManagerPtr()->setUserConfig(branches[1], user_config_ptr);
						} catch (std::exception&) {
							xmlFreeNodeList(user_config_ptr);
							throw;
						}
						xmlFreeNodeList(user_config_ptr);

						// respond with the new User's configuration
						if (! cfg.getUserManagerPtr()->writeConfigXML(ss, branches[1]))
							throw UserManager::UserNotFoundException(branches[1]);

					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to modify User " + branches[1] + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}
				} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_DELETE) {

					// remove an existing User

					// Check whether the User has permission to remove the specified User.
					if (cfg.getUserManagerPtr()->removalAllowed(request->getUser(), *cfg.getUserManagerPtr(), branches[1])) {
						cfg.getUserManagerPtr()->removeUser(branches[1]);

						// send a 204 (No Content) response
						response_ptr->setStatusCode(HTTPTypes::RESPONSE_CODE_NO_CONTENT);
						response_ptr->setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_NO_CONTENT);
					} else {
						// Log an error and send a 403 (Forbidden) response.
						std::string error_msg = "User doesn't have permission to delete User " + branches[1] + ".";
						handleForbiddenRequest(request, tcp_conn, error_msg);
						return;
					}
				} else {
					// Log an error and send a 405 (Method Not Allowed) response.
					handleMethodNotAllowed(request, tcp_conn, "GET, PUT, DELETE");
					return;
				}
			}
			
			// Remove any passwords before logging results
			std::string clean_content(request->getContent());
			std::string result_fmt("<Password>...</Password>");
			boost::regex find_password_rx("<Password>(.*?)</Password>");
			clean_content = boost::regex_replace(clean_content, find_password_rx, result_fmt);
			request->setContent(clean_content);
			
			//
			// END USERS CONFIG
			//
			
		} else if (branches.front() == "plugins") {

			// Send a list of all Plugins found in the Plugin directories.

			ConfigManager::writeBeginPionConfigXML(ss);

			std::vector<std::string> plugins;
			PionPlugin::getAllPluginNames(plugins);
			for (std::vector<std::string>::iterator i = plugins.begin(); i != plugins.end(); ++i) {
				ss << "<Plugin>" << *i << "</Plugin>";
			}

			ConfigManager::writeEndPionConfigXML(ss);

		} else if (branches.front() == "dbengines") {

			// Send configuration of all database engines.

			cfg.getDatabaseManager().writeDatabaseEnginesXML(ss);

		} else {
			// Log an error and send a 404 (Not Found) response.
			handleNotFoundRequest(request, tcp_conn);
			return;
		}
	} catch (std::exception&) {
		// This exception will be caught by HTTPServer::handleRequest(), which will log the error and
		// send a 500 (Server Error) response, so all we need to do here is log the request, if needed.
		logRequestIfPotentialConfigChange(request, HTTPTypes::RESPONSE_CODE_SERVER_ERROR);
		throw;
	}

	logRequestIfPotentialConfigChange(request, response_ptr->getStatusCode());

	// prepare the writer object for XML output
	HTTPResponseWriterPtr writer(HTTPResponseWriter::create(tcp_conn, response_ptr,
															std::bind(&TCPConnection::finish, tcp_conn)));
	writer->getResponse().setContentType(HTTPTypes::CONTENT_TYPE_XML);

	// send the response
	writer->write(ss.str());
	writer->send();
}

void ConfigService::handleBadRequest(HTTPRequestPtr& request, TCPConnectionPtr& tcp_conn, const std::string& error_msg) {
	PlatformService::handleBadRequest(request, tcp_conn, error_msg);
	logRequestIfPotentialConfigChange(request, HTTPTypes::RESPONSE_CODE_BAD_REQUEST);
}

void ConfigService::handleForbiddenRequest(HTTPRequestPtr& request, TCPConnectionPtr& tcp_conn, const std::string& error_msg) {
	PlatformService::handleForbiddenRequest(request, tcp_conn, error_msg);
	logRequestIfPotentialConfigChange(request, HTTPTypes::RESPONSE_CODE_FORBIDDEN);
}

void ConfigService::handleNotFoundRequest(HTTPRequestPtr& request, TCPConnectionPtr& tcp_conn) {
	PlatformService::handleNotFoundRequest(request, tcp_conn);
	logRequestIfPotentialConfigChange(request, HTTPTypes::RESPONSE_CODE_NOT_FOUND);
}

void ConfigService::handleMethodNotAllowed(HTTPRequestPtr& request, TCPConnectionPtr& tcp_conn, const std::string& allowed_methods) {
	PlatformService::handleMethodNotAllowed(request, tcp_conn, allowed_methods);
	logRequestIfPotentialConfigChange(request, HTTPTypes::RESPONSE_CODE_METHOD_NOT_ALLOWED);
}

void ConfigService::logRequestIfPotentialConfigChange(HTTPRequestPtr& request, unsigned int status) {
	// In ConfigService, all GET requests that are handled leave the configuration unchanged.
	if (request->getMethod() != HTTPTypes::REQUEST_METHOD_GET) {
		std::string content(request->getContent(), request->getContentLength()); // Could be empty.
		std::string user(request->getUser() ? request->getUser()->getUsername() : "NONE");
		PION_LOG_INFO(m_config_logger, request->getMethod() 
										<< ' ' << status
										<< ' ' << user
										<< ' ' << request->getResource() << ' ' << content);
	}
}

}	// end namespace plugins
}	// end namespace pion


/// creates new ConfigService objects
extern "C" PION_PLUGIN_API pion::server::PlatformService *pion_create_ConfigService(void) {
	return new pion::plugins::ConfigService();
}

/// destroys ConfigService objects
extern "C" PION_PLUGIN_API void pion_destroy_ConfigService(pion::plugins::ConfigService *service_ptr) {
	delete service_ptr;
}
