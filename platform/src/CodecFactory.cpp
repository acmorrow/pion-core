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

#include <pion/platform/CodecFactory.hpp>


namespace pion {		// begin namespace pion
namespace platform {	// begin namespace platform (Pion Platform Library)


// static members of CodecFactory
const std::string			CodecFactory::DEFAULT_CONFIG_FILE = "codecs.xml";
const std::string			CodecFactory::CODEC_ELEMENT_NAME = "Codec";
const std::string			CodecFactory::CODECS_PERMISSION_TYPE = "Codecs";


// CodecFactory member functions

CodecFactory::CodecFactory(const VocabularyManager& vocab_mgr)
	: PluginConfig<Codec>(vocab_mgr, DEFAULT_CONFIG_FILE, CODEC_ELEMENT_NAME)
{
	setLogger(PION_GET_LOGGER("pion.platform.CodecFactory"));
}
	
CodecPtr CodecFactory::getCodec(const std::string& codec_id)
{
	std::lock_guard<std::mutex> factory_lock(m_mutex);
	Codec *codec_ptr = m_plugins.get(codec_id);
	// throw an exception if the codec was not found
	if (codec_ptr == NULL)
		throw CodecNotFoundException(codec_id);
	// return a cloned instance of the Codec since its state may change
	// while encoding or decoding data streams
	return codec_ptr->clone();
}

void CodecFactory::setCodecConfig(const std::string& codec_id,
								  const xmlNodePtr config_ptr)
{
	// convert PluginNotFound exceptions into CodecNotFound exceptions
	try {
		PluginConfig<Codec>::setPluginConfig(codec_id, config_ptr);
	} catch (PluginManager<Codec>::PluginNotFoundException&) {
		throw CodecNotFoundException(codec_id);
	}
}

std::string CodecFactory::addCodec(const xmlNodePtr config_ptr)
{
	return PluginConfig<Codec>::addPlugin(config_ptr);
}

void CodecFactory::removeCodec(const std::string& codec_id) {
	// convert PluginNotFound exceptions into CodecNotFound exceptions
	try {
		PluginConfig<Codec>::removePlugin(codec_id);
	} catch (PluginManager<Codec>::PluginNotFoundException&) {
		throw CodecNotFoundException(codec_id);
	}
}

	
}	// end namespace platform
}	// end namespace pion
