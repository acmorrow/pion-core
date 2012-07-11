// ------------------------------------------------------------------------
// Pion is a development platform for building Reactors that process Events
// ------------------------------------------------------------------------
// Copyright (C) 2007-2011 Atomic Labs, Inc.  (http://www.atomiclabs.com)
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

#include <fstream>
#include <functional>
#include <pion/PionConfig.hpp>
#include <pion/PionPlugin.hpp>
#include <pion/platform/PluginConfig.hpp>
#include <pion/platform/Codec.hpp>
#include <pion/platform/CodecFactory.hpp>
#include <pion/PionUnitTestDefs.hpp>
#include <pion/platform/PionPlatformUnitTest.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/mpl/list.hpp>
#include <boost/regex.hpp>

#ifdef PION_WIN32
#define OSEOL "\r\n"
#else
#define OSEOL "\n"
#endif

using namespace pion;
using namespace pion::platform;


/// static strings used by these unit tests
static const std::string COMMON_LOG_FILE(LOG_FILE_DIR + "common.log");
static const std::string COMBINED_LOG_FILE(LOG_FILE_DIR + "combined.log");
static const std::string EXTENDED_LOG_FILE(LOG_FILE_DIR + "extended.log");


/// cleans up config files relevant to Codecs in the working directory
void cleanup_codec_config_files(bool copy_codec_config_file)
{
	PionPlatformUnitTest::cleanup_vocab_config_files();

	if (boost::filesystem::exists(CODECS_CONFIG_FILE))
		boost::filesystem::remove(CODECS_CONFIG_FILE);
	if (copy_codec_config_file)
		boost::filesystem::copy_file(CODECS_TEMPLATE_FILE, CODECS_CONFIG_FILE);
}


BOOST_AUTO_TEST_CASE(checkPionPluginPtrDeclaredBeforeCodecPtr) {
	
	// Note that PionPluginPtr MUST be in scope as long or longer than any
	// Codecs that use it!!!
	
	PionPluginPtr<Codec> ppp;
	CodecPtr p;
	ppp.open("LogCodec");
	p = CodecPtr(ppp.create());
	BOOST_CHECK_EQUAL(p->getContentType(), "text/ascii");
}

BOOST_AUTO_TEST_CASE(checkPionPluginPtrDeclaredAfterCodecPtr) {

	//BOOST_FAIL("This test would cause a crash if the code below were not commented out");

	// This is a placeholder to alert people that this test would result in a crash

	// The only difference between this test and checkPionPluginPtrDeclaredBeforeCodecPtr is that
	// the first two lines are swapped.  In this case, when p goes out of scope, it crashes.

	// This happens because PionPluginPtr contains the library codec for "LogCodec",
	// so if it goes out of scope while there are still active LogCodec's instances,
	// LogCodec's destructor will attempt to access stack code that no longer exists.
	
/*
	CodecPtr p;
	PionPluginPtr<Codec> ppp;
	ppp.open("LogCodec");
	p = CodecPtr(ppp.create());
	BOOST_CHECK_EQUAL(p->getContentType(), "text/ascii");
*/
}


class PluginPtrReadyToAddCodec_F : public PionPluginPtr<Codec> {
public:
	PluginPtrReadyToAddCodec_F() {
	}
};

BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(PluginPtrReadyToAddCodec_S, 
									   boost::mpl::list<PluginPtrReadyToAddCodec_F>)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkOpenLogCodec) {
	BOOST_CHECK_NO_THROW(F::open("LogCodec"));
}

#ifdef PION_HAVE_JSON
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkOpenJSONCodec) {
	BOOST_CHECK_NO_THROW(F::open("JSONCodec"));
}
#endif

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkOpenXMLCodec) {
	BOOST_CHECK_NO_THROW(F::open("XMLCodec"));
}

BOOST_AUTO_TEST_SUITE_END()


template<const char* plugin_name>
class PluginPtrWithCodecLoaded_F : public PluginPtrReadyToAddCodec_F {
public:
	PluginPtrWithCodecLoaded_F() {
		m_plugin_name = plugin_name;
		open(m_plugin_name);
		m_codec = NULL;
	}
	~PluginPtrWithCodecLoaded_F() {
		if (m_codec) destroy(m_codec);
	}

	Codec* m_codec;
	std::string m_plugin_name;
};

// These have external linkage so they can be used as template parameters.
extern const char LogCodec_name[]  = "LogCodec";
extern const char JSONCodec_name[] = "JSONCodec";
extern const char XMLCodec_name[]  = "XMLCodec";

typedef boost::mpl::list<PluginPtrWithCodecLoaded_F<LogCodec_name>,
#ifdef PION_HAVE_JSON
						 PluginPtrWithCodecLoaded_F<JSONCodec_name>,
#endif
						 PluginPtrWithCodecLoaded_F<XMLCodec_name> > codec_fixture_list;

// PluginPtrWithCodecLoaded_S contains tests that should pass for any type of Codec
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(PluginPtrWithCodecLoaded_S, codec_fixture_list)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkIsOpenReturnsTrue) {
	BOOST_CHECK(F::is_open());
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetPluginNameReturnsPluginName) {
	BOOST_CHECK_EQUAL(F::getPluginName(), F::m_plugin_name);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkCreateReturnsSomething) {
	BOOST_CHECK((F::m_codec = F::create()) != NULL);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkDestroyDoesntThrowExceptionAfterCreate) {
	F::m_codec = F::create();
	BOOST_CHECK_NO_THROW(F::destroy(F::m_codec));
	F::m_codec = NULL;
}

BOOST_AUTO_TEST_SUITE_END()


typedef enum { CREATED, CLONED, MANUFACTURED } LINEAGE;

template<const char* plugin_type, LINEAGE lineage>
class CodecPtr_F {
public:
	CodecPtr_F() : m_config_ptr(NULL) {
		cleanup_codec_config_files(true);
		BOOST_REQUIRE(lineage == CREATED || lineage == CLONED || lineage == MANUFACTURED);
		if (lineage == MANUFACTURED) {
			p.reset(); // MANUFACTURED is only allowed for derived classes that support it.  See checkLineageIsOK.
		} else {
			m_ppp.open(plugin_type);
			m_original_codec_ptr = CodecPtr(m_ppp.create());
			BOOST_REQUIRE(lineage == CREATED || lineage == CLONED);
			p = (lineage == CREATED? m_original_codec_ptr : m_original_codec_ptr->clone());
			BOOST_REQUIRE(p);
		}
		m_codec_type = plugin_type;
	}
	virtual ~CodecPtr_F() {
		if (m_config_ptr) {
			xmlFreeNodeList(m_config_ptr);
		}

		// make sure all shared pointers to any Codec created by m_ppp are reset prior to destruction of m_ppp
		m_original_codec_ptr.reset(); // note that m_original_codec_ptr might never have pointed to anything
		p.reset(); // note that p might have already been reset
	}

	// From a string representation of a Codec configuration, obtain an xmlNodePtr that
	// points to a list of all the child nodes, as needed by Codec::setConfig().
	void parseConfig(const std::string& config_str, xmlNodePtr& config_ptr) {
		config_ptr = ConfigManager::createResourceConfig("Codec", config_str.c_str(), config_str.size());
		BOOST_REQUIRE(config_ptr);
	}

public:
	CodecPtr p; // This is what's actually playing the role of fixture, i.e., F::p is being tested, not F itself.
	xmlNodePtr m_config_ptr;

	// If you feel the need to use this in a test, you should probably instead move the test to a more specific test suite.
	// This is here to make it easy to temporarily skip tests that belong here, but don't pass yet.
	std::string m_codec_type;

protected:
	CodecPtr m_original_codec_ptr;
	PionPluginPtr<Codec> m_ppp;
};

typedef boost::mpl::list<
	CodecPtr_F<LogCodec_name, CREATED>,
	CodecPtr_F<LogCodec_name, CLONED>,
#ifdef PION_HAVE_JSON
	CodecPtr_F<JSONCodec_name, CREATED>,
	CodecPtr_F<JSONCodec_name, CLONED>,
#endif
	CodecPtr_F<XMLCodec_name, CREATED>,
	CodecPtr_F<XMLCodec_name, CLONED>
> CodecPtr_fixture_list;

// CodecPtr_S contains tests that should pass for any type of Codec
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(CodecPtr_S, CodecPtr_fixture_list)

// This will fail if the fixture template is instantiated with a lineage inappropriate for this test suite, e.g. MANUFACTURED.
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkLineageIsOK) {
	BOOST_REQUIRE(F::p);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetContentType) {
	// Exact values are tested elsewhere, in tests of specific Codecs.
	BOOST_CHECK(F::p->getContentType() != "");
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetEventType) {
	BOOST_CHECK_EQUAL(F::p->getEventType(), Vocabulary::UNDEFINED_TERM_REF);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetId) {
	// Would it be better if this threw an exception?
	BOOST_CHECK(F::p->getId() == "");
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkReadWithEventOfUndefinedType) {
	EventFactory event_factory;
	EventPtr ep(event_factory.create(Vocabulary::UNDEFINED_TERM_REF));
	std::stringstream ss("some text\n");

	// Currently, this is returning true for LogCodecs.  Although a case can be made for this,
	// in that it's succeeding in reading zero fields, it seems misleading.
	// Should this throw an exception instead, e.g., something like EmptyFieldMap?
	// Should it return false, since it didn't read anything?
	// Or is it OK?
	/////////BOOST_WARN(F::p->read(ss, *ep) == false);
	BOOST_WARN_THROW(F::p->read(ss, *ep), PionException);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkReadWithEventOfUndefinedTypeAndEmptyString) {
	EventFactory event_factory;
	EventPtr ep(event_factory.create(Vocabulary::UNDEFINED_TERM_REF));
	std::stringstream ss("");

	// See comments in previous test, checkReadWithEventOfUndefinedType.
	/////////BOOST_WARN(F::p->read(ss, *ep) == false);
	BOOST_WARN_THROW(F::p->read(ss, *ep), PionException);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkReadWithEventOfWrongType) {
	VocabularyManager vocab_mgr;
	vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
	vocab_mgr.openConfigFile();
	VocabularyPtr vocab_ptr(vocab_mgr.getVocabulary());
	Event::EventType some_type = vocab_ptr->findTerm("urn:vocab:clickstream#useragent");

	EventFactory event_factory;
	EventPtr ep(event_factory.create(some_type));
	std::stringstream ss("some text\n");
	BOOST_CHECK_THROW(F::p->read(ss, *ep), Codec::WrongEventTypeException);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkSetConfig) {
	// Prepare some valid input for Codec::setConfig().
	std::string event_type_1 = "urn:vocab:clickstream#http-event";
	this->parseConfig("<PionConfig><Codec>"
					"<EventType>" + event_type_1 + "</EventType>"
				"</Codec></PionConfig>",
				F::m_config_ptr);
	VocabularyManager vocab_mgr;
	vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
	vocab_mgr.openConfigFile();
	VocabularyPtr vocab_ptr(vocab_mgr.getVocabulary());

	// Confirm that setConfig() returns.
	BOOST_CHECK_NO_THROW(F::p->setConfig(*vocab_ptr, F::m_config_ptr));

	// Check that Codec::getEventType() returns the EventType specified in the configuration.
	Event::EventType event_type_ref = vocab_ptr->findTerm(event_type_1);
	BOOST_CHECK_EQUAL(F::p->getEventType(), event_type_ref);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkSetConfigWithRepeatedFieldTermsWithTheSameName) {
	this->parseConfig("<PionConfig><Codec>"
					"<EventType>urn:vocab:clickstream#http-event</EventType>"
					"<Field term=\"urn:vocab:test#plain-old-int\">A</Field>"
					"<Field term=\"urn:vocab:test#plain-old-int\">A</Field>"
				"</Codec></PionConfig>",
				F::m_config_ptr);
	VocabularyManager vocab_mgr;
	vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
	vocab_mgr.openConfigFile();
	VocabularyPtr vocab_ptr(vocab_mgr.getVocabulary());

	// Confirm that setConfig() throws an exception.
	BOOST_CHECK_THROW(F::p->setConfig(*vocab_ptr, F::m_config_ptr), PionException);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkSetConfigWithRepeatedFieldTermsWithDifferentNames) {
	this->parseConfig("<PionConfig><Codec>"
					"<EventType>urn:vocab:clickstream#http-event</EventType>"
					"<Field term=\"urn:vocab:test#plain-old-int\">A</Field>"
					"<Field term=\"urn:vocab:test#plain-old-int\">B</Field>"
				"</Codec></PionConfig>",
				F::m_config_ptr);
	VocabularyManager vocab_mgr;
	vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
	vocab_mgr.openConfigFile();
	VocabularyPtr vocab_ptr(vocab_mgr.getVocabulary());

	// Confirm that setConfig() throws an exception.
	BOOST_CHECK_THROW(F::p->setConfig(*vocab_ptr, F::m_config_ptr), PionException);
}

// This is definitely ambiguous for a JSONCodec.  (It can't distinguish between big-int and plain-old-int.)
// It's not ambiguous for a LogCodec, but it's still probably not a good idea to have two columns with the same name.
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkSetConfigWithRepeatedFieldNames) {
	this->parseConfig("<PionConfig><Codec>"
					"<EventType>urn:vocab:clickstream#http-event</EventType>"
					"<Field term=\"urn:vocab:test#plain-old-int\">A</Field>"
					"<Field term=\"urn:vocab:test#big-int\">A</Field>"
				"</Codec></PionConfig>",
				F::m_config_ptr);
	VocabularyManager vocab_mgr;
	vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
	vocab_mgr.openConfigFile();
	VocabularyPtr vocab_ptr(vocab_mgr.getVocabulary());

	// Confirm that setConfig() throws an exception.
	BOOST_CHECK_THROW(F::p->setConfig(*vocab_ptr, F::m_config_ptr), PionException);
}

// This is just one basic test of Codec::clone(), which is primarily being tested via fixtures
// CodecPtr_F<*, CLONED>, ConfiguredCodecPtr_F<*, CLONED>, etc.
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkClone) {
	// Note that p might already be a clone, depending on the fixture.
	BOOST_CHECK(F::p->clone());

	// just one simple check of 'cloneness'
	BOOST_CHECK(F::p->clone()->getContentType() == F::p->getContentType());
}

BOOST_AUTO_TEST_SUITE_END()


static const std::string NAME_1       = "Test Codec";
static const std::string EVENT_TYPE_1 = "urn:vocab:clickstream#http-event";
static const std::string FIELD_TERM_1 = "urn:vocab:clickstream#bytes";
static const std::string FIELD_NAME_1 = "bytes";

template<const char* plugin_type, LINEAGE lineage>
class ConfiguredCodecPtr_F : public CodecPtr_F<plugin_type, lineage> {
public:
	ConfiguredCodecPtr_F()
		: m_vocab_mgr(), m_vocab_ptr(m_vocab_mgr.getVocabulary()), m_factory(NULL)
	{
		initFixture("<PionConfig><Codec>"
						"<Plugin>" + std::string(plugin_type) + "</Plugin>"
						"<Name>" + NAME_1 + "</Name>"
						"<EventType>" + EVENT_TYPE_1 + "</EventType>"
						"<Field term=\"" + FIELD_TERM_1 + "\">" + FIELD_NAME_1 + "</Field>"
					"</Codec></PionConfig>");
	}
	virtual ~ConfiguredCodecPtr_F() {
		if (lineage == MANUFACTURED) {
			// the Codec was created by a factory, so make sure it's destroyed prior to destruction of the factory
			this->p.reset();

			delete m_factory;
		}
	}

protected:
	// enables a derived fixture to pass in its own configuration string before the Codec is constructed
	ConfiguredCodecPtr_F(const std::string& config_str) : m_vocab_mgr(), m_factory(NULL) {
		// a derived fixture can pass in an empty string to bypass initFixture()
		if (!config_str.empty())
			initFixture(config_str);
	}

