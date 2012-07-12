// ------------------------------------------------------------------------
// Pion is a development platform for building Reactors that process Events
// ------------------------------------------------------------------------
// Copyright (C) 2007-2010 Atomic Labs, Inc.  (http://www.atomiclabs.com)
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
#include <sstream>
#include <pion/PionId.hpp>
#include <pion/PionAlgorithms.hpp>
#include <pion/net/HTTPResponse.hpp>
#include <pion/net/HTTPResponseWriter.hpp>
#include "pion/platform/Event.hpp"
#include "PlatformConfig.hpp"
#include "MonitorService.hpp"

using namespace pion::net;
using namespace pion::server;
using namespace pion::platform;


namespace pion {		// begin namespace pion
namespace plugins {		// begin namespace plugins

const std::string dtd = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
const std::string urnvocab("urn:vocab:");		// shorthand notation
const unsigned URN_VOCAB = urnvocab.length();	// length("urn:vocab:") clicstream

const unsigned				MonitorService::WRITERS = 10;
const std::string			MonitorService::MONITOR_SERVICE_PERMISSION_TYPE = "MonitorService";


void MonitorWriter::writeEvent(EventPtr e)
{
	PION_LOG_DEBUG(m_logger, "Received event via " << getConnectionId());
	if (e.get() == NULL) {
		// Reactor is being removed -> close the connection
		// note that the ReactionEngine will remove the connection for us
		// keep the data, in case user wants to still watch it
		PION_LOG_DEBUG(m_logger, "Reactor removed for " << getConnectionId());
		stop(false);
	} else if (m_age + boost::posix_time::seconds(120) < boost::posix_time::second_clock::local_time()) {
		// It's been over two minutes since last call -- detach & clear events
		PION_LOG_DEBUG(m_logger, "Timing-out idle monitor for " << getConnectionId());
		stop(true, true);
	} else {
		try {
			// lock the mutex to ensure that only one Event is sent at a time
			std::unique_lock<std::mutex> writer_lock(m_mutex);
			const Vocabulary::TermRef tref = e->getType();
			m_events_seen.insert(tref);
			// if this event type is NOT found in filtered_events, then add it to the stream
			if (m_filtered_events.find(tref) == m_filtered_events.end() && m_stopped == false) {
				// Add latest event to end of circular buffer
				m_event_buffer.push_back(e);
				++m_event_counter;

				// If we're not scrolling, and buffer is full, then disconnect from the feed
				if (!m_scroll && m_event_buffer.full()) {
					PION_LOG_DEBUG(m_logger, "Event buffer full for " << getConnectionId());
					writer_lock.unlock();	// stop() needs to acquire mutex
					stop();
				}
			}
		} catch (std::exception& ex) {
			// stop sending Events if we encounter an exception
			PION_LOG_WARN(m_logger, "Error sending event to " << getConnectionId() << ":" << ex.what());
			stop();
		}
	}
}
	
void MonitorWriter::start(const HTTPTypes::QueryParams& qp)
{
	setQP(qp);	// Configure settings based on query parameters

	// lock the mutex to ensure that the HTTP response is sent first
	std::lock_guard<std::mutex> writer_lock(m_mutex);

	// tell the ReactionEngine to start sending us Events
	Reactor::EventHandler event_handler(std::bind(&MonitorWriter::writeEvent,
													shared_from_this(), std::placeholders::_1));
	m_reaction_engine.addTempConnectionOut(getReactorId(), getConnectionId(),
										   "MonitorService", event_handler);

	PION_LOG_INFO(m_logger, "Opened new output feed to " << getConnectionId());
}

void MonitorWriter::SerializeXML(pion::platform::Vocabulary::TermRef tref,
	const pion::platform::Event::ParameterValue& value, std::ostream& xml, TermCol& cols)
{
	// NOTE: assumes mutex is locked already
	
	if (tref > m_vocab_ptr->size())		// sanity check
		tref = Vocabulary::UNDEFINED_TERM_REF;

	// Add to set of seen terms
	m_terms_seen.insert(tref);

	// If we're in opt-in mode, check that term is in selected set
	if (m_hide_all) {
		if (m_show_terms.find(tref) == m_show_terms.end())
			return;
	} else {
		// Don't add suppressed terms
		if (m_suppressed_terms.find(tref) != m_suppressed_terms.end())
			return;
	}

	const Vocabulary::Term& t((*m_vocab_ptr)[tref]);	// term corresponding with Event parameter
	// Have we seen this tref (column) yet?
	if (t.term_type == Vocabulary::TYPE_OBJECT)
		tref = Vocabulary::UNDEFINED_TERM_REF;			// Mask OBJECT to undef
	TermCol::iterator i = cols.find(tref);
	if (i == cols.end()) {
		cols[tref] = cols.size() + 1;
		i = cols.find(tref);
	}
	// Don't serialize the non-serializable
	if (t.term_type == Vocabulary::TYPE_NULL)
		xml << "<C" << i->second << "\\>";
	if (t.term_type == Vocabulary::TYPE_OBJECT)
		xml << "<C" << i->second << '>' << t.term_id.substr(URN_VOCAB) << "</C" << i->second << '>';
	else {
		std::string tmp;		// tmp storage for values
		xml << "<C" << i->second << '>'
			<< ConfigManager::xml_encode(Event::write(tmp, value, t).substr(0, m_truncate))
			<< "</C" << i->second << '>';
	}
}

std::string MonitorWriter::getStatus(const HTTPTypes::QueryParams& qp)
{
	setQP(qp);	// Configure settings based on query parameters

	// Map for termref -> Cnn index, we'll use it for building the guide
	TermCol col_map;
	if (m_hide_all)	// In opt-in mode, show all show_terms columns
		for (TermRefSet::const_iterator i = m_show_terms.begin(); i != m_show_terms.end(); ++i)
			col_map[*i] = col_map.size() + 1;

    std::ostringstream preamble;

	// traverse through all events in buffer
	std::ostringstream xml;
	unsigned size;
	{
		std::lock_guard<std::mutex> writer_lock(m_mutex);
		size = m_event_buffer.size();
		for (boost::circular_buffer<pion::platform::EventPtr>::const_iterator i = m_event_buffer.begin(); i != m_event_buffer.end(); i++) {
			// traverse through all terms in event
			const Vocabulary::TermRef tref = (*i)->getType();
			// if this event type is NOT found in filtered_events, then add it to the stream
			if (m_filtered_events.find(tref) == m_filtered_events.end()) {
				const Vocabulary::Term& et((*m_vocab_ptr)[tref]);	// term corresponding with Event parameter
				xml << "<Event><C0>" << et.term_id.substr(URN_VOCAB) << "</C0>";
				(*i)->for_each(std::bind(&MonitorWriter::SerializeXML,
					this, std::placeholders::_1, std::placeholders::_2, std::ref(xml), std::ref(col_map)));
				xml << "</Event>";
				if (xml.tellp() > 1000000) {	// FIXME: Max limit of 1MB (for now)
					preamble << "<Truncated>" << xml.tellp() << "</Truncated>";
					break;
 				}
			}
		}
	}
	std::ostringstream prefix;
	prefix << "<C0>Event Type</C0>";
	for (TermCol::const_iterator i = col_map.begin(); i != col_map.end(); i++)
		if (i->second == Vocabulary::UNDEFINED_TERM_REF)
			prefix << "<C" << i->second << ">type</C" << i->second << '>';
		else {
			const Vocabulary::Term& t((*m_vocab_ptr)[i->first]);
			prefix << "<C" << i->second << '>' << t.term_id.substr(URN_VOCAB) << "</C" << i->second << '>';
		}

	std::ostringstream seen;
	seen << "<TermsSeen>";
	for (TermRefSet::const_iterator i = m_terms_seen.begin(); i != m_terms_seen.end(); i++)
		if (*i != Vocabulary::UNDEFINED_TERM_REF) {
			const Vocabulary::Term& t((*m_vocab_ptr)[*i]);
			if (i != m_terms_seen.begin())
				seen << ',';
			seen << t.term_id.substr(URN_VOCAB);
		}
	seen << "</TermsSeen><EventsSeen>";
	for (TermRefSet::const_iterator i = m_events_seen.begin(); i != m_events_seen.end(); i++)
		if (*i != Vocabulary::UNDEFINED_TERM_REF) {
			const Vocabulary::Term& t((*m_vocab_ptr)[*i]);
			if (i != m_events_seen.begin())
				seen << ',';
			seen << t.term_id.substr(URN_VOCAB);
		}
	seen << "</EventsSeen>";

	preamble << "<Monitoring>" << m_reactor_id << "</Monitoring><Running>" << (m_stopped ? "Stopped" : "Collecting")
			<< "</Running><EventCounter>" << m_event_counter << "</EventCounter><ChangeCounter>" << m_change_counter
			<< "</ChangeCounter><Collected>" << size << "</Collected><Capacity>" << m_event_buffer.capacity()
			<< "</Capacity><Truncating>" << m_truncate << "</Truncating><Scroll>" << (m_scroll ? "true" : "false")
			<< "</Scroll>" << seen.str();
	return "<Status>" + preamble.str() + "<ColSet>" + prefix.str() + "</ColSet><Events>" + xml.str() + "</Events></Status>";
}

void MonitorWriter::setQP(const HTTPTypes::QueryParams& qp)
{
	std::lock_guard<std::mutex> writer_lock(m_mutex);

	setAge();

	if (qp.empty())
		return;

	// We'll assume that any QP's will inflict a change
	++m_change_counter;

    HTTPTypes::QueryParams::const_iterator qpi = qp.find("events");
    if (qpi != qp.end()) {
        unsigned events = boost::lexical_cast<boost::uint32_t>(qpi->second);
		if (events != m_size) {
			// Remove (if necessary) first events, change capacity to match new
			m_event_buffer.rset_capacity(m_size = events);
		}
	}

    qpi = qp.find("truncate");
    if (qpi != qp.end())
		m_truncate = boost::lexical_cast<boost::uint32_t>(qpi->second);

    qpi = qp.find("scroll");
    if (qpi != qp.end())
		m_scroll = qpi->second == "true";

	qpi = qp.find("opt");
	if (qpi != qp.end())
		m_hide_all = (qpi->second == "in");

	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	static const boost::char_separator<char> sep(",");

	qpi = qp.find("show");
	if (qpi != qp.end()) {
		std::string str(algo::url_decode(qpi->second));
		tokenizer tokens(str, sep);
		for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter)
			m_show_terms.insert(m_vocab_ptr->findTerm(urnvocab + *tok_iter));
	}

