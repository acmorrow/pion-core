// -----------------------------------------------------------------------
// pion-common: a collection of common libraries used by the Pion Platform
// -----------------------------------------------------------------------
// Copyright (C) 2007-2008 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_PIONHASHMAP_HEADER__
#define __PION_PIONHASHMAP_HEADER__

#include <string>
#include <cctype>
#include <unordered_map>

namespace pion {	// begin namespace pion

/// returns true if two strings are equal (ignoring case)
struct CaseInsensitiveEqual {
	inline bool operator()(const std::string& str1, const std::string& str2) const {
		if (str1.size() != str2.size())
			return false;
		std::string::const_iterator it1 = str1.begin();
		std::string::const_iterator it2 = str2.begin();
		while ( (it1!=str1.end()) && (it2!=str2.end()) ) {
			if (tolower(*it1) != tolower(*it2))
				return false;
			++it1;
			++it2;
		}
		return true;
	}
};


/// case insensitive hash function for std::string
struct CaseInsensitiveHash {
	inline unsigned long operator()(const std::string& str) const {
		unsigned long value = 0;
		for (std::string::const_iterator i = str.begin(); i!= str.end(); ++i)
			value = static_cast<unsigned char>(tolower(*i)) + (value << 6) + (value << 16) - value;
		return value;
	}
};


/// returns true if str1 < str2 (ignoring case)
struct CaseInsensitiveLess {
	inline bool operator()(const std::string& str1, const std::string& str2) const {
		std::string::const_iterator it1 = str1.begin();
		std::string::const_iterator it2 = str2.begin();
		while ( (it1 != str1.end()) && (it2 != str2.end()) ) {
			if (tolower(*it1) != tolower(*it2))
				return (tolower(*it1) < tolower(*it2));
			++it1;
			++it2;
		}
		return (str1.size() < str2.size());
	}
};


	typedef std::unordered_map<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual >	StringDictionary;

}	// end namespace pion

#endif