	VocabularyManager m_vocab_mgr;
	VocabularyPtr	m_vocab_ptr;

private:
	void initFixture(const std::string& config_str) {
		// Create and parse a valid Codec configuration string.
		this->parseConfig(config_str, this->m_config_ptr);

		initVocabularyManager();
		makeConfiguredCodecPtr();
	}

	void initVocabularyManager() {
		// Initialize the VocabularyManager.
		m_vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
		m_vocab_mgr.openConfigFile();
		m_vocab_ptr = m_vocab_mgr.getVocabulary();
	}

	void initCodecFactory() {
		// Initialize the CodecFactory.
		m_factory->setConfigFile(CODECS_CONFIG_FILE);
		m_factory->openConfigFile();
	}

protected:
	void makeConfiguredCodecPtr() {
		// Make a configured CodecPtr of the specified lineage.
		if (lineage == MANUFACTURED) {
			m_factory = new CodecFactory(m_vocab_mgr);
			initCodecFactory();
			std::string codec_id = m_factory->addCodec(this->m_config_ptr);
			this->p = m_factory->getCodec(codec_id);
		} else {
			this->m_original_codec_ptr->setConfig(*m_vocab_ptr, this->m_config_ptr);
			this->p = (lineage == CREATED? this->m_original_codec_ptr : this->m_original_codec_ptr->clone());
		}
	}

private:
	CodecFactory* m_factory;
};

// This might eventually become part of Event itself, but first it would need to, at least,
// be updated to accommodate reordered Event entries, and have tests of its own.
bool operator==(const Event& e1, const Event& e2) {
	Event::ConstIterator it_1 = e1.begin();
	Event::ConstIterator it_2 = e2.begin();
	while (it_1 != e1.end() && it_2 != e2.end()) {
		if (it_1->term_ref != it_2->term_ref)
			return false;
		//if (it_1->value != it_2->value)	// boost::variant doesn't define operator!=
		if (!(it_1->value == it_2->value))
			return false;
		++it_1;
		++it_2;
	}
	return (it_1 == e1.end() && it_2 == e2.end());
}

typedef boost::mpl::list<
	ConfiguredCodecPtr_F<LogCodec_name, CREATED>,
	ConfiguredCodecPtr_F<LogCodec_name, CLONED>,
	ConfiguredCodecPtr_F<LogCodec_name, MANUFACTURED>,
#ifdef PION_HAVE_JSON
	ConfiguredCodecPtr_F<JSONCodec_name, CREATED>,
	ConfiguredCodecPtr_F<JSONCodec_name, CLONED>,
	ConfiguredCodecPtr_F<JSONCodec_name, MANUFACTURED>,
#endif
	ConfiguredCodecPtr_F<XMLCodec_name, CREATED>,
	ConfiguredCodecPtr_F<XMLCodec_name, CLONED>,
	ConfiguredCodecPtr_F<XMLCodec_name, MANUFACTURED>
> ConfiguredCodecPtr_fixture_list;

// ConfiguredCodecPtr_S contains tests that should pass for any type of Codec.
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(ConfiguredCodecPtr_S, ConfiguredCodecPtr_fixture_list)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetName) {
	BOOST_CHECK_EQUAL(F::p->getName(), NAME_1);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetComment) {
	BOOST_CHECK_EQUAL(F::p->getComment(), "");
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetEventType) {
	Event::EventType expected_event_type_ref = F::m_vocab_ptr->findTerm(EVENT_TYPE_1);
	BOOST_CHECK_EQUAL(F::p->getEventType(), expected_event_type_ref);
}

// This is just one basic test of Codec::clone(), which is primarily being tested via fixtures
// CodecPtr_F<*, CLONED>, ConfiguredCodecPtr_F<*, CLONED>, etc.
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkClone) {
	// Note that p might already be a clone, depending on the fixture.
	BOOST_CHECK(F::p->clone());

	// just one simple check of 'cloneness'
	BOOST_CHECK(F::p->clone()->getContentType() == F::p->getContentType());
}

// Need Codec specific versions of this - an empty string is not valid XML, nor does it
// constitute a JSON array, so XMLCodec and JSONCodec should throw an exception.
// See checkReadWithEmptyEventArray, checkReadWithEmptyRootElement and
// ConfiguredLogCodecPtr_S::checkReadWithEmptyString.
/*
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkReadWithEmptyString) {
	EventFactory event_factory;
	EventPtr ep(event_factory.create(F::p->getEventType()));
	std::stringstream ss("");
	BOOST_CHECK(F::p->read(ss, *ep) == false);
}
*/

// It's convenient to have this test in this suite, but note that the input string can't be
// universally valid, so read() could legitimately throw a different exception due to the
// input string, without ever touching the event.
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkReadWithEventOfUndefinedType) {
	EventFactory event_factory;
	EventPtr ep(event_factory.create(Vocabulary::UNDEFINED_TERM_REF));
	std::stringstream ss("some text\n");
	BOOST_CHECK_THROW(F::p->read(ss, *ep), Codec::WrongEventTypeException);
}

// See comment for previous test, checkReadWithEventOfUndefinedType.
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkReadWithEventOfWrongType) {
	Event::EventType other_type = F::m_vocab_ptr->findTerm("urn:vocab:clickstream#useragent");
	BOOST_REQUIRE(other_type != F::p->getEventType());
	EventFactory event_factory;
	EventPtr ep(event_factory.create(other_type));
	std::stringstream ss("some text\n");
	BOOST_CHECK_THROW(F::p->read(ss, *ep), Codec::WrongEventTypeException);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkWriteOutputsSomething) {
	EventAllocator ea;
	Event e(F::p->getEventType(), &ea);
	std::ostringstream out;
	BOOST_CHECK_NO_THROW(F::p->write(out, e));
	BOOST_CHECK(!out.str().empty());
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkReadOutputOfWrite) {
	EventFactory event_factory;
	EventPtr event_ptr(event_factory.create(F::p->getEventType()));
	Vocabulary::TermRef bytes_ref = F::m_vocab_ptr->findTerm(FIELD_TERM_1);
	event_ptr->setUBigInt(bytes_ref, 42);
	std::ostringstream out;
	BOOST_CHECK_NO_THROW(F::p->write(out, *event_ptr));
	std::string output_str = out.str();
	std::istringstream in(output_str);
	EventPtr event_ptr_2(event_factory.create(F::p->getEventType()));
	BOOST_CHECK(F::p->read(in, *event_ptr_2));
	BOOST_CHECK_EQUAL(event_ptr_2->getUBigInt(bytes_ref), static_cast<boost::uint64_t>(42));
	BOOST_CHECK(*event_ptr == *event_ptr_2);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkReadOutputOfWriteAfterFinish) {
	EventFactory event_factory;
	EventPtr event_ptr(event_factory.create(F::p->getEventType()));
	Vocabulary::TermRef bytes_ref = F::m_vocab_ptr->findTerm(FIELD_TERM_1);
	event_ptr->setUBigInt(bytes_ref, 42);
	std::ostringstream out;
	BOOST_CHECK_NO_THROW(F::p->write(out, *event_ptr));
	BOOST_CHECK_NO_THROW(F::p->finish(out));
	std::string output_str = out.str();
	std::istringstream in(output_str);
	EventPtr event_ptr_2(event_factory.create(F::p->getEventType()));
	BOOST_CHECK(F::p->read(in, *event_ptr_2));
	BOOST_CHECK_EQUAL(event_ptr_2->getUBigInt(bytes_ref), static_cast<boost::uint64_t>(42));
	BOOST_CHECK(*event_ptr == *event_ptr_2);

	BOOST_CHECK(!F::p->read(in, *event_ptr_2));
	BOOST_CHECK(event_ptr_2->empty());
	BOOST_CHECK(in.eof());
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkReadOutputOfWritingEmptyEvent) {
	EventFactory event_factory;
	EventPtr event_ptr(event_factory.create(F::p->getEventType()));
	std::ostringstream out;
	BOOST_CHECK_NO_THROW(F::p->write(out, *event_ptr));
	BOOST_CHECK_NO_THROW(F::p->finish(out));
	std::string output_str = out.str();
	std::istringstream in(output_str);
	EventPtr event_ptr_2(event_factory.create(F::p->getEventType()));
	BOOST_CHECK(F::p->read(in, *event_ptr_2));
	BOOST_CHECK(event_ptr_2->empty());
	BOOST_CHECK(*event_ptr == *event_ptr_2);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkReadOutputOfWriteWithMultipleValuesForATerm) {
	EventFactory event_factory;
	EventPtr event_ptr(event_factory.create(F::p->getEventType()));
	Vocabulary::TermRef bytes_ref = F::m_vocab_ptr->findTerm(FIELD_TERM_1);
	event_ptr->setUBigInt(bytes_ref, 42);
	event_ptr->setUBigInt(bytes_ref, 82);
	std::ostringstream out;
	BOOST_CHECK_NO_THROW(F::p->write(out, *event_ptr));
	std::string output_str = out.str();
	std::istringstream in(output_str);
	EventPtr event_ptr_2(event_factory.create(F::p->getEventType()));

	// The only guarantee we can make for a generic Codec is that we can read the event
	// and one of the values that was set for the term is in the reconstituted event.
	BOOST_CHECK(F::p->read(in, *event_ptr_2));
	BOOST_CHECK(event_ptr_2->getUBigInt(bytes_ref) % 40 == 2);
}

BOOST_AUTO_TEST_SUITE_END()

static const std::string EVENT_TYPE_2 = "urn:vocab:test#simple-object";

template<const char* plugin_type, LINEAGE lineage>
class ReconfiguredCodecPtr_F : public ConfiguredCodecPtr_F<plugin_type, lineage> {
public:
	ReconfiguredCodecPtr_F() {
		// Add a new term to the Vocabulary.
		std::string config_str = "<PionConfig><Term><Type>float</Type></Term></PionConfig>";
		this->m_vocab_mgr.addTerm("urn:vocab:test",
								  "urn:vocab:test#float-term-1",
								  ConfigManager::createResourceConfig("Term", config_str.c_str(), config_str.size()));
		this->m_vocab_ptr = this->m_vocab_mgr.getVocabulary();

		// Reconfigure the Codec, with a different event type and a field using the new term.
		this->parseConfig("<PionConfig><Codec>"
						"<EventType>" + EVENT_TYPE_2 + "</EventType>"
						"<Field term=\"urn:vocab:test#plain-old-int\">A</Field>"
						"<Field term=\"urn:vocab:test#float-term-1\">B</Field>"
					"</Codec></PionConfig>",
					this->m_config_ptr);
		this->p->setConfig(*this->m_vocab_ptr, this->m_config_ptr);
	}
	virtual ~ReconfiguredCodecPtr_F() {
	}
};

typedef boost::mpl::list<
	ReconfiguredCodecPtr_F<LogCodec_name, CREATED>,
	ReconfiguredCodecPtr_F<LogCodec_name, CLONED>,
	ReconfiguredCodecPtr_F<LogCodec_name, MANUFACTURED>,
#ifdef PION_HAVE_JSON
	ReconfiguredCodecPtr_F<JSONCodec_name, CREATED>,
	ReconfiguredCodecPtr_F<JSONCodec_name, CLONED>,
	ReconfiguredCodecPtr_F<JSONCodec_name, MANUFACTURED>,
#endif
	ReconfiguredCodecPtr_F<XMLCodec_name, CREATED>,
	ReconfiguredCodecPtr_F<XMLCodec_name, CLONED>,
	ReconfiguredCodecPtr_F<XMLCodec_name, MANUFACTURED>
> ReconfiguredCodecPtr_fixture_list;

// ReconfiguredCodecPtr_S contains tests that should pass for any type of Codec.
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(ReconfiguredCodecPtr_S, ReconfiguredCodecPtr_fixture_list)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetEventType) {
	// Confirm that we get the new event type.
	Event::EventType expected_event_type_ref = F::m_vocab_ptr->findTerm(EVENT_TYPE_2);
	BOOST_CHECK_EQUAL(F::p->getEventType(), expected_event_type_ref);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkReadOutputOfWrite) {
	EventFactory event_factory;
	EventPtr event_ptr(event_factory.create(F::p->getEventType()));
	Vocabulary::TermRef term_ref_A = F::m_vocab_ptr->findTerm("urn:vocab:test#plain-old-int");
	event_ptr->setInt(term_ref_A, 42);
	Vocabulary::TermRef term_ref_B = F::m_vocab_ptr->findTerm("urn:vocab:test#float-term-1");
	event_ptr->setFloat(term_ref_B, 1.23F);
	std::ostringstream out;
	BOOST_CHECK_NO_THROW(F::p->write(out, *event_ptr));
	std::string output_str = out.str();
	std::istringstream in(output_str);
	EventPtr event_ptr_2(event_factory.create(F::p->getEventType()));
	BOOST_CHECK(F::p->read(in, *event_ptr_2));
	BOOST_CHECK_EQUAL(event_ptr_2->getInt(term_ref_A), static_cast<boost::int32_t>(42));
	BOOST_CHECK_EQUAL(event_ptr_2->getFloat(term_ref_B), 1.23F);
	BOOST_CHECK(*event_ptr == *event_ptr_2);
}

BOOST_AUTO_TEST_SUITE_END()


static const std::string FIELD_TERM_INT_16  = "urn:vocab:test#plain-old-int";
static const std::string FIELD_NAME_INT_16  = "plain-old-int";
static const std::string FIELD_TERM_UINT_64 = "urn:vocab:test#big-int";
static const std::string FIELD_NAME_UINT_64 = "big-int";
static const std::string FIELD_TERM_DATE    = "urn:vocab:test#date";
static const std::string FIELD_NAME_DATE    = "date";
static const boost::int16_t  E1_FIELD_VALUE_INT_16  = 500;
#if _MSC_VER == 1500  // 1500 == VC++ 9.0
// This value uses all 64 bits, but only 18 decimal digits, thus avoiding a bug
// in VC9 that occurs with 19 decimal digit numbers.
// Boost regression test lexical_cast_test is also failing because of this bug; 
// see http://www.boost.org/development/tests/release-1_35_0/developer/conversion.html.
static const boost::uint64_t E1_FIELD_VALUE_UINT_64 = 0x8A00FF00FF00FF00ULL;
#else
static const boost::uint64_t E1_FIELD_VALUE_UINT_64 = 0xFF00FF00FF00FF00ULL;
#endif
static const boost::int16_t  E2_FIELD_VALUE_INT_16  = 0;
static const boost::uint64_t E2_FIELD_VALUE_UINT_64 = 0x0123456789ABCDEFULL;

template<const char* plugin_type, LINEAGE lineage>
class CodecPtrWithVariousFieldTerms_F : public ConfiguredCodecPtr_F<plugin_type, lineage> {
public:
	CodecPtrWithVariousFieldTerms_F() : ConfiguredCodecPtr_F<plugin_type, lineage>(
		"<PionConfig><Codec>"
			"<Plugin>" + std::string(plugin_type) + "</Plugin>"
			"<Name>" + NAME_1 + "</Name>"
			"<EventType>" + EVENT_TYPE_2 + "</EventType>"
			"<Field term=\"" + FIELD_TERM_INT_16  + "\">" + FIELD_NAME_INT_16  + "</Field>"
			"<Field term=\"" + FIELD_TERM_UINT_64 + "\">" + FIELD_NAME_UINT_64 + "</Field>"
			"<Field term=\"" + FIELD_TERM_DATE    + "\">" + FIELD_NAME_DATE    + "</Field>"
		"</Codec></PionConfig>") {

		m_event_ptr = event_factory.create(this->p->getEventType());
	}
	~CodecPtrWithVariousFieldTerms_F() {
	}