	qpi = qp.find("unshow");
	if (qpi != qp.end()) {
		std::string str(algo::url_decode(qpi->second));
		tokenizer tokens(str, sep);
		for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter)
			m_show_terms.erase(m_vocab_ptr->findTerm(urnvocab + *tok_iter));
	}

	qpi = qp.find("hide");
	if (qpi != qp.end()) {
		std::string str(algo::url_decode(qpi->second));
		tokenizer tokens(str, sep);
		for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter)
			m_suppressed_terms.insert(m_vocab_ptr->findTerm(urnvocab + *tok_iter));
	}

	qpi = qp.find("unhide");
	if (qpi != qp.end()) {
		std::string str(algo::url_decode(qpi->second));
		tokenizer tokens(str, sep);
		for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter)
			m_suppressed_terms.erase(m_vocab_ptr->findTerm(urnvocab + *tok_iter));
	}

	qpi = qp.find("filter");
	if (qpi != qp.end()) {
		std::string str(algo::url_decode(qpi->second));
		tokenizer tokens(str, sep);
		for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter) {
			const Vocabulary::TermRef tref = m_vocab_ptr->findTerm(urnvocab + *tok_iter);
			m_filtered_events.insert(tref);	// Add event type to filtered events
			// Remove all events of the filtered type from the buffer
			EventBuffer::iterator i = m_event_buffer.begin();
			while (i != m_event_buffer.end())
				if ((*i)->getType() == tref)
					i = m_event_buffer.erase(i);	// erase returns NEXT elemeent, or end()
				else
					++i;
		}
	}

	qpi = qp.find("unfilter");
	if (qpi != qp.end()) {
		std::string str(algo::url_decode(qpi->second));
		tokenizer tokens(str, sep);
		for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter)
			m_filtered_events.erase(m_vocab_ptr->findTerm(urnvocab + *tok_iter));
	}
}


