//========================================================================
//
// UnicodeMap.h
//
// Mapping from Unicode to an encoding.
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

//========================================================================
//
// Modified under the Poppler project - http://poppler.freedesktop.org
//
// All changes made under the Poppler project to this file are licensed
// under GPL version 2 or later
//
// Copyright (C) 2017 Adrian Johnson <ajohnson@redneon.com>
// Copyright (C) 2018, 2019 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2018 Adam Reichold <adam.reichold@t-online.de>
// Copyright (C) 2019 Volker Krause <vkrause@kde.org>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#ifndef UNICODEMAP_H
#define UNICODEMAP_H

#include "poppler-config.h"
#include "CharTypes.h"
#include <atomic>

class GooString;

//------------------------------------------------------------------------

enum UnicodeMapKind {
  unicodeMapUser,		// read from a file
  unicodeMapResident,		// static list of ranges
  unicodeMapFunc		// function pointer
};

typedef int (*UnicodeMapFunc)(Unicode u, char *buf, int bufSize);

struct UnicodeMapRange {
  Unicode start, end;		// range of Unicode chars
  unsigned int code, nBytes;		// first output code
};

struct UnicodeMapExt;

//------------------------------------------------------------------------

class UnicodeMap {
public:

  // Create the UnicodeMap specified by <encodingName>.  Sets the
  // initial reference count to 1.  Returns NULL on failure.
  static UnicodeMap *parse(GooString *encodingNameA);

  // Create a resident UnicodeMap.
  UnicodeMap(const char *encodingNameA, bool unicodeOutA,
	     const UnicodeMapRange *rangesA, int lenA);

  // Create a resident UnicodeMap that uses a function instead of a
  // list of ranges.
  UnicodeMap(const char *encodingNameA, bool unicodeOutA,
	     UnicodeMapFunc funcA);

  UnicodeMap(UnicodeMap &&other) noexcept;
  UnicodeMap& operator=(UnicodeMap &&other) noexcept;

  void swap(UnicodeMap& other) noexcept;

  ~UnicodeMap();

  UnicodeMap(const UnicodeMap &) = delete;
  UnicodeMap& operator=(const UnicodeMap &) = delete;

  void incRefCnt();
  void decRefCnt();

  const GooString *getEncodingName() const { return encodingName; }

  bool isUnicode() const { return unicodeOut; }

  // Return true if this UnicodeMap matches the specified
  // <encodingNameA>.
  bool match(const GooString *encodingNameA) const;

  // Map Unicode to the target encoding.  Fills in <buf> with the
  // output and returns the number of bytes used.  Output will be
  // truncated at <bufSize> bytes.  No string terminator is written.
  // Returns 0 if no mapping is found.
  int mapUnicode(Unicode u, char *buf, int bufSize);

private:

  UnicodeMap(GooString *encodingNameA);

  GooString *encodingName;
  UnicodeMapKind kind;
  bool unicodeOut;
  union {
    const UnicodeMapRange *ranges;	// (user, resident)
    UnicodeMapFunc func;	// (func)
  };
  int len;			// (user, resident)
  UnicodeMapExt *eMaps;		// (user)
  int eMapsLen;			// (user)
  std::atomic_int refCnt;
};

//------------------------------------------------------------------------

#define unicodeMapCacheSize 4

class UnicodeMapCache {
public:

  UnicodeMapCache();
  ~UnicodeMapCache();

  UnicodeMapCache(const UnicodeMapCache &) = delete;
  UnicodeMapCache& operator=(const UnicodeMapCache &) = delete;

  // Get the UnicodeMap for <encodingName>.  Increments its reference
  // count; there will be one reference for the cache plus one for the
  // caller of this function.  Returns NULL on failure.
  UnicodeMap *getUnicodeMap(GooString *encodingName);

private:

  UnicodeMap *cache[unicodeMapCacheSize];
};

#endif