	EventFactory event_factory;
	EventPtr m_event_ptr;
};

typedef boost::mpl::list<
	CodecPtrWithVariousFieldTerms_F<LogCodec_name, CREATED>,
	CodecPtrWithVariousFieldTerms_F<LogCodec_name, CLONED>,
#ifdef PION_HAVE_JSON
	CodecPtrWithVariousFieldTerms_F<JSONCodec_name, CREATED>,
	CodecPtrWithVariousFieldTerms_F<JSONCodec_name, CLONED>,
#endif
	CodecPtrWithVariousFieldTerms_F<XMLCodec_name, CREATED>,
	CodecPtrWithVariousFieldTerms_F<XMLCodec_name, CLONED>
> ConfiguredCodecPtrNoFactory_fixture_list;

// ConfiguredCodecPtrNoFactory_S contains tests that should pass for any type of Codec.
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(ConfiguredCodecPtrNoFactory_S, ConfiguredCodecPtrNoFactory_fixture_list)

// This test needs to be in the "No Factory" suite, because in the case where the
// Codec is created by a factory, calling m_vocab_mgr.removeTerm() automatically calls
// updateVocabulary() on the Codec.
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkUpdateVocabularyWithOneTermRemoved) {
	F::m_vocab_mgr.setLocked("urn:vocab:test", false);
	F::m_vocab_mgr.removeTerm("urn:vocab:test", FIELD_TERM_INT_16);
	VocabularyPtr vocab_ptr(F::m_vocab_mgr.getVocabulary());
	BOOST_CHECK_EQUAL(vocab_ptr->findTerm(FIELD_TERM_INT_16), Vocabulary::UNDEFINED_TERM_REF);
	BOOST_CHECK_THROW(F::p->updateVocabulary(*vocab_ptr), Vocabulary::TermNoLongerDefinedException);
}

// See comment for checkUpdateVocabularyWithOneTermRemoved().
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkUpdateVocabularyWithOneTermChanged) {
	Vocabulary::TermRef term_ref = F::m_vocab_ptr->findTerm(FIELD_TERM_INT_16);
	Vocabulary::Term modified_term = (*F::m_vocab_ptr)[term_ref];
	modified_term.term_comment = "A modified comment";
	F::m_vocab_mgr.setLocked("urn:vocab:test", false);
	F::m_vocab_mgr.updateTerm("urn:vocab:test", modified_term);

	BOOST_CHECK_NO_THROW(F::p->updateVocabulary(*F::m_vocab_mgr.getVocabulary()));
}

// See comment for checkUpdateVocabularyWithOneTermRemoved().
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkUpdateVocabularyWithDateFormatChanged) {
	// Create an Event with some known value for the date field.
	EventFactory event_factory;
	EventPtr event_ptr(event_factory.create(F::p->getEventType()));
	Vocabulary::TermRef date_ref = F::m_vocab_ptr->findTerm(FIELD_TERM_DATE);
	event_ptr->setDateTime(date_ref, PionDateTime(boost::gregorian::date(2008, 1, 10)));

	// Output the Event and confirm that the date appears in the expected format.
	std::ostringstream out;
	BOOST_CHECK_NO_THROW(F::p->write(out, *event_ptr));
	std::string out_str = out.str();
	BOOST_CHECK(boost::regex_search(out_str, boost::regex("2008-01-10")));

	// Update the date format.
	Vocabulary::Term modified_term = (*F::m_vocab_ptr)[date_ref];
	modified_term.term_format = "%m/%d/%y";
	F::m_vocab_mgr.setLocked("urn:vocab:test", false);
	F::m_vocab_mgr.updateTerm("urn:vocab:test", modified_term);
	BOOST_CHECK_NO_THROW(F::p->updateVocabulary(*F::m_vocab_mgr.getVocabulary()));

	// Output the Event again and check that the date appears in the new format.
	std::ostringstream out2;
	BOOST_CHECK_NO_THROW(F::p->write(out2, *event_ptr));
	out_str = out2.str();
	BOOST_CHECK(boost::regex_search(out_str, boost::regex("01/10/08")));
}

BOOST_AUTO_TEST_SUITE_END()


typedef CodecPtrWithVariousFieldTerms_F<LogCodec_name, CREATED> ConfiguredLogCodecPtr_F;
BOOST_FIXTURE_TEST_SUITE(ConfiguredLogCodecPtr_S, ConfiguredLogCodecPtr_F)

BOOST_AUTO_TEST_CASE(checkReadWithEmptyString) {
	EventFactory event_factory;
	EventPtr ep(event_factory.create(p->getEventType()));
	std::stringstream ss("");
	BOOST_CHECK(p->read(ss, *ep) == false);
}

BOOST_AUTO_TEST_CASE(checkReadOneEvent) {
	std::ostringstream oss;
	oss << E1_FIELD_VALUE_INT_16 << " "
		<< E1_FIELD_VALUE_UINT_64 << " "
		<< "-" << "\n"; // leave the date field empty for now
	std::istringstream in(oss.str());

	BOOST_CHECK(p->read(in, *m_event_ptr));

	BOOST_CHECK_EQUAL(m_event_ptr->getInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 )), E1_FIELD_VALUE_INT_16);
	BOOST_CHECK_EQUAL(m_event_ptr->getUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64)), E1_FIELD_VALUE_UINT_64);
}

BOOST_AUTO_TEST_SUITE_END()


#ifdef PION_HAVE_JSON
typedef CodecPtrWithVariousFieldTerms_F<JSONCodec_name, CREATED> ConfiguredJSONCodecPtr_F;
BOOST_FIXTURE_TEST_SUITE(ConfiguredJSONCodecPtr_S, ConfiguredJSONCodecPtr_F)

BOOST_AUTO_TEST_CASE(checkReadWithEmptyEventArray) {
	EventFactory event_factory;
	EventPtr ep(event_factory.create(p->getEventType()));
	std::stringstream ss("[]");
	BOOST_CHECK(p->read(ss, *ep) == false);
	BOOST_CHECK(p->read(ss, *ep) == false);
}

BOOST_AUTO_TEST_CASE(checkReadOneEvent) {
	const boost::int16_t FIELD_VALUE_INT_16 = 500;
#if _MSC_VER == 1500  // 1500 == VC++ 9.0
	BOOST_WARN_MESSAGE(false, "working around a bug in VC9");
	// This value uses all 64 bits, but only 18 decimal digits; see comments for E1_FIELD_VALUE_UINT_64.
	const boost::uint64_t FIELD_VALUE_UINT_64 = 0x8A00FF00FF00FF00ULL;
#else
	const boost::uint64_t FIELD_VALUE_UINT_64 = 0xFF00FF00FF00FF00ULL;
#endif
	const int YEAR = 2008;
	const int MONTH = 6;
	const int DAY = 16;

	std::ostringstream oss;
	oss << "[{ \"" << FIELD_NAME_INT_16  << "\": " << FIELD_VALUE_INT_16
		<<  ", \"" << FIELD_NAME_UINT_64 << "\": " << FIELD_VALUE_UINT_64
		<<  ", \"" << FIELD_NAME_DATE    << "\": " << "\"" << YEAR << "-" << MONTH << "-" << DAY << "\""
		<<  "}";
	std::istringstream in(oss.str());

	BOOST_CHECK(p->read(in, *m_event_ptr));

	BOOST_CHECK_EQUAL(m_event_ptr->getInt(     m_vocab_ptr->findTerm(FIELD_TERM_INT_16 )), FIELD_VALUE_INT_16);
	BOOST_CHECK_EQUAL(m_event_ptr->getUBigInt( m_vocab_ptr->findTerm(FIELD_TERM_UINT_64)), FIELD_VALUE_UINT_64);
	BOOST_CHECK_EQUAL(m_event_ptr->getDateTime(m_vocab_ptr->findTerm(FIELD_TERM_DATE)),
					  PionDateTime(boost::gregorian::date(YEAR, MONTH, DAY)));
}

BOOST_AUTO_TEST_CASE(checkReadOneEventWithTermOrderChanged) {
	const boost::int16_t FIELD_VALUE_INT_16 = 500;
#if _MSC_VER == 1500  // 1500 == VC++ 9.0
	BOOST_WARN_MESSAGE(false, "working around a bug in VC9");
	// This value uses all 64 bits, but only 18 decimal digits; see comments for E1_FIELD_VALUE_UINT_64.
	const boost::uint64_t FIELD_VALUE_UINT_64 = 0x8A00FF00FF00FF00ULL;
#else
	const boost::uint64_t FIELD_VALUE_UINT_64 = 0xFF00FF00FF00FF00ULL;
#endif

	// This time, the terms are not in the order in which they appear in the configuration.
	std::ostringstream oss;
	oss << "[{ \"" << FIELD_NAME_UINT_64 << "\": " << FIELD_VALUE_UINT_64
		<<  ", \"" << FIELD_NAME_INT_16  << "\": " << FIELD_VALUE_INT_16
		<< " }";
	std::istringstream in(oss.str());

	BOOST_CHECK(p->read(in, *m_event_ptr));

	BOOST_CHECK_EQUAL(m_event_ptr->getInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 )), FIELD_VALUE_INT_16);
	BOOST_CHECK_EQUAL(m_event_ptr->getUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64)), FIELD_VALUE_UINT_64);
}

BOOST_AUTO_TEST_CASE(checkReadWithLeadingWhiteSpace) {
	std::ostringstream oss;
	oss << " \n\t[{ \"" << FIELD_NAME_INT_16  << "\": " << E1_FIELD_VALUE_INT_16 << " }";
	std::istringstream in(oss.str());

	BOOST_CHECK(p->read(in, *m_event_ptr));

	BOOST_CHECK_EQUAL(m_event_ptr->getInt(m_vocab_ptr->findTerm(FIELD_TERM_INT_16)), E1_FIELD_VALUE_INT_16);
}

BOOST_AUTO_TEST_CASE(checkReadTwoEvents) {
	std::ostringstream oss;
	oss << "[ { \"" << FIELD_NAME_INT_16  << "\": " << E1_FIELD_VALUE_INT_16
		<<  " , \"" << FIELD_NAME_UINT_64 << "\": " << E1_FIELD_VALUE_UINT_64
		<<  " }"
		<< ", { \"" << FIELD_NAME_INT_16  << "\": " << E2_FIELD_VALUE_INT_16
		<<  " , \"" << FIELD_NAME_UINT_64 << "\": " << E2_FIELD_VALUE_UINT_64
		<<  " } "
		<< "]";
	std::istringstream in(oss.str());

	// read and verify the first event
	BOOST_CHECK(p->read(in, *m_event_ptr));
	BOOST_CHECK_EQUAL(m_event_ptr->getInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 )), E1_FIELD_VALUE_INT_16);
	BOOST_CHECK_EQUAL(m_event_ptr->getUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64)), E1_FIELD_VALUE_UINT_64);

	// iterate through the event to confirm that the values are in the configured order, and that there are no other values
	Event::ConstIterator it = m_event_ptr->begin();
	BOOST_CHECK_EQUAL(boost::get<boost::int32_t>( it->value), E1_FIELD_VALUE_INT_16);
	it++;
	BOOST_CHECK_EQUAL(boost::get<boost::uint64_t>(it->value), E1_FIELD_VALUE_UINT_64);
	it++;
	BOOST_CHECK(it == m_event_ptr->end());

	// read and verify the second event
	BOOST_CHECK(p->read(in, *m_event_ptr));
	BOOST_CHECK_EQUAL(m_event_ptr->getInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 )), E2_FIELD_VALUE_INT_16);
	BOOST_CHECK_EQUAL(m_event_ptr->getUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64)), E2_FIELD_VALUE_UINT_64);

	// iterate through the event to confirm that the values are in the configured order, and that there are no other values
	it = m_event_ptr->begin();
	BOOST_CHECK_EQUAL(boost::get<boost::int32_t>( it->value), E2_FIELD_VALUE_INT_16);
	it++;
	BOOST_CHECK_EQUAL(boost::get<boost::uint64_t>(it->value), E2_FIELD_VALUE_UINT_64);
	it++;
	BOOST_CHECK(it == m_event_ptr->end());
}

BOOST_AUTO_TEST_CASE(checkReadWithMultipleValuesForATerm) {
	std::ostringstream oss;
	oss << "[{ \"" << FIELD_NAME_INT_16  << "\": " << 105
		<<  ", \"" << FIELD_NAME_UINT_64 << "\": " << "\"" << E1_FIELD_VALUE_UINT_64 << "\"" 
		<<  ", \"" << FIELD_NAME_INT_16  << "\": " << 205
		<< " }";
	std::istringstream in(oss.str());

	// Check that an event can be read from the input.
	BOOST_CHECK(p->read(in, *m_event_ptr));

	// Check that both input values are present in the event for the multiple-valued term.
	Vocabulary::TermRef	multiple_valued_term_ref = m_vocab_ptr->findTerm(FIELD_TERM_INT_16);
	Event::ValuesRange range = m_event_ptr->equal_range(multiple_valued_term_ref);
	Event::ConstIterator i = range.first;
	BOOST_REQUIRE(i != range.second);
	BOOST_CHECK(boost::get<boost::int32_t>(i->value) % 100 == 5);
	BOOST_REQUIRE(++i != range.second);
	BOOST_CHECK(boost::get<boost::int32_t>(i->value) % 100 == 5);
	BOOST_REQUIRE(++i == range.second);

	// Finally, check the value of the non-multiple valued term.
	BOOST_CHECK_EQUAL(m_event_ptr->getUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64)), E1_FIELD_VALUE_UINT_64);
}

BOOST_AUTO_TEST_CASE(checkWriteOneEvent) {
	// initialize the event
	m_event_ptr->setInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 ), E1_FIELD_VALUE_INT_16);
	m_event_ptr->setUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64), E1_FIELD_VALUE_UINT_64);

	// Write a string with the expected output.
	// Due to YAJL limitations, 64-bit integers are output as quoted strings.
	std::ostringstream oss;
	oss << "[\n"
		<< "\t{\n"
		<< "\t\t\"" << FIELD_NAME_INT_16  << "\": " << E1_FIELD_VALUE_INT_16 << ",\n" 
		<< "\t\t\"" << FIELD_NAME_UINT_64 << "\": " << "\"" << E1_FIELD_VALUE_UINT_64 << "\"\n" 
		<< "\t}";
	std::string expected_output_string = oss.str();

	// Confirm that the output is as expected.
	std::ostringstream out;
	BOOST_REQUIRE_NO_THROW(p->write(out, *m_event_ptr));
	BOOST_CHECK_EQUAL(out.str(), expected_output_string);
}

BOOST_AUTO_TEST_CASE(checkWriteOneEventWithTermOrderChanged) {
	// This time, the terms are not set in the order in which they appear in the configuration.
	m_event_ptr->setUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64), E1_FIELD_VALUE_UINT_64);
	m_event_ptr->setInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 ), E1_FIELD_VALUE_INT_16);

	// The terms should still be output in the order in which they appear in the configuration.
	std::ostringstream oss;
	oss << "[\n"
		<< "\t{\n"
		<< "\t\t\"" << FIELD_NAME_INT_16  << "\": " << E1_FIELD_VALUE_INT_16 << ",\n" 
		<< "\t\t\"" << FIELD_NAME_UINT_64 << "\": " << "\"" << E1_FIELD_VALUE_UINT_64 << "\"\n" 
		<< "\t}";
	std::string expected_output_string = oss.str();

	// Confirm that the output is as expected.
	std::ostringstream out;
	BOOST_REQUIRE_NO_THROW(p->write(out, *m_event_ptr));
	BOOST_CHECK_EQUAL(out.str(), expected_output_string);
}