// MonitorService member functions

void MonitorService::operator()(HTTPRequestPtr& request, TCPConnectionPtr& tcp_conn)
{
	// split out the path branches from the HTTP request
	PathBranches branches;
	splitPathBranches(branches, request->getResource());
	
	// make sure that there are two extra path branches in the request
	if (branches.size() < 2) {
		// Log an error and send a 404 (Not Found) response.
		handleNotFoundRequest(request, tcp_conn);
		return;
	}

	//bool allowed = getConfig().getUserManagerPtr()->accessAllowed(request->getUser(), *this);
	//if (! allowed) {
	//	// Log an error and send a 403 (Forbidden) response.
	//	std::string error_msg = "User doesn't have permission for Monitor Service.";
	//	handleForbiddenRequest(request, tcp_conn, error_msg);
	//	return;
	//}

	// get the start/stop verb
	const std::string verb(branches[0]);

	// only allow one thread to make changes at a time
	std::lock_guard<std::mutex> service_lock(m_mutex);

	// check the request method to determine if we should read or write Events
	if (request->getMethod() == HTTPTypes::REQUEST_METHOD_GET) {

		HTTPResponseWriterPtr response_writer(HTTPResponseWriter::create(tcp_conn, *request,
										  std::bind(&TCPConnection::finish, tcp_conn)));

		// Process QueryParameters in start & status
		const HTTPTypes::QueryParams qp = request->getQueryParams();

		// request made to receive a stream of Events
		if (verb == "start") {
			// get the reactor_id from the first path branch
			const std::string reactor_id(branches[1]);
			if (reactor_id.empty() || !getConfig().getReactionEngine().hasPlugin(reactor_id)) {
				// Log an error and send a 404 (Not Found) response.
				handleNotFoundRequest(request, tcp_conn);
				return;
			}

			// Check whether the User has permission for this Reactor.
			bool reactor_allowed = getConfig().getUserManagerPtr()->accessAllowed(request->getUser(), getConfig().getReactionEngine(), reactor_id);
			if (! reactor_allowed) {
				// Log an error and send a 403 (Forbidden) response.
				std::string error_msg = "User doesn't have permission for Reactor " + reactor_id + ".";
				handleForbiddenRequest(request, tcp_conn, error_msg);
				return;
			}

			unsigned slot, oldest;
			boost::posix_time::ptime oldest_age = boost::date_time::not_a_date_time;
			// Try to find an empty slot, also find the oldest
			for (oldest = slot = 0; slot < m_writers.size(); slot++) {
				if (!m_writers[slot]) break;	// If you find an empty one...
				if (m_writers[slot]->getAge() == boost::date_time::not_a_date_time) break;	// ...or a dead one
				if (oldest_age == boost::date_time::not_a_date_time || m_writers[slot]->getAge() < oldest_age) {
					oldest = slot;
					oldest_age = m_writers[slot]->getAge();
				}
			}
			// If no empty slots, clear oldest
			if (slot == m_writers.size())
				slot = oldest;
			VocabularyPtr vocab_ptr(getConfig().getReactionEngine().getVocabulary());
			// create a MonitorWriter object that will be used to send Events
			m_writers[slot].reset(new MonitorWriter(getConfig().getReactionEngine(), vocab_ptr, reactor_id, 1000, true, m_logger));
			m_writers[slot]->start(qp);
			std::ostringstream xml;
			xml << dtd << "<MonitorService>" << slot << "</MonitorService>";
			response_writer->write(xml.str());
			PION_LOG_INFO(m_logger, "start request for reactor " << reactor_id << " (slot " << slot << ")");
		} else {
			// use local array to identify the reactor_id
			// possibly a secondary id, for multiple instances per reactor_id
			unsigned slot = boost::lexical_cast<boost::uint32_t>(branches[1]);

			if (slot < m_writers.size() && m_writers[slot]) {
				// Check whether the User has permission for this Reactor.
				const std::string reactor_id = m_writers[slot]->getReactorId();
				bool reactor_allowed = getConfig().getUserManagerPtr()->accessAllowed(request->getUser(), getConfig().getReactionEngine(), reactor_id);
				if (! reactor_allowed) {
					// Log an error and send a 403 (Forbidden) response.
					std::string error_msg = "User doesn't have permission for Reactor " + reactor_id + ".";
					handleForbiddenRequest(request, tcp_conn, error_msg);
					return;
				}

				if (verb == "status") {
					PION_LOG_DEBUG(m_logger, "status request for slot " << slot);
					// get the status for this capture...
					std::string response = m_writers[slot]->getStatus(qp);
					response_writer->write(dtd + response);
				} else if (verb == "stop") {
					PION_LOG_DEBUG(m_logger, "stop request for slot " << slot);
					m_writers[slot]->stop();
					std::ostringstream xml;
					xml << dtd << "<MonitorService action=\"stopped\">" << slot << "</MonitorService>";
					response_writer->write(xml.str());
				} else if (verb == "delete") {
					PION_LOG_DEBUG(m_logger, "delete request for slot " << slot);
					m_writers[slot]->stop();
					m_writers[slot].reset();
					std::ostringstream xml;
					xml << dtd << "<MonitorService action=\"deleted\">" << slot << "</MonitorService>";
					response_writer->write(xml.str());
				} else if (verb == "ping") {
					PION_LOG_DEBUG(m_logger, "ping request for slot " << slot);
					std::ostringstream xml;
					xml << dtd << "<MonitorService action=\"ping\">" << slot << "</MonitorService>";
					response_writer->write(xml.str());
					m_writers[slot]->setAge();
				}
			} else {
				response_writer->write(dtd + "<Error>Invalid slot defined</Error>");
				PION_LOG_ERROR(m_logger, "invalid slot defined: " << slot);
			}
		}

		response_writer->send();

	} else if (request->getMethod() == HTTPTypes::REQUEST_METHOD_HEAD) {
		
		// request is just checking if the reactor is valid -> return OK
		HTTPResponseWriterPtr response_writer(HTTPResponseWriter::create(tcp_conn, *request,
											  std::bind(&TCPConnection::finish, tcp_conn)));
		response_writer->send();

	} else {
		// Log an error and send a 405 (Method Not Allowed) response.
		handleMethodNotAllowed(request, tcp_conn, "GET, HEAD");
	}	
}


}	// end namespace plugins
}	// end namespace pion


/// creates new MonitorService objects
extern "C" PION_PLUGIN_API pion::server::PlatformService *pion_create_MonitorService(void) {
	return new pion::plugins::MonitorService();
}

/// destroys MonitorService objects
extern "C" PION_PLUGIN_API void pion_destroy_MonitorService(pion::plugins::MonitorService *service_ptr) {
	delete service_ptr;
}