BOOST_AUTO_TEST_CASE(checkWriteOneEventAndFinish) {
	// initialize the event
	m_event_ptr->setInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 ), E1_FIELD_VALUE_INT_16);
	m_event_ptr->setUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64), E1_FIELD_VALUE_UINT_64);

	// This time there should be a ']' at the end, to indicate that there are no more events.
	std::ostringstream oss;
	oss << "[\n"
		<< "\t{\n"
		<< "\t\t\"" << FIELD_NAME_INT_16  << "\": " << E1_FIELD_VALUE_INT_16 << ",\n" 
		<< "\t\t\"" << FIELD_NAME_UINT_64 << "\": " << "\"" << E1_FIELD_VALUE_UINT_64 << "\"\n" 
		<< "\t}\n"
		<< "]\n";
	std::string expected_output_string = oss.str();

	// Confirm that the output is as expected.
	std::ostringstream out;
	BOOST_REQUIRE_NO_THROW(p->write(out, *m_event_ptr));
	BOOST_REQUIRE_NO_THROW(p->finish(out));
	BOOST_CHECK_EQUAL(out.str(), expected_output_string);
}

BOOST_AUTO_TEST_CASE(checkWriteWithMultipleValuesForATerm) {
	// Initialize the event, setting a value twice for one of the terms.
	m_event_ptr->setInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 ), 105);
	m_event_ptr->setUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64), 12345);
	m_event_ptr->setInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 ), 205);

	// Output the event.
	std::ostringstream out;
	BOOST_REQUIRE_NO_THROW(p->write(out, *m_event_ptr));

	// Prepare a regular expression for a name value pair.
	// Note that numerical values can occur with or without quotes.
	const boost::regex name_value_pair("\"([\\w-]+)\":\\s*\"?(\\d+)\"?");

	// Confirm that there are 3 name/value pairs and save them.
	std::string out_str = out.str();
	std::string::const_iterator start = out_str.begin();
	std::string::const_iterator end   = out_str.end(); 
	typedef std::pair<std::string, std::string> string_pair;
	std::vector<string_pair> v;
	boost::match_results<std::string::const_iterator> m;
	BOOST_REQUIRE(boost::regex_search(start, end, m, name_value_pair));
	v.push_back(make_pair(m[1], m[2]));
	start = m[0].second;
	BOOST_REQUIRE(boost::regex_search(start, end, m, name_value_pair));
	v.push_back(make_pair(m[1], m[2]));
	start = m[0].second;
	BOOST_REQUIRE(boost::regex_search(start, end, m, name_value_pair));
	v.push_back(make_pair(m[1], m[2]));
	start = m[0].second;

	// Next token should be '}'.
	BOOST_CHECK(boost::regex_match(start, end, boost::regex("\\s*\\}.*")));

	// Confirm that the three pairs we found are the ones we expected (in any order).
	BOOST_CHECK(find(v.begin(), v.end(), string_pair("plain-old-int", "105")) != v.end());
	BOOST_CHECK(find(v.begin(), v.end(), string_pair("plain-old-int", "205")) != v.end());
	BOOST_CHECK(find(v.begin(), v.end(), string_pair("big-int", "12345")) != v.end());
}

BOOST_AUTO_TEST_SUITE_END()
#endif


typedef CodecPtrWithVariousFieldTerms_F<XMLCodec_name, CREATED> ConfiguredXMLCodecPtr_F;
BOOST_FIXTURE_TEST_SUITE(ConfiguredXMLCodecPtr_S, ConfiguredXMLCodecPtr_F)

BOOST_AUTO_TEST_CASE(checkReadWithEmptyRootElement) {
	EventFactory event_factory;
	EventPtr ep(event_factory.create(p->getEventType()));
	std::stringstream ss("<Events></Events>");
	BOOST_CHECK(p->read(ss, *ep) == false);
	BOOST_CHECK(p->read(ss, *ep) == false);
}

BOOST_AUTO_TEST_CASE(checkReadOneEvent) {
	const boost::int16_t FIELD_VALUE_INT_16 = 500;
	const boost::uint64_t FIELD_VALUE_UINT_64 = E1_FIELD_VALUE_UINT_64;
	const int YEAR = 2008;
	const int MONTH = 6;
	const int DAY = 16;

	std::ostringstream oss;
	oss << "<Events><Event>" 
		<< "<" << FIELD_NAME_INT_16  << ">" << FIELD_VALUE_INT_16  << "</" << FIELD_NAME_INT_16  << ">" 
		<< "<" << FIELD_NAME_UINT_64 << ">" << FIELD_VALUE_UINT_64 << "</" << FIELD_NAME_UINT_64 << ">" 
		<< "<" << FIELD_NAME_DATE    << ">" << YEAR << "-" << MONTH << "-" << DAY << "</" << FIELD_NAME_DATE << ">" 
		<< "</Event></Events>";
	std::istringstream in(oss.str());

	BOOST_CHECK(p->read(in, *m_event_ptr));

	BOOST_CHECK_EQUAL(m_event_ptr->getInt(     m_vocab_ptr->findTerm(FIELD_TERM_INT_16 )), FIELD_VALUE_INT_16);
	BOOST_CHECK_EQUAL(m_event_ptr->getUBigInt( m_vocab_ptr->findTerm(FIELD_TERM_UINT_64)), FIELD_VALUE_UINT_64);
	BOOST_CHECK_EQUAL(m_event_ptr->getDateTime(m_vocab_ptr->findTerm(FIELD_TERM_DATE)),
					  PionDateTime(boost::gregorian::date(YEAR, MONTH, DAY)));
}

BOOST_AUTO_TEST_CASE(checkReadOneEventWithTermOrderChanged) {
	const boost::int16_t FIELD_VALUE_INT_16 = 500;
	const boost::uint64_t FIELD_VALUE_UINT_64 = E1_FIELD_VALUE_UINT_64;

	// This time, the terms are not in the order in which they appear in the configuration.
	std::ostringstream oss;
	oss << "<Events><Event>" 
		<< "<" << FIELD_NAME_UINT_64 << ">" << FIELD_VALUE_UINT_64 << "</" << FIELD_NAME_UINT_64 << ">" 
		<< "<" << FIELD_NAME_INT_16  << ">" << FIELD_VALUE_INT_16  << "</" << FIELD_NAME_INT_16  << ">" 
		<< "</Event></Events>";
	std::istringstream in(oss.str());

	BOOST_CHECK(p->read(in, *m_event_ptr));

	BOOST_CHECK_EQUAL(m_event_ptr->getInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 )), FIELD_VALUE_INT_16);
	BOOST_CHECK_EQUAL(m_event_ptr->getUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64)), FIELD_VALUE_UINT_64);
}

BOOST_AUTO_TEST_CASE(checkReadWithLeadingWhiteSpace) {
	std::ostringstream oss;
	oss << " \n\t<Events><Event>" 
		<< "<" << FIELD_NAME_INT_16  << ">" << E1_FIELD_VALUE_INT_16  << "</" << FIELD_NAME_INT_16  << ">" 
		<< "</Event></Events>";
	std::istringstream in(oss.str());

	BOOST_CHECK(p->read(in, *m_event_ptr));

	BOOST_CHECK_EQUAL(m_event_ptr->getInt(m_vocab_ptr->findTerm(FIELD_TERM_INT_16)), E1_FIELD_VALUE_INT_16);
}

BOOST_AUTO_TEST_CASE(checkReadTwoEvents) {
	std::ostringstream oss;
	oss << "<Events><Event>" 
		<< "<" << FIELD_NAME_INT_16  << ">" << E1_FIELD_VALUE_INT_16  << "</" << FIELD_NAME_INT_16  << ">" 
		<< "<" << FIELD_NAME_UINT_64 << ">" << E1_FIELD_VALUE_UINT_64 << "</" << FIELD_NAME_UINT_64 << ">" 
		<< "</Event><Event>"
		<< "<" << FIELD_NAME_INT_16  << ">" << E2_FIELD_VALUE_INT_16  << "</" << FIELD_NAME_INT_16  << ">" 
		<< "<" << FIELD_NAME_UINT_64 << ">" << E2_FIELD_VALUE_UINT_64 << "</" << FIELD_NAME_UINT_64 << ">" 
		<< "</Event></Events>";
	std::istringstream in(oss.str());

	// read and verify the first event
	BOOST_CHECK(p->read(in, *m_event_ptr));
	BOOST_CHECK_EQUAL(m_event_ptr->getInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 )), E1_FIELD_VALUE_INT_16);
	BOOST_CHECK_EQUAL(m_event_ptr->getUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64)), E1_FIELD_VALUE_UINT_64);

	// iterate through the event to confirm that the values are in the configured order, and that there are no other values
	Event::ConstIterator it = m_event_ptr->begin();
	BOOST_CHECK_EQUAL(boost::get<boost::int32_t>( it->value), E1_FIELD_VALUE_INT_16);
	it++;
	BOOST_CHECK_EQUAL(boost::get<boost::uint64_t>(it->value), E1_FIELD_VALUE_UINT_64);
	it++;
	BOOST_CHECK(it == m_event_ptr->end());

	// read and verify the second event
	BOOST_CHECK(p->read(in, *m_event_ptr));
	BOOST_CHECK_EQUAL(m_event_ptr->getInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 )), E2_FIELD_VALUE_INT_16);
	BOOST_CHECK_EQUAL(m_event_ptr->getUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64)), E2_FIELD_VALUE_UINT_64);

	// iterate through the event to confirm that the values are in the configured order, and that there are no other values
	it = m_event_ptr->begin();
	BOOST_CHECK_EQUAL(boost::get<boost::int32_t>( it->value), E2_FIELD_VALUE_INT_16);
	it++;
	BOOST_CHECK_EQUAL(boost::get<boost::uint64_t>(it->value), E2_FIELD_VALUE_UINT_64);
	it++;
	BOOST_CHECK(it == m_event_ptr->end());
}

BOOST_AUTO_TEST_CASE(checkReadWithMultipleValuesForATerm) {
	std::ostringstream oss;
	oss << "<Events><Event>" 
		<< "<" << FIELD_NAME_INT_16  << ">" << 105                    << "</" << FIELD_NAME_INT_16  << ">" 
		<< "<" << FIELD_NAME_UINT_64 << ">" << E1_FIELD_VALUE_UINT_64 << "</" << FIELD_NAME_UINT_64 << ">" 
		<< "<" << FIELD_NAME_INT_16  << ">" << 205                    << "</" << FIELD_NAME_INT_16  << ">" 
		<< "</Event></Events>";
	std::istringstream in(oss.str());

	// Check that an event can be read from the input.
	BOOST_CHECK(p->read(in, *m_event_ptr));

	// Check that both input values are present in the event for the multiple-valued term.
	Vocabulary::TermRef	multiple_valued_term_ref = m_vocab_ptr->findTerm(FIELD_TERM_INT_16);
	Event::ValuesRange range = m_event_ptr->equal_range(multiple_valued_term_ref);
	Event::ConstIterator i = range.first;
	BOOST_REQUIRE(i != range.second);
	BOOST_CHECK(boost::get<boost::int32_t>(i->value) % 100 == 5);
	BOOST_REQUIRE(++i != range.second);
	BOOST_CHECK(boost::get<boost::int32_t>(i->value) % 100 == 5);
	BOOST_REQUIRE(++i == range.second);

	// Finally, check the value of the non-multiple valued term.
	BOOST_CHECK_EQUAL(m_event_ptr->getUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64)), E1_FIELD_VALUE_UINT_64);
}

BOOST_AUTO_TEST_CASE(checkWriteOneEvent) {
	// initialize the event
	m_event_ptr->setInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 ), E1_FIELD_VALUE_INT_16);
	m_event_ptr->setUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64), E1_FIELD_VALUE_UINT_64);

	// Make a string with the expected output.
	std::ostringstream oss;
	oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		<< "<Events>\n"
		<< "\t<Event>\n"
		<< "\t\t<" << FIELD_NAME_INT_16  << ">" << E1_FIELD_VALUE_INT_16  << "</" << FIELD_NAME_INT_16  << ">\n" 
		<< "\t\t<" << FIELD_NAME_UINT_64 << ">" << E1_FIELD_VALUE_UINT_64 << "</" << FIELD_NAME_UINT_64 << ">\n" 
		<< "\t</Event>\n";
	std::string expected_output_string = oss.str();

	// Confirm that the output is as expected.
	std::ostringstream out;
	BOOST_REQUIRE_NO_THROW(p->write(out, *m_event_ptr));
	BOOST_CHECK_EQUAL(out.str(), expected_output_string);
}

BOOST_AUTO_TEST_CASE(checkWriteOneEventWithTermOrderChanged) {
	// This time, the terms are not set in the order in which they appear in the configuration.
	m_event_ptr->setUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64), E1_FIELD_VALUE_UINT_64);
	m_event_ptr->setInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 ), E1_FIELD_VALUE_INT_16);

	// The terms should still be output in the order in which they appear in the configuration.
	std::ostringstream oss;
	oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		<< "<Events>\n"
		<< "\t<Event>\n"
		<< "\t\t<" << FIELD_NAME_INT_16  << ">" << E1_FIELD_VALUE_INT_16  << "</" << FIELD_NAME_INT_16  << ">\n" 
		<< "\t\t<" << FIELD_NAME_UINT_64 << ">" << E1_FIELD_VALUE_UINT_64 << "</" << FIELD_NAME_UINT_64 << ">\n" 
		<< "\t</Event>\n";
	std::string expected_output_string = oss.str();

	// Confirm that the output is as expected.
	std::ostringstream out;
	BOOST_REQUIRE_NO_THROW(p->write(out, *m_event_ptr));
	BOOST_CHECK_EQUAL(out.str(), expected_output_string);
}

BOOST_AUTO_TEST_CASE(checkWriteOneEventAndFinish) {
	// initialize the event
	m_event_ptr->setInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 ), E1_FIELD_VALUE_INT_16);
	m_event_ptr->setUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64), E1_FIELD_VALUE_UINT_64);

	// This time there should be an 'Events' end-tag, to indicate that there are no more events.
	std::ostringstream oss;
	oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		<< "<Events>\n"
		<< "\t<Event>\n"
		<< "\t\t<" << FIELD_NAME_INT_16  << ">" << E1_FIELD_VALUE_INT_16  << "</" << FIELD_NAME_INT_16  << ">\n" 
		<< "\t\t<" << FIELD_NAME_UINT_64 << ">" << E1_FIELD_VALUE_UINT_64 << "</" << FIELD_NAME_UINT_64 << ">\n" 
		<< "\t</Event>\n"
		<< "</Events>\n";
	std::string expected_output_string = oss.str();

	// Confirm that the output is as expected.
	std::ostringstream out;
	BOOST_REQUIRE_NO_THROW(p->write(out, *m_event_ptr));
	BOOST_REQUIRE_NO_THROW(p->finish(out));
	BOOST_CHECK_EQUAL(out.str(), expected_output_string);
}

BOOST_AUTO_TEST_CASE(checkWriteWithMultipleValuesForATerm) {
	// Initialize the event, setting a value twice for one of the terms.
	m_event_ptr->setInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 ), 105);
	m_event_ptr->setUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64), 12345);
	m_event_ptr->setInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 ), 205);

	// Output the event.
	std::ostringstream out;
	BOOST_REQUIRE_NO_THROW(p->write(out, *m_event_ptr));

	// Prepare a regular expression for a numeric Term element.
	const boost::regex name_value_pair("<([\\w-]+)>(\\d+)</[\\w-]+>");

	// Confirm that there are 3 Term elements and save them.
	std::string out_str = out.str();
	std::string::const_iterator start = out_str.begin();
	std::string::const_iterator end   = out_str.end(); 
	typedef std::pair<std::string, std::string> string_pair;
	std::vector<string_pair> v;
	boost::match_results<std::string::const_iterator> m;
	BOOST_REQUIRE(boost::regex_search(start, end, m, name_value_pair));
	v.push_back(make_pair(m[1], m[2]));
	start = m[0].second;
	BOOST_REQUIRE(boost::regex_search(start, end, m, name_value_pair));
	v.push_back(make_pair(m[1], m[2]));
	start = m[0].second;
	BOOST_REQUIRE(boost::regex_search(start, end, m, name_value_pair));
	v.push_back(make_pair(m[1], m[2]));
	start = m[0].second;

	// Confirm that there are only 3 Term elements.
	BOOST_CHECK(boost::regex_match(start, end, boost::regex("\\s*</Event>.*")));

	// Confirm that the three elements we found are the ones we expected (in any order).
	BOOST_CHECK(find(v.begin(), v.end(), string_pair("plain-old-int", "105")) != v.end());
	BOOST_CHECK(find(v.begin(), v.end(), string_pair("plain-old-int", "205")) != v.end());
	BOOST_CHECK(find(v.begin(), v.end(), string_pair("big-int", "12345")) != v.end());
}

BOOST_AUTO_TEST_SUITE_END()


class XMLCodecWithNonDefaultEventTags_F : public ConfiguredCodecPtr_F<XMLCodec_name, CREATED> {
public:
	XMLCodecWithNonDefaultEventTags_F() : ConfiguredCodecPtr_F<XMLCodec_name, CREATED>(
		"<PionConfig><Codec>"
			"<Plugin>" + std::string(XMLCodec_name) + "</Plugin>"
			"<Name>" + NAME_1 + "</Name>"
			"<EventType>" + EVENT_TYPE_2 + "</EventType>"
			"<EventTag>Apple</EventTag>"
			"<EventContainerTag>Applecart</EventContainerTag>"
			"<Field term=\"" + FIELD_TERM_INT_16  + "\">" + FIELD_NAME_INT_16  + "</Field>"
			"<Field term=\"" + FIELD_TERM_UINT_64 + "\">" + FIELD_NAME_UINT_64 + "</Field>"
			"<Field term=\"" + FIELD_TERM_DATE    + "\">" + FIELD_NAME_DATE    + "</Field>"
		"</Codec></PionConfig>")
	{
		m_event_ptr = event_factory.create(this->p->getEventType());
	}
	~XMLCodecWithNonDefaultEventTags_F() {
	}

	EventFactory event_factory;
	EventPtr m_event_ptr;
};

BOOST_FIXTURE_TEST_SUITE(XMLCodecWithNonDefaultEventTags_S, XMLCodecWithNonDefaultEventTags_F)

BOOST_AUTO_TEST_CASE(checkReadWithEmptyRootElement) {
	EventFactory event_factory;
	EventPtr ep(event_factory.create(p->getEventType()));
	std::stringstream ss("<Applecart></Applecart>");
	BOOST_CHECK(p->read(ss, *ep) == false);
	BOOST_CHECK(p->read(ss, *ep) == false);
}

BOOST_AUTO_TEST_CASE(checkReadOneEvent) {
	const boost::int16_t FIELD_VALUE_INT_16 = 500;
	const boost::uint64_t FIELD_VALUE_UINT_64 = E1_FIELD_VALUE_UINT_64;
	const int YEAR = 2008;
	const int MONTH = 6;
	const int DAY = 16;

	std::ostringstream oss;
	oss << "<Applecart><Apple>" 
		<< "<" << FIELD_NAME_INT_16  << ">" << FIELD_VALUE_INT_16  << "</" << FIELD_NAME_INT_16  << ">" 
		<< "<" << FIELD_NAME_UINT_64 << ">" << FIELD_VALUE_UINT_64 << "</" << FIELD_NAME_UINT_64 << ">" 
		<< "<" << FIELD_NAME_DATE    << ">" << YEAR << "-" << MONTH << "-" << DAY << "</" << FIELD_NAME_DATE << ">" 
		<< "</Apple></Applecart>";
	std::istringstream in(oss.str());

	BOOST_CHECK(p->read(in, *m_event_ptr));

	BOOST_CHECK_EQUAL(m_event_ptr->getInt(     m_vocab_ptr->findTerm(FIELD_TERM_INT_16 )), FIELD_VALUE_INT_16);
	BOOST_CHECK_EQUAL(m_event_ptr->getUBigInt( m_vocab_ptr->findTerm(FIELD_TERM_UINT_64)), FIELD_VALUE_UINT_64);
	BOOST_CHECK_EQUAL(m_event_ptr->getDateTime(m_vocab_ptr->findTerm(FIELD_TERM_DATE)),
					  PionDateTime(boost::gregorian::date(YEAR, MONTH, DAY)));
}

BOOST_AUTO_TEST_CASE(checkWriteOneEventAndFinish) {
	// initialize the event
	m_event_ptr->setInt(    m_vocab_ptr->findTerm(FIELD_TERM_INT_16 ), E1_FIELD_VALUE_INT_16);
	m_event_ptr->setUBigInt(m_vocab_ptr->findTerm(FIELD_TERM_UINT_64), E1_FIELD_VALUE_UINT_64);

	// Specify the expected usage of the non-default tags.
	std::ostringstream oss;
	oss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		<< "<Applecart>\n"
		<< "\t<Apple>\n"
		<< "\t\t<" << FIELD_NAME_INT_16  << ">" << E1_FIELD_VALUE_INT_16  << "</" << FIELD_NAME_INT_16  << ">\n" 
		<< "\t\t<" << FIELD_NAME_UINT_64 << ">" << E1_FIELD_VALUE_UINT_64 << "</" << FIELD_NAME_UINT_64 << ">\n" 
		<< "\t</Apple>\n"
		<< "</Applecart>\n";
	std::string expected_output_string = oss.str();

	// Confirm that the output is as expected.
	std::ostringstream out;
	BOOST_REQUIRE_NO_THROW(p->write(out, *m_event_ptr));
	BOOST_REQUIRE_NO_THROW(p->finish(out));
	BOOST_CHECK_EQUAL(out.str(), expected_output_string);
}

BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_SUITE(codecFactoryCreationAndDestruction_S)

BOOST_AUTO_TEST_CASE(checkCodecFactoryConstructor) {
	VocabularyManager vocab_mgr;
	BOOST_CHECK_NO_THROW(CodecFactory codecFactory(vocab_mgr));
}

BOOST_AUTO_TEST_CASE(checkCodecFactoryDestructor) {
	VocabularyManager vocab_mgr;
	CodecFactory* codecFactory = new CodecFactory(vocab_mgr);
	BOOST_CHECK_NO_THROW(delete codecFactory);
}

BOOST_AUTO_TEST_CASE(checkLockVocabularyManagerAfterCodecFactoryDestroyed) {
	VocabularyManager vocab_mgr;
	vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
	vocab_mgr.openConfigFile();
	{
		CodecFactory codecFactory(vocab_mgr);
	}

	vocab_mgr.setLocked("urn:vocab:clickstream", false);
}

BOOST_AUTO_TEST_SUITE_END()


template<const char* plugin_type, LINEAGE lineage>
class CodecPtrWithFieldsOfAllTypes_F : public ConfiguredCodecPtr_F<plugin_type, lineage> {
public:
	CodecPtrWithFieldsOfAllTypes_F() : ConfiguredCodecPtr_F<plugin_type, lineage>("") {
		initVocabularyManagerFromScratch();

		// Configure the Codec with one field for every Term.
		// Delimiters are ignored by Codecs that don't need them, e.g. JSONCodec or XMLCodec.
		this->parseConfig("<PionConfig><Codec>"
						"<EventType>urn:vocab:v1#object-term-1</EventType>"
						"<Field term=\"urn:vocab:v1#null-term-1\">null-1</Field>"
						"<Field term=\"urn:vocab:v1#int8-term-1\">int8-1</Field>"
						"<Field term=\"urn:vocab:v1#uint8-term-1\">uint8-1</Field>"
						"<Field term=\"urn:vocab:v1#int16-term-1\">int16-1</Field>"
						"<Field term=\"urn:vocab:v1#uint16-term-1\">uint16-1</Field>"
						"<Field term=\"urn:vocab:v1#int32-term-1\">int32-1</Field>"
						"<Field term=\"urn:vocab:v1#uint32-term-1\">uint32-1</Field>"
						"<Field term=\"urn:vocab:v1#int64-term-1\">int64-1</Field>"
						"<Field term=\"urn:vocab:v1#uint64-term-1\">uint64-1</Field>"
						"<Field term=\"urn:vocab:v1#float-term-1\">float-1</Field>"
						"<Field term=\"urn:vocab:v1#double-term-1\">double-1</Field>"
						"<Field term=\"urn:vocab:v1#longdouble-term-1\">longdouble-1</Field>"
						"<Field term=\"urn:vocab:v1#shortstring-term-1\" start=\"&quot;\" end=\"&quot;\">shortstring-1</Field>"
						"<Field term=\"urn:vocab:v1#string-term-1\" start=\"&quot;\" end=\"&quot;\">string-1</Field>"
						"<Field term=\"urn:vocab:v1#longstring-term-1\" start=\"&quot;\" end=\"&quot;\">longstring-1</Field>"
						"<Field term=\"urn:vocab:v1#datetime-term-1\" start=\"[\" end=\"]\">datetime-1</Field>"
						"<Field term=\"urn:vocab:v1#date-term-1\">date-1</Field>"
						"<Field term=\"urn:vocab:v1#time-term-1\">time-1</Field>"
						"<Field term=\"urn:vocab:v1#char-term-1\" start=\"&quot;\" end=\"&quot;\">char-1</Field>"
					"</Codec></PionConfig>",
					this->m_config_ptr);
		ConfiguredCodecPtr_F<plugin_type,lineage>::makeConfiguredCodecPtr();

		m_event_ptr_in = this->m_event_factory.create(this->p->getEventType());
		m_event_ptr_out = this->m_event_factory.create(this->p->getEventType());
	}
	virtual ~CodecPtrWithFieldsOfAllTypes_F() {
		this->m_vocab_mgr.removeVocabulary("urn:vocab:v1");
		PionPlatformUnitTest::cleanup_backup_files();
	}

	EventFactory m_event_factory;
	EventPtr m_event_ptr_in;
	EventPtr m_event_ptr_out;

private:
	// "from scratch" means without using any pre-existing files
	void initVocabularyManagerFromScratch() {
		if (boost::filesystem::exists(VOCABS_CONFIG_FILE))
			boost::filesystem::remove(VOCABS_CONFIG_FILE);
		this->m_vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
		this->m_vocab_mgr.createConfigFile();
		this->m_vocab_mgr.addVocabulary("urn:vocab:v1", "v1", "no comment");

		// one Term for every value of Vocabulary::DataType
		addTerm("null-term-1", "<Type>null</Type>");
		addTerm("int8-term-1", "<Type>int8</Type>");
		addTerm("uint8-term-1", "<Type>uint8</Type>");
		addTerm("int16-term-1", "<Type>int16</Type>");
		addTerm("uint16-term-1", "<Type>uint16</Type>");
		addTerm("int32-term-1", "<Type>int32</Type>");
		addTerm("uint32-term-1", "<Type>uint32</Type>");
		addTerm("int64-term-1", "<Type>int64</Type>");
		addTerm("uint64-term-1", "<Type>uint64</Type>");
		addTerm("float-term-1", "<Type>float</Type>");
		addTerm("double-term-1", "<Type>double</Type>");
		addTerm("longdouble-term-1", "<Type>longdouble</Type>");
		addTerm("shortstring-term-1", "<Type>shortstring</Type>");
		addTerm("string-term-1", "<Type>string</Type>");
		addTerm("longstring-term-1", "<Type>longstring</Type>");
		addTerm("datetime-term-1", "<Type format=\"%Y-%m-%d %H:%M:%S\">datetime</Type>");
		addTerm("date-term-1", "<Type format=\"%Y-%m-%d\">date</Type>");
		addTerm("time-term-1", "<Type format=\"%H:%M:%S\">time</Type>");
		addTerm("char-term-1", "<Type size=\"10\">char</Type>");
		addTerm("object-term-1", "<Type>object</Type>");

		this->m_vocab_ptr = this->m_vocab_mgr.getVocabulary();
	}

	void addTerm(const std::string& term_name, const std::string& inner_term_config) {
		std::string config_str = "<PionConfig><Term>" + inner_term_config + "</Term></PionConfig>";
		std::string term_id = std::string("urn:vocab:v1#") + term_name;
		this->m_vocab_mgr.addTerm("urn:vocab:v1", term_id,
								  ConfigManager::createResourceConfig("Term", config_str.c_str(), config_str.size()));
	}
};

// MANUFACTURED not included here, because it requires codecs.xml, the default version of which
// uses urn:vocab:clickstream.
typedef boost::mpl::list<
	CodecPtrWithFieldsOfAllTypes_F<LogCodec_name, CREATED>,
	CodecPtrWithFieldsOfAllTypes_F<LogCodec_name, CLONED>,
#ifdef PION_HAVE_JSON
	CodecPtrWithFieldsOfAllTypes_F<JSONCodec_name, CREATED>,
	CodecPtrWithFieldsOfAllTypes_F<JSONCodec_name, CLONED>,
#endif
	CodecPtrWithFieldsOfAllTypes_F<XMLCodec_name, CREATED>,
	CodecPtrWithFieldsOfAllTypes_F<XMLCodec_name, CLONED>
> CodecPtrWithFieldsOfAllTypes_fixture_list;

// CodecPtrWithFieldsOfAllTypes_S contains tests that should pass for any type of Codec.
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(CodecPtrWithFieldsOfAllTypes_S, CodecPtrWithFieldsOfAllTypes_fixture_list)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkReadOutputOfWrite) {
	// Set one value for every Term in urn:vocab:v1, except the null Term and the object Term.
	// TODO: object Terms aren't supported for LogCodecs, but could they be for some Codecs?
	F::m_event_ptr_in->setInt(       F::m_vocab_ptr->findTerm("urn:vocab:v1#int8-term-1"),       -8);
	F::m_event_ptr_in->setUInt(      F::m_vocab_ptr->findTerm("urn:vocab:v1#uint8-term-1"),       8);
	F::m_event_ptr_in->setInt(       F::m_vocab_ptr->findTerm("urn:vocab:v1#int16-term-1"),      -16);
	F::m_event_ptr_in->setUInt(      F::m_vocab_ptr->findTerm("urn:vocab:v1#uint16-term-1"),      16);
	F::m_event_ptr_in->setInt(       F::m_vocab_ptr->findTerm("urn:vocab:v1#int32-term-1"),      -32);
	F::m_event_ptr_in->setUInt(      F::m_vocab_ptr->findTerm("urn:vocab:v1#uint32-term-1"),      32);
	F::m_event_ptr_in->setBigInt(    F::m_vocab_ptr->findTerm("urn:vocab:v1#int64-term-1"),      -64);
	F::m_event_ptr_in->setUBigInt(   F::m_vocab_ptr->findTerm("urn:vocab:v1#uint64-term-1"),      64);
	F::m_event_ptr_in->setFloat(     F::m_vocab_ptr->findTerm("urn:vocab:v1#float-term-1"),       1.01234e-30F);
	F::m_event_ptr_in->setDouble(    F::m_vocab_ptr->findTerm("urn:vocab:v1#double-term-1"),      1.0123456789012345e-300);
	F::m_event_ptr_in->setLongDouble(F::m_vocab_ptr->findTerm("urn:vocab:v1#longdouble-term-1"),  1.0123456789012345e+300L);
	F::m_event_ptr_in->setString(    F::m_vocab_ptr->findTerm("urn:vocab:v1#shortstring-term-1"), "abc");
	F::m_event_ptr_in->setString(    F::m_vocab_ptr->findTerm("urn:vocab:v1#string-term-1"),      "123");
	F::m_event_ptr_in->setString(    F::m_vocab_ptr->findTerm("urn:vocab:v1#longstring-term-1"),  "XYZ");
	PionDateTime date_time = PionTimeFacet("%Y-%m-%d %H:%M:%S").fromString("2008-06-17 10:22:01");
	F::m_event_ptr_in->setDateTime(  F::m_vocab_ptr->findTerm("urn:vocab:v1#datetime-term-1"),    date_time);
	PionDateTime date = PionTimeFacet("%Y-%m-%d").fromString("2008-06-17");
	F::m_event_ptr_in->setDateTime(  F::m_vocab_ptr->findTerm("urn:vocab:v1#date-term-1"),        date);
	PionDateTime time = PionTimeFacet("%H:%M:%S").fromString("10:22:01");
	F::m_event_ptr_in->setDateTime(  F::m_vocab_ptr->findTerm("urn:vocab:v1#time-term-1"),        time);
	F::m_event_ptr_in->setString(    F::m_vocab_ptr->findTerm("urn:vocab:v1#char-term-1"),        "0123456789");

	// Write out the Event and read the output into a new Event.
	std::ostringstream out;
	BOOST_CHECK_NO_THROW(F::p->write(out, *F::m_event_ptr_in));
	BOOST_CHECK_NO_THROW(F::p->finish(out));
	std::istringstream in(out.str());
	BOOST_CHECK(F::p->read(in, *F::m_event_ptr_out));

	// Check that the reconstituted Event is the same as the original Event.
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getInt(       F::m_vocab_ptr->findTerm("urn:vocab:v1#int8-term-1")),       -8);
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getUInt(      F::m_vocab_ptr->findTerm("urn:vocab:v1#uint8-term-1")),       8U);
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getInt(       F::m_vocab_ptr->findTerm("urn:vocab:v1#int16-term-1")),      -16);
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getUInt(      F::m_vocab_ptr->findTerm("urn:vocab:v1#uint16-term-1")),      16U);
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getInt(       F::m_vocab_ptr->findTerm("urn:vocab:v1#int32-term-1")),      -32);
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getUInt(      F::m_vocab_ptr->findTerm("urn:vocab:v1#uint32-term-1")),      32U);
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getBigInt(    F::m_vocab_ptr->findTerm("urn:vocab:v1#int64-term-1")),      -64);
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getUBigInt(   F::m_vocab_ptr->findTerm("urn:vocab:v1#uint64-term-1")),      64U);
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getFloat(     F::m_vocab_ptr->findTerm("urn:vocab:v1#float-term-1")),       1.01234e-30F);
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getDouble(    F::m_vocab_ptr->findTerm("urn:vocab:v1#double-term-1")),      1.0123456789012345e-300);
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getLongDouble(F::m_vocab_ptr->findTerm("urn:vocab:v1#longdouble-term-1")),  1.0123456789012345e+300L);
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getString(    F::m_vocab_ptr->findTerm("urn:vocab:v1#shortstring-term-1")), "abc");
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getString(    F::m_vocab_ptr->findTerm("urn:vocab:v1#string-term-1")),      "123");
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getString(    F::m_vocab_ptr->findTerm("urn:vocab:v1#longstring-term-1")),  "XYZ");
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getDateTime(  F::m_vocab_ptr->findTerm("urn:vocab:v1#datetime-term-1")),    date_time);
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getDateTime(  F::m_vocab_ptr->findTerm("urn:vocab:v1#date-term-1")),        date);
	// Can't use BOOST_CHECK_EQUAL, because it uses operator<< on its inputs, which crashes for PionDateTimes without a date.
	BOOST_CHECK(      F::m_event_ptr_out->getDateTime(  F::m_vocab_ptr->findTerm("urn:vocab:v1#time-term-1")) ==      time);
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getString(    F::m_vocab_ptr->findTerm("urn:vocab:v1#char-term-1")),        "0123456789");
	BOOST_CHECK(*F::m_event_ptr_in == *F::m_event_ptr_out);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkReadOutputOfWritingPartialDateTimes) {
	// Pass in a complete date time to a term that only handles dates and a term that only handles times.
	PionTimeFacet time_facet("%Y-%m-%d %H:%M:%S");
	PionDateTime date_time = time_facet.fromString("2008-06-17 10:22:01");
	F::m_event_ptr_in->setDateTime(F::m_vocab_ptr->findTerm("urn:vocab:v1#date-term-1"), date_time);
	F::m_event_ptr_in->setDateTime(F::m_vocab_ptr->findTerm("urn:vocab:v1#time-term-1"), date_time);

	// Write out the Event and read the output into a new Event.
	std::ostringstream out;
	BOOST_CHECK_NO_THROW(F::p->write(out, *F::m_event_ptr_in));
	std::istringstream in(out.str());
	BOOST_CHECK(F::p->read(in, *F::m_event_ptr_out));

	// Check that the date part of the reconstituted date Term value is the same as the date part of the input.
	PionDateTime date_term_value = F::m_event_ptr_out->getDateTime(F::m_vocab_ptr->findTerm("urn:vocab:v1#date-term-1"));
	BOOST_CHECK_EQUAL(date_term_value.date(), date_time.date());

	// Check that the time part of the reconstituted time Term value is the same as the time part of the input.
	PionDateTime time_term_value = F::m_event_ptr_out->getDateTime(F::m_vocab_ptr->findTerm("urn:vocab:v1#time-term-1"));
	BOOST_CHECK_EQUAL(time_term_value.time_of_day(), date_time.time_of_day());
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkReadOutputOfWritingComplexStrings) {
	F::m_event_ptr_in->setString(F::m_vocab_ptr->findTerm("urn:vocab:v1#shortstring-term-1"), "  word1  word2  ");
	F::m_event_ptr_in->setString(F::m_vocab_ptr->findTerm("urn:vocab:v1#string-term-1"),      "\"quoted string\"");
	F::m_event_ptr_in->setString(F::m_vocab_ptr->findTerm("urn:vocab:v1#longstring-term-1"),  "a \"quoted\" substring");
	F::m_event_ptr_in->setString(F::m_vocab_ptr->findTerm("urn:vocab:v1#char-term-1"),        "one \" here");

	// Write out the Event and read the output into a new Event.
	std::ostringstream out;
	BOOST_CHECK_NO_THROW(F::p->write(out, *F::m_event_ptr_in));
	std::istringstream in(out.str());
	BOOST_CHECK(F::p->read(in, *F::m_event_ptr_out));

	// Check that the reconstituted Event is the same as the original Event.
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getString(F::m_vocab_ptr->findTerm("urn:vocab:v1#shortstring-term-1")), "  word1  word2  ");
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getString(F::m_vocab_ptr->findTerm("urn:vocab:v1#string-term-1")),      "\"quoted string\"");
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getString(F::m_vocab_ptr->findTerm("urn:vocab:v1#longstring-term-1")),  "a \"quoted\" substring");
	BOOST_CHECK_EQUAL(F::m_event_ptr_out->getString(F::m_vocab_ptr->findTerm("urn:vocab:v1#char-term-1")),        "one \" here");
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkFixedLengthCharTerm) {
	// Create three strings with length less than, equal to and greater than the length of the fixed-length Term.
	std::string undersized = "< 10";
	std::string just_right = "exactly 10";
	std::string oversized  = "more than 10 chars";

	// Write out three Events with the fixed-length Term value set to each of these strings.
	EventPtr event_ptr_1 = F::m_event_factory.create(this->p->getEventType());
	EventPtr event_ptr_2 = F::m_event_factory.create(this->p->getEventType());
	EventPtr event_ptr_3 = F::m_event_factory.create(this->p->getEventType());
	event_ptr_1->setString(F::m_vocab_ptr->findTerm("urn:vocab:v1#char-term-1"), undersized);
	event_ptr_2->setString(F::m_vocab_ptr->findTerm("urn:vocab:v1#char-term-1"), just_right);
	event_ptr_3->setString(F::m_vocab_ptr->findTerm("urn:vocab:v1#char-term-1"), oversized);
	std::ostringstream out;
	F::p->write(out, *event_ptr_1);
	F::p->write(out, *event_ptr_2);
	F::p->write(out, *event_ptr_3);

	// Read in the three Events.
	EventPtr event_ptr_out_1 = F::m_event_factory.create(this->p->getEventType());
	EventPtr event_ptr_out_2 = F::m_event_factory.create(this->p->getEventType());
	EventPtr event_ptr_out_3 = F::m_event_factory.create(this->p->getEventType());
	std::istringstream in(out.str());
	BOOST_CHECK(F::p->read(in, *event_ptr_out_1));
	BOOST_CHECK(F::p->read(in, *event_ptr_out_2));
	BOOST_CHECK(F::p->read(in, *event_ptr_out_3));

	// Check that the value of the fixed-length Term is correct in all cases.
	BOOST_CHECK_EQUAL(event_ptr_out_1->getString(F::m_vocab_ptr->findTerm("urn:vocab:v1#char-term-1")), undersized);
	BOOST_CHECK_EQUAL(event_ptr_out_2->getString(F::m_vocab_ptr->findTerm("urn:vocab:v1#char-term-1")), just_right);
	BOOST_CHECK_EQUAL(event_ptr_out_3->getString(F::m_vocab_ptr->findTerm("urn:vocab:v1#char-term-1")), oversized.substr(0, 10));

	// TODO: test a string longer than 255 bytes with "urn:vocab:v1#shortstring-term-1", etc.
}

BOOST_AUTO_TEST_SUITE_END()


typedef CodecPtrWithFieldsOfAllTypes_F<LogCodec_name, CREATED> LogCodecPtrWithFieldsOfAllTypes_F;
BOOST_FIXTURE_TEST_SUITE(LogCodecPtrWithFieldsOfAllTypes_S, LogCodecPtrWithFieldsOfAllTypes_F)

BOOST_AUTO_TEST_CASE(checkReadOverLongString) {
	// make an input string with empty values for all terms except the char (fixed-length) Term
	std::string str = "- - - - - - - - - - - - \"\" \"\" \"\" [] - - \"more than 10 chars\"";

	// read in an Event from the string
	std::istringstream in(str);
	BOOST_CHECK(p->read(in, *this->m_event_ptr_out));

	// Check that the reconstituted char Term value is just the first 10 characters of the input string.
	BOOST_CHECK_EQUAL(this->m_event_ptr_out->getString(m_vocab_ptr->findTerm("urn:vocab:v1#char-term-1")), "more than ");

	// TODO: test a string longer than 255 bytes with "urn:vocab:v1#shortstring-term-1", etc.
}

BOOST_AUTO_TEST_SUITE_END()


#ifdef PION_HAVE_JSON
typedef CodecPtrWithFieldsOfAllTypes_F<JSONCodec_name, CREATED> JSONCodecPtrWithFieldsOfAllTypes_F;
BOOST_FIXTURE_TEST_SUITE(JSONCodecPtrWithFieldsOfAllTypes_S, JSONCodecPtrWithFieldsOfAllTypes_F)

BOOST_AUTO_TEST_CASE(checkReadOverLongString) {
	// make a JSON input string with just a char (fixed-length) Term
	std::string str = "[{\"char-1\": \"more than 10 chars\"}]";

	// read in an Event from the string
	std::istringstream in(str);
	BOOST_CHECK(p->read(in, *this->m_event_ptr_out));

	// Check that the reconstituted char Term value is just the first 10 characters of the input string.
	BOOST_CHECK_EQUAL(this->m_event_ptr_out->getString(m_vocab_ptr->findTerm("urn:vocab:v1#char-term-1")), "more than ");

	// TODO: test a string longer than 255 bytes with "urn:vocab:v1#shortstring-term-1", etc.
}

BOOST_AUTO_TEST_SUITE_END()
#endif


typedef CodecPtrWithFieldsOfAllTypes_F<XMLCodec_name, CREATED> XMLCodecPtrWithFieldsOfAllTypes_F;
BOOST_FIXTURE_TEST_SUITE(XMLCodecPtrWithFieldsOfAllTypes_S, XMLCodecPtrWithFieldsOfAllTypes_F)

/*
BOOST_AUTO_TEST_CASE(checkReadOverLongString) {

}
*/

BOOST_AUTO_TEST_SUITE_END()


class NewCodecFactory_F : public CodecFactory {
public:
	NewCodecFactory_F() : CodecFactory(m_vocab_mgr) {
		cleanup_codec_config_files(false);
		
		if (! m_config_loaded) {
			// load the CLF vocabulary
			m_vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
			m_vocab_mgr.openConfigFile();
			m_config_loaded = true;
		}

		m_codec_id = "some_ID";

		// create a new codec configuration file
		setConfigFile(CODECS_CONFIG_FILE);
		createConfigFile();

		// check new codec config file
		// ...
	}
	~NewCodecFactory_F() {
	}

	/**
	 * returns a valid configuration tree for a Codec
	 *
	 * @param plugin_type the type of new plugin that is being created
	 *
	 * @return xmlNodePtr XML configuration list for the new Codec
	 */
	inline xmlNodePtr createCodecConfig(const std::string& plugin_type) {
		xmlNodePtr config_ptr(ConfigManager::createPluginConfig(plugin_type));
		xmlNodePtr event_type_node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>("EventType"));
		xmlNodeSetContent(event_type_node,  reinterpret_cast<const xmlChar*>("urn:vocab:clickstream#http-event"));
		xmlAddNextSibling(config_ptr, event_type_node);
		return config_ptr;
	}

	std::string m_codec_id;
	static VocabularyManager m_vocab_mgr;
	static bool m_config_loaded;
};

VocabularyManager	NewCodecFactory_F::m_vocab_mgr;
bool				NewCodecFactory_F::m_config_loaded = false;


BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(NewCodecFactory_S, 
									   boost::mpl::list<NewCodecFactory_F>)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkLoadLogCodec) {
	xmlNodePtr config_ptr(F::createCodecConfig("LogCodec"));
	BOOST_CHECK_NO_THROW(F::addCodec(config_ptr));
	xmlFreeNodeList(config_ptr);
}

#ifdef PION_HAVE_JSON
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkLoadJSONCodec) {
	xmlNodePtr config_ptr(F::createCodecConfig("JSONCodec"));
	BOOST_CHECK_NO_THROW(F::addCodec(config_ptr));
	xmlFreeNodeList(config_ptr);
}
#endif

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkLoadXMLCodec) {
	xmlNodePtr config_ptr(F::createCodecConfig("XMLCodec"));
	BOOST_CHECK_NO_THROW(F::addCodec(config_ptr));
	xmlFreeNodeList(config_ptr);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkLoadMultipleCodecs) {
	xmlNodePtr config_ptr(F::createCodecConfig("LogCodec"));
	BOOST_CHECK_NO_THROW(F::addCodec(config_ptr));
#ifdef PION_HAVE_JSON
	xmlNodeSetContent(config_ptr,  reinterpret_cast<const xmlChar*>("JSONCodec"));
	BOOST_CHECK_NO_THROW(F::addCodec(config_ptr));
#endif
	xmlNodeSetContent(config_ptr,  reinterpret_cast<const xmlChar*>("XMLCodec"));
	BOOST_CHECK_NO_THROW(F::addCodec(config_ptr));
	xmlFreeNodeList(config_ptr);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkLoadUnknownCodec) {
	xmlNodePtr config_ptr(F::createCodecConfig("UnknownCodec"));
	BOOST_CHECK_THROW(F::addCodec(config_ptr), PionPlugin::PluginNotFoundException);
	xmlFreeNodeList(config_ptr);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkSetCodecConfigForMissingCodec) {
	BOOST_CHECK_THROW(F::setCodecConfig(F::m_codec_id, NULL), CodecFactory::CodecNotFoundException);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkRemoveCodec) {
	BOOST_CHECK_THROW(F::removeCodec(F::m_codec_id), CodecFactory::CodecNotFoundException);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetCodec) {
	BOOST_CHECK_THROW(F::getCodec(F::m_codec_id), CodecFactory::CodecNotFoundException);
}

BOOST_AUTO_TEST_SUITE_END()


template<const char* plugin_name>
class CodecFactoryWithCodecLoaded_F : public NewCodecFactory_F {
public:
	CodecFactoryWithCodecLoaded_F() {
		m_plugin_name = plugin_name;
		xmlNodePtr config_ptr(NewCodecFactory_F::createCodecConfig(plugin_name));
		m_codec_id = addCodec(config_ptr);
		xmlFreeNodeList(config_ptr);
	}

	std::string m_plugin_name;
};

typedef boost::mpl::list<CodecFactoryWithCodecLoaded_F<LogCodec_name>,
#ifdef PION_HAVE_JSON
						 CodecFactoryWithCodecLoaded_F<JSONCodec_name>,
#endif
						 CodecFactoryWithCodecLoaded_F<XMLCodec_name> > codec_fixture_list_2;

// CodecFactoryWithCodecLoaded_S contains tests that should pass for any type of Codec
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(CodecFactoryWithCodecLoaded_S, codec_fixture_list_2)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetCodec) {
	BOOST_CHECK(F::getCodec(F::m_codec_id));
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkRemoveCodec) {
	BOOST_CHECK_NO_THROW(F::removeCodec(F::m_codec_id));
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkSetCodecConfigMissingEventType) {
	BOOST_CHECK_THROW(F::setCodecConfig(F::m_codec_id, NULL), Codec::EmptyEventException);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkSetCodecConfigUnknownEventType) {
	xmlNodePtr event_type_node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>("EventType"));
	xmlNodeSetContent(event_type_node,  reinterpret_cast<const xmlChar*>("NotAType"));

	BOOST_CHECK_THROW(F::setCodecConfig(F::m_codec_id, event_type_node), Codec::UnknownTermException);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkSetCodecConfigEventTypeNotAnObject) {
	xmlNodePtr event_type_node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>("EventType"));
	xmlNodeSetContent(event_type_node,  reinterpret_cast<const xmlChar*>("urn:vocab:clickstream#c-ip"));

	BOOST_CHECK_THROW(F::setCodecConfig(F::m_codec_id, event_type_node), Codec::NotAnObjectException);
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkSetNewCodecConfiguration) {
	xmlNodePtr comment_node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>("Comment"));
	xmlNodeSetContent(comment_node,  reinterpret_cast<const xmlChar*>("A new comment"));
	xmlNodePtr event_type_node = xmlNewNode(NULL, reinterpret_cast<const xmlChar*>("EventType"));
	xmlNodeSetContent(event_type_node,  reinterpret_cast<const xmlChar*>("urn:vocab:clickstream#http-event"));
	xmlAddNextSibling(comment_node, event_type_node);

	BOOST_CHECK_NO_THROW(F::setCodecConfig(F::m_codec_id, comment_node));
	xmlFreeNodeList(comment_node);

	// check codec config file
	// ...
}

BOOST_AUTO_TEST_SUITE_END()


// CodecFactoryWithLogCodecLoaded_S contains tests that are specific to LogCodecs
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(CodecFactoryWithLogCodecLoaded_S, boost::mpl::list<CodecFactoryWithCodecLoaded_F<LogCodec_name> >)

//BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkSetCodecConfigX) {
//	...
//	BOOST_CHECK_NO_THROW(F::setCodecConfig(F::m_codec_id, log_codec_options));
//	...
//}

BOOST_AUTO_TEST_SUITE_END()


#ifdef PION_HAVE_JSON
// CodecFactoryWithJSONCodecLoaded_S contains tests that are specific to JSONCodecs
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(CodecFactoryWithJSONCodecLoaded_S, boost::mpl::list<CodecFactoryWithCodecLoaded_F<JSONCodec_name> >)

//BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkSetCodecConfigX) {
//	...
//	BOOST_CHECK_NO_THROW(F::setCodecConfig(F::m_codec_id, json_codec_options));
//	...
//}

BOOST_AUTO_TEST_SUITE_END()
#endif


// CodecFactoryWithXMLCodecLoaded_S contains tests that are specific to XMLCodecs
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(CodecFactoryWithXMLCodecLoaded_S, boost::mpl::list<CodecFactoryWithCodecLoaded_F<XMLCodec_name> >)

//BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkSetCodecConfigX) {
//	...
//	BOOST_CHECK_NO_THROW(F::setCodecConfig(F::m_codec_id, xml_codec_options));
//	...
//}

BOOST_AUTO_TEST_SUITE_END()


class CodecFactoryWithMultipleCodecsLoaded_F : public NewCodecFactory_F {
public:
	CodecFactoryWithMultipleCodecsLoaded_F() {
		xmlNodePtr config_ptr(NewCodecFactory_F::createCodecConfig(LogCodec_name));
		m_LogCodec_id = addCodec(config_ptr);
#ifdef PION_HAVE_JSON
		xmlNodeSetContent(config_ptr,  reinterpret_cast<const xmlChar*>(JSONCodec_name));
		m_JSONCodec_id = addCodec(config_ptr);
#endif
		xmlNodeSetContent(config_ptr,  reinterpret_cast<const xmlChar*>(XMLCodec_name));
		m_XMLCodec_id = addCodec(config_ptr);
		xmlFreeNodeList(config_ptr);
	}

	std::string m_LogCodec_id;
	std::string m_JSONCodec_id;
	std::string m_XMLCodec_id;
};

BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(CodecFactoryWithMultipleCodecsLoaded_S, 
									   boost::mpl::list<CodecFactoryWithMultipleCodecsLoaded_F>)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetCodec) {
	BOOST_CHECK(F::getCodec(F::m_LogCodec_id));
#ifdef PION_HAVE_JSON
	BOOST_CHECK(F::getCodec(F::m_JSONCodec_id));
#endif
	BOOST_CHECK(F::getCodec(F::m_XMLCodec_id));
}

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkRemoveCodec) {
	BOOST_CHECK_NO_THROW(F::removeCodec(F::m_LogCodec_id));
#ifdef PION_HAVE_JSON
	BOOST_CHECK_NO_THROW(F::removeCodec(F::m_JSONCodec_id));
#endif
	BOOST_CHECK_NO_THROW(F::removeCodec(F::m_XMLCodec_id));
}

// TODO: check that all the codecs got their vocabulary updated
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkUpdateVocabulary) {
	BOOST_CHECK_NO_THROW(F::updateVocabulary());
}

BOOST_AUTO_TEST_SUITE_END()


template<const char* plugin_name>
class CodecFactoryWithCodecPtr_F : public CodecFactoryWithCodecLoaded_F<plugin_name> {
public:
	CodecFactoryWithCodecPtr_F() {
		BOOST_REQUIRE(m_codec_ptr = this->getCodec(this->m_codec_id));
	}

	CodecPtr m_codec_ptr;
};

typedef boost::mpl::list<CodecFactoryWithCodecPtr_F<LogCodec_name>,
#ifdef PION_HAVE_JSON
						 CodecFactoryWithCodecPtr_F<JSONCodec_name>,
#endif
						 CodecFactoryWithCodecPtr_F<XMLCodec_name> > codec_fixture_list_3;

// CodecFactoryWithCodecPtr_S contains tests that should pass for any type of Codec.
// It's empty now because the tests that were in it are now in ConfiguredCodecPtr_S,
// but I'll leave it for now since the fixture's still being used, and it might be
// a good place for tests that are specific to factory generated Codecs and need
// access to the factory.
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(CodecFactoryWithCodecPtr_S, codec_fixture_list_3)

BOOST_AUTO_TEST_SUITE_END()


// CodecFactoryWithLogCodecPtr_S contains tests that are specific to LogCodecs
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(CodecFactoryWithLogCodecPtr_S, boost::mpl::list<CodecFactoryWithCodecPtr_F<LogCodec_name> >)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetContentType) {
	BOOST_CHECK_EQUAL(F::m_codec_ptr->getContentType(), "text/ascii");
}

BOOST_AUTO_TEST_SUITE_END()


#ifdef PION_HAVE_JSON
// CodecFactoryWithJSONCodecPtr_S contains tests that are specific to JSONCodecs
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(CodecFactoryWithJSONCodecPtr_S, boost::mpl::list<CodecFactoryWithCodecPtr_F<JSONCodec_name> >)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetContentType) {
	BOOST_CHECK_EQUAL(F::m_codec_ptr->getContentType(), "text/json");
}

BOOST_AUTO_TEST_SUITE_END()
#endif


// CodecFactoryWithXMLCodecPtr_S contains tests that are specific to XMLCodecs
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(CodecFactoryWithXMLCodecPtr_S, boost::mpl::list<CodecFactoryWithCodecPtr_F<XMLCodec_name> >)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkGetContentType) {
	BOOST_CHECK_EQUAL(F::m_codec_ptr->getContentType(), "text/xml");
}

BOOST_AUTO_TEST_SUITE_END()


class CodecFactoryLogFormatTests_F : public CodecFactory {
public:
	CodecFactoryLogFormatTests_F()
		: CodecFactory(m_vocab_mgr),
		m_common_id("a174c3b0-bfcd-11dc-9db2-0016cb926e68"),
		m_combined_id("3f49f2da-bfe3-11dc-8875-0016cb926e68"),
		m_extended_id("23f68d5a-bfec-11dc-81a7-0016cb926e68"),
		m_justdate_id("dba9eac2-d8bb-11dc-bebe-001cc02bd66b")
	{
		cleanup_codec_config_files(false);
		boost::filesystem::copy_file(CODECS_TEMPLATE_FILE, CODECS_CONFIG_FILE);

		if (! m_config_loaded) {
			// load the CLF vocabulary
			m_vocab_mgr.setConfigFile(VOCABS_CONFIG_FILE);
			m_vocab_mgr.openConfigFile();
			m_config_loaded = true;
		}

		setConfigFile(CODECS_CONFIG_FILE);
		openConfigFile();

		m_common_codec = getCodec(m_common_id);
		BOOST_CHECK(m_common_codec);

		m_combined_codec = getCodec(m_combined_id);
		BOOST_CHECK(m_combined_codec);

		m_extended_codec = getCodec(m_extended_id);
		BOOST_CHECK(m_extended_codec);

		m_date_codec = getCodec(m_justdate_id);
		BOOST_CHECK(m_date_codec);
		
		m_vocab_ptr = m_vocab_mgr.getVocabulary();

		m_remotehost_ref = m_vocab_ptr->findTerm("urn:vocab:clickstream#c-ip");
		BOOST_REQUIRE(m_remotehost_ref != Vocabulary::UNDEFINED_TERM_REF);

		m_rfc931_ref = m_vocab_ptr->findTerm("urn:vocab:clickstream#rfc931");
		BOOST_REQUIRE(m_rfc931_ref != Vocabulary::UNDEFINED_TERM_REF);

		m_authuser_ref = m_vocab_ptr->findTerm("urn:vocab:clickstream#authuser");
		BOOST_REQUIRE(m_authuser_ref != Vocabulary::UNDEFINED_TERM_REF);

		m_date_ref = m_vocab_ptr->findTerm("urn:vocab:clickstream#clf-date");
		BOOST_REQUIRE(m_date_ref != Vocabulary::UNDEFINED_TERM_REF);

		m_request_ref = m_vocab_ptr->findTerm("urn:vocab:clickstream#request");
		BOOST_REQUIRE(m_request_ref != Vocabulary::UNDEFINED_TERM_REF);

		m_status_ref = m_vocab_ptr->findTerm("urn:vocab:clickstream#status");
		BOOST_REQUIRE(m_status_ref != Vocabulary::UNDEFINED_TERM_REF);

		m_bytes_ref = m_vocab_ptr->findTerm("urn:vocab:clickstream#bytes");
		BOOST_REQUIRE(m_bytes_ref != Vocabulary::UNDEFINED_TERM_REF);

		m_referer_ref = m_vocab_ptr->findTerm("urn:vocab:clickstream#referer");
		BOOST_REQUIRE(m_referer_ref != Vocabulary::UNDEFINED_TERM_REF);

		m_useragent_ref = m_vocab_ptr->findTerm("urn:vocab:clickstream#useragent");
		BOOST_REQUIRE(m_useragent_ref != Vocabulary::UNDEFINED_TERM_REF);
	}
	~CodecFactoryLogFormatTests_F() {}

	EventFactory		m_event_factory;
	const std::string	m_common_id;
	const std::string	m_combined_id;
	const std::string	m_extended_id;
	const std::string	m_justdate_id;
	CodecPtr			m_common_codec;
	CodecPtr			m_combined_codec;
	CodecPtr			m_extended_codec;
	CodecPtr			m_date_codec;
	Vocabulary::TermRef	m_remotehost_ref;
	Vocabulary::TermRef	m_rfc931_ref;
	Vocabulary::TermRef	m_authuser_ref;
	Vocabulary::TermRef	m_date_ref;
	Vocabulary::TermRef	m_request_ref;
	Vocabulary::TermRef	m_status_ref;
	Vocabulary::TermRef	m_bytes_ref;
	Vocabulary::TermRef	m_referer_ref;
	Vocabulary::TermRef	m_useragent_ref;
	VocabularyPtr		m_vocab_ptr;

	static VocabularyManager m_vocab_mgr;
	static bool	m_config_loaded;
};

VocabularyManager	CodecFactoryLogFormatTests_F::m_vocab_mgr;
bool				CodecFactoryLogFormatTests_F::m_config_loaded = false;

// CodecFactoryWithCodecLoaded_S contains tests for the common log format
BOOST_FIXTURE_TEST_SUITE(CodecFactoryLogFormatTests_S, CodecFactoryLogFormatTests_F)

BOOST_AUTO_TEST_CASE(checkGetCodec) {
	BOOST_CHECK(getCodec(m_common_id));
	BOOST_CHECK(getCodec(m_combined_id));
	BOOST_CHECK(getCodec(m_extended_id));
}

BOOST_AUTO_TEST_CASE(checkCommonCodecEventTypes) {
	const Event::EventType event_type_ref = m_vocab_ptr->findTerm("urn:vocab:clickstream#http-event");
	BOOST_CHECK_EQUAL(m_common_codec->getEventType(), event_type_ref);
	BOOST_CHECK_EQUAL(m_combined_codec->getEventType(), event_type_ref);
	BOOST_CHECK_EQUAL(m_extended_codec->getEventType(), event_type_ref);
}

BOOST_AUTO_TEST_CASE(checkCommonCodecName) {
	BOOST_CHECK_EQUAL(m_common_codec->getName(), "Common Log Format");
	BOOST_CHECK_EQUAL(m_combined_codec->getName(), "Combined Log Format");
	BOOST_CHECK_EQUAL(m_extended_codec->getName(), "Extended Log Format");
}

BOOST_AUTO_TEST_CASE(checkCommonCodecComment) {
	BOOST_CHECK_EQUAL(m_common_codec->getComment(), "Codec for the Common Log Format (CLF)");
	BOOST_CHECK_EQUAL(m_combined_codec->getComment(), "Codec for the Combined Log Format (DLF)");
	BOOST_CHECK_EQUAL(m_extended_codec->getComment(), "Codec for the Extended Log Format (ELF)");
}

BOOST_AUTO_TEST_CASE(checkJustDateCodecReadEntry) {
	BOOST_REQUIRE(m_date_codec);
	std::stringstream ss("\"05/Apr/2007:05:37:11 -0600\"\n");
	EventPtr event_ptr(m_event_factory.create(m_date_codec->getEventType()));
	BOOST_REQUIRE(m_date_codec->read(ss, *event_ptr));
	BOOST_CHECK_EQUAL(event_ptr->getDateTime(m_date_ref).date(),
					  boost::gregorian::date(2007, 4, 5));
}

BOOST_AUTO_TEST_CASE(checkCommonCodecReadLogFile) {
	// open the CLF log file
	std::ifstream in;
	in.open(COMMON_LOG_FILE.c_str(), std::ios::in);
	BOOST_REQUIRE(in.is_open());

	// read the first record
	EventPtr event_ptr(m_event_factory.create(m_common_codec->getEventType()));
	BOOST_REQUIRE(m_common_codec->read(in, *event_ptr));
	// check the first data
	BOOST_CHECK_EQUAL(event_ptr->getString(m_remotehost_ref), "10.0.19.111");
	BOOST_CHECK(! event_ptr->isDefined(m_rfc931_ref));
	BOOST_CHECK(! event_ptr->isDefined(m_authuser_ref));
	// NOTE: timezone offsets are currently not working in DateTimeFacet
	BOOST_CHECK_EQUAL(event_ptr->getDateTime(m_date_ref),
					  PionDateTime(boost::gregorian::date(2007, 4, 5),
								   boost::posix_time::time_duration(5, 37, 11)));
	BOOST_CHECK_EQUAL(event_ptr->getString(m_request_ref), "GET /robots.txt HTTP/1.0");
	BOOST_CHECK_EQUAL(event_ptr->getUInt(m_status_ref), 404UL);
	BOOST_CHECK_EQUAL(event_ptr->getUBigInt(m_bytes_ref), 208UL);

	// read the second record
	event_ptr->clear();
	BOOST_REQUIRE(m_common_codec->read(in, *event_ptr));
	// check the second data
	BOOST_CHECK_EQUAL(event_ptr->getString(m_remotehost_ref), "10.0.31.104");
	BOOST_CHECK_EQUAL(event_ptr->getString(m_rfc931_ref), "ab");
	BOOST_CHECK(! event_ptr->isDefined(m_authuser_ref));
	// NOTE: timezone offsets are currently not working in DateTimeFacet
	BOOST_CHECK_EQUAL(event_ptr->getDateTime(m_date_ref),
					  PionDateTime(boost::gregorian::date(2007, 6, 8),
								   boost::posix_time::time_duration(7, 20, 2)));
	BOOST_CHECK_EQUAL(event_ptr->getString(m_request_ref), "GET /community/ HTTP/1.1");
	BOOST_CHECK_EQUAL(event_ptr->getUInt(m_status_ref), 200UL);
	BOOST_CHECK_EQUAL(event_ptr->getUBigInt(m_bytes_ref), 3546UL);

	// read the third record
	event_ptr->clear();
	BOOST_REQUIRE(m_common_codec->read(in, *event_ptr));
	// check the third data
	BOOST_CHECK_EQUAL(event_ptr->getString(m_remotehost_ref), "10.0.2.104");
	BOOST_CHECK(! event_ptr->isDefined(m_rfc931_ref));
	BOOST_CHECK_EQUAL(event_ptr->getString(m_authuser_ref), "cd");
	// NOTE: timezone offsets are currently not working in DateTimeFacet
	BOOST_CHECK_EQUAL(event_ptr->getDateTime(m_date_ref),
					  PionDateTime(boost::gregorian::date(2007, 9, 24),
								   boost::posix_time::time_duration(12, 13, 3)));
	BOOST_CHECK_EQUAL(event_ptr->getString(m_request_ref), "GET /default.css HTTP/1.1");
	BOOST_CHECK_EQUAL(event_ptr->getUInt(m_status_ref), 200UL);
	BOOST_CHECK_EQUAL(event_ptr->getUBigInt(m_bytes_ref), 6698UL);

	// read the forth record
	event_ptr->clear();
	BOOST_REQUIRE(m_common_codec->read(in, *event_ptr));
	// check the forth data
	BOOST_CHECK_EQUAL(event_ptr->getString(m_remotehost_ref), "10.0.141.122");
	BOOST_CHECK_EQUAL(event_ptr->getString(m_rfc931_ref), "ef");
	BOOST_CHECK_EQUAL(event_ptr->getString(m_authuser_ref), "gh");
	// NOTE: timezone offsets are currently not working in DateTimeFacet
	BOOST_CHECK_EQUAL(event_ptr->getDateTime(m_date_ref),
					  PionDateTime(boost::gregorian::date(2008, 1, 30),
								   boost::posix_time::time_duration(15, 26, 7)));
	BOOST_CHECK_EQUAL(event_ptr->getString(m_request_ref), "GET /pion/ HTTP/1.1");
	BOOST_CHECK_EQUAL(event_ptr->getUInt(m_status_ref), 200UL);
	BOOST_CHECK_EQUAL(event_ptr->getUBigInt(m_bytes_ref), 7058UL);
}

BOOST_AUTO_TEST_CASE(checkCommonCodecWriteLogFormatJustOneField) {
	EventPtr event_ptr(m_event_factory.create(m_common_codec->getEventType()));
	event_ptr->setString(m_remotehost_ref, "192.168.0.1");
	std::stringstream ss;
	m_common_codec->write(ss, *event_ptr);
	BOOST_CHECK_EQUAL(ss.str(), "192.168.0.1 - - [] \"\" - -" OSEOL);
}

BOOST_AUTO_TEST_CASE(checkCommonCodecWriteLogFormatAllFields) {
	EventPtr event_ptr(m_event_factory.create(m_common_codec->getEventType()));
	event_ptr->setString(m_remotehost_ref, "192.168.10.10");
	event_ptr->setString(m_rfc931_ref, "greg");
	event_ptr->setString(m_authuser_ref, "bob");
	event_ptr->setDateTime(m_date_ref, PionDateTime(boost::gregorian::date(2008, 1, 10),
													boost::posix_time::time_duration(12, 31, 0)));
	event_ptr->setString(m_request_ref, "GET / HTTP/1.1");
	event_ptr->setUInt(m_status_ref, 302);
	event_ptr->setUBigInt(m_bytes_ref, 116);
	std::stringstream ss;
	m_common_codec->write(ss, *event_ptr);
	// NOTE: timezone offsets are currently not working in DateTimeFacet
	BOOST_CHECK_EQUAL(ss.str(), "192.168.10.10 greg bob [10/Jan/2008:12:31:00 ] \"GET / HTTP/1.1\" 302 116" OSEOL);
}

BOOST_AUTO_TEST_CASE(checkCommonCodecReadLogFormatAllFieldsWithQuotes) {
	std::string log_entry("192.168.10.10 greg bob [10/Jan/2008:12:31:00 ] \"GET /\\\" HTTP/1.1\" 302 116" OSEOL);
	std::stringstream ss(log_entry);

	EventPtr event_ptr(m_event_factory.create(m_common_codec->getEventType()));
	BOOST_REQUIRE(m_common_codec->read(ss, *event_ptr));

	BOOST_CHECK_EQUAL(event_ptr->getString(m_remotehost_ref), "192.168.10.10");
	BOOST_CHECK_EQUAL(event_ptr->getString(m_rfc931_ref), "greg");
	BOOST_CHECK_EQUAL(event_ptr->getString(m_authuser_ref), "bob");
	// NOTE: timezone offsets are currently not working in DateTimeFacet
	BOOST_CHECK_EQUAL(event_ptr->getDateTime(m_date_ref),
					  PionDateTime(boost::gregorian::date(2008, 1, 10),
								   boost::posix_time::time_duration(12, 31, 0)));
	BOOST_CHECK_EQUAL(event_ptr->getString(m_request_ref), "GET /\" HTTP/1.1");
	BOOST_CHECK_EQUAL(event_ptr->getUInt(m_status_ref), 302UL);
	BOOST_CHECK_EQUAL(event_ptr->getUBigInt(m_bytes_ref), 116UL);
}

BOOST_AUTO_TEST_CASE(checkCombinedCodecReadLogFile) {
	// open the CLF log file
	std::ifstream in;
	in.open(COMBINED_LOG_FILE.c_str(), std::ios::in);
	BOOST_REQUIRE(in.is_open());

	// read the first record
	EventPtr event_ptr(m_event_factory.create(m_combined_codec->getEventType()));
	BOOST_REQUIRE(m_combined_codec->read(in, *event_ptr));
	BOOST_CHECK_EQUAL(event_ptr->getString(m_referer_ref), "http://www.example.com/start.html");
	BOOST_CHECK_EQUAL(event_ptr->getString(m_useragent_ref), "Mozilla/4.08 [en] (Win98; I ;Nav)");

	// read the second record
	event_ptr->clear();
	BOOST_REQUIRE(m_combined_codec->read(in, *event_ptr));
	BOOST_CHECK_EQUAL(event_ptr->getString(m_referer_ref), "http://www.atomiclabs.com/");
	BOOST_CHECK_EQUAL(event_ptr->getString(m_useragent_ref), "Mozilla/4.08 [en] (Win98; I ;Nav)");

	// read the third record
	event_ptr->clear();
	BOOST_REQUIRE(m_combined_codec->read(in, *event_ptr));
	BOOST_CHECK_EQUAL(event_ptr->getString(m_referer_ref), "http://www.google.com/");
	BOOST_CHECK_EQUAL(event_ptr->getString(m_useragent_ref), "Mozilla/5.0 (Macintosh; U; PPC Mac OS X Mach-O; en-US; rv:1.7a) Gecko/20040614 Firefox/0.9.0+");

	// read the forth record
	event_ptr->clear();
	BOOST_REQUIRE(m_combined_codec->read(in, *event_ptr));
	BOOST_CHECK_EQUAL(event_ptr->getString(m_referer_ref), "http://www.wikipedia.com/");
	BOOST_CHECK_EQUAL(event_ptr->getString(m_useragent_ref), "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1)");
}

BOOST_AUTO_TEST_CASE(checkCombinedCodecWriteJustExtraFields) {
	EventPtr event_ptr(m_event_factory.create(m_combined_codec->getEventType()));
	event_ptr->setString(m_referer_ref, "http://www.atomiclabs.com/");
	event_ptr->setString(m_useragent_ref, "Mozilla/4.08 [en] (Win98; I ;Nav)");
	std::stringstream ss;
	m_combined_codec->write(ss, *event_ptr);
	BOOST_CHECK_EQUAL(ss.str(), "- - - [] \"\" - - \"http://www.atomiclabs.com/\" \"Mozilla/4.08 [en] (Win98; I ;Nav)\"" OSEOL);
}

BOOST_AUTO_TEST_CASE(checkExtendedCodecReadLogFile) {
	// open the CLF log file
	std::ifstream in;
	in.open(EXTENDED_LOG_FILE.c_str(), std::ios::in);
	BOOST_REQUIRE(in.is_open());

	// read the first record
	EventPtr event_ptr(m_event_factory.create(m_extended_codec->getEventType()));
	BOOST_REQUIRE(m_extended_codec->read(in, *event_ptr));
	// check the third data
	BOOST_CHECK_EQUAL(event_ptr->getString(m_remotehost_ref), "10.0.2.104");
	// NOTE: timezone offsets are currently not working in DateTimeFacet
	BOOST_CHECK_EQUAL(event_ptr->getDateTime(m_date_ref),
					  PionDateTime(boost::gregorian::date(2007, 9, 24),
								   boost::posix_time::time_duration(12, 13, 3)));
	BOOST_CHECK_EQUAL(event_ptr->getString(m_request_ref), "GET /default.css HTTP/1.1");
	BOOST_CHECK_EQUAL(event_ptr->getUInt(m_status_ref), 200UL);

	// read the second record
	event_ptr->clear();
	BOOST_REQUIRE(m_extended_codec->read(in, *event_ptr));
	// check the forth data
	BOOST_CHECK_EQUAL(event_ptr->getString(m_remotehost_ref), "10.0.141.122");
	// NOTE: timezone offsets are currently not working in DateTimeFacet
	BOOST_CHECK_EQUAL(event_ptr->getDateTime(m_date_ref),
					  PionDateTime(boost::gregorian::date(2008, 1, 30),
								   boost::posix_time::time_duration(15, 26, 7)));
	BOOST_CHECK_EQUAL(event_ptr->getString(m_request_ref), "GET /pion/ HTTP/1.1");
	BOOST_CHECK_EQUAL(event_ptr->getUInt(m_status_ref), 200UL);

	// read the third record
	event_ptr->clear();
	BOOST_REQUIRE(m_extended_codec->read(in, *event_ptr));
	// check the first data
	BOOST_CHECK_EQUAL(event_ptr->getString(m_remotehost_ref), "10.0.19.111");
	// NOTE: timezone offsets are currently not working in DateTimeFacet
	BOOST_CHECK_EQUAL(event_ptr->getDateTime(m_date_ref),
					  PionDateTime(boost::gregorian::date(2007, 4, 5),
								   boost::posix_time::time_duration(5, 37, 11)));
	BOOST_CHECK_EQUAL(event_ptr->getString(m_request_ref), "GET /robots.txt HTTP/1.0");
	BOOST_CHECK_EQUAL(event_ptr->getUInt(m_status_ref), 404UL);

	// read the forth record
	event_ptr->clear();
	BOOST_REQUIRE(m_extended_codec->read(in, *event_ptr));
	// check the second data
	BOOST_CHECK_EQUAL(event_ptr->getString(m_remotehost_ref), "10.0.31.104");
	// NOTE: timezone offsets are currently not working in DateTimeFacet
	BOOST_CHECK_EQUAL(event_ptr->getDateTime(m_date_ref),
					  PionDateTime(boost::gregorian::date(2007, 6, 8),
								   boost::posix_time::time_duration(7, 20, 2)));
	BOOST_CHECK_EQUAL(event_ptr->getString(m_request_ref), "GET /community/ HTTP/1.1");
	BOOST_CHECK_EQUAL(event_ptr->getUInt(m_status_ref), 200UL);
}

BOOST_AUTO_TEST_CASE(checkExtendedCodecWrite) {
	EventPtr event_ptr(m_event_factory.create(m_extended_codec->getEventType()));
	event_ptr->setString(m_remotehost_ref, "192.168.10.10");
	event_ptr->setDateTime(m_date_ref, PionDateTime(boost::gregorian::date(2008, 1, 10),
										   boost::posix_time::time_duration(12, 31, 0)));
	event_ptr->setString(m_request_ref, "GET / HTTP/1.1");
	event_ptr->setString(m_referer_ref, "http://www.atomiclabs.com/");
	event_ptr->setUInt(m_status_ref, 302);
	std::stringstream ss;
	m_extended_codec->write(ss, *event_ptr);
	m_extended_codec->write(ss, *event_ptr);
	std::string str = ss.str();
	size_t start = str.find("#Date: ");
	BOOST_REQUIRE(start != std::string::npos);
	size_t end = str.find("#Software: ");
	BOOST_REQUIRE(end != std::string::npos);
	str.replace(start + 7, end - (start + 7), "..." OSEOL);
	BOOST_CHECK_EQUAL(str, "#Version: 1.0" OSEOL "#Date: ..." OSEOL "#Software: Pion v" PION_VERSION OSEOL "#Fields: clf-date c-ip request cs(Referer) status" OSEOL "\"10/Jan/2008:12:31:00 \" 192.168.10.10 \"GET / HTTP/1.1\" \"http://www.atomiclabs.com/\" 302" OSEOL "\"10/Jan/2008:12:31:00 \" 192.168.10.10 \"GET / HTTP/1.1\" \"http://www.atomiclabs.com/\" 302" OSEOL);
}

BOOST_AUTO_TEST_SUITE_END()
