//========================================================================
//
// GlobalParams.cc
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
// Copyright (C) 2005 Martin Kretzschmar <martink@gnome.org>
// Copyright (C) 2005, 2006 Kristian Høgsberg <krh@redhat.com>
// Copyright (C) 2005, 2007-2010, 2012, 2015, 2017, 2018 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2005 Jonathan Blandford <jrb@redhat.com>
// Copyright (C) 2006, 2007 Jeff Muizelaar <jeff@infidigm.net>
// Copyright (C) 2006 Takashi Iwai <tiwai@suse.de>
// Copyright (C) 2006 Ed Catmur <ed@catmur.co.uk>
// Copyright (C) 2007 Krzysztof Kowalczyk <kkowalczyk@gmail.com>
// Copyright (C) 2007, 2009 Jonathan Kew <jonathan_kew@sil.org>
// Copyright (C) 2009 Petr Gajdos <pgajdos@novell.com>
// Copyright (C) 2009, 2011, 2012, 2015 William Bader <williambader@hotmail.com>
// Copyright (C) 2009 Kovid Goyal <kovid@kovidgoyal.net>
// Copyright (C) 2010, 2012 Hib Eris <hib@hiberis.nl>
// Copyright (C) 2010 Patrick Spendrin <ps_ml@gmx.de>
// Copyright (C) 2010 Jakub Wilk <jwilk@jwilk.net>
// Copyright (C) 2011 Pino Toscano <pino@kde.org>
// Copyright (C) 2011 Koji Otani <sho@bbr.jp>
// Copyright (C) 2012 Yi Yang <ahyangyi@gmail.com>
// Copyright (C) 2012, 2017 Adrian Johnson <ajohnson@redneon.com>
// Copyright (C) 2012 Thomas Freitag <Thomas.Freitag@alfa.de>
// Copyright (C) 2012 Peter Breitenlohner <peb@mppmu.mpg.de>
// Copyright (C) 2013, 2014 Jason Crain <jason@aquaticape.us>
// Copyright (C) 2017 Christoph Cullmann <cullmann@kde.org>
// Copyright (C) 2017 Jean Ghali <jghali@libertysurf.fr>
// Copyright (C) 2018 Klarälvdalens Datakonsult AB, a KDAB Group company, <info@kdab.com>. Work sponsored by the LiMux project of the city of Munich
// Copyright (C) 2018 Adam Reichold <adam.reichold@t-online.de>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#include <config.h>

#ifdef USE_GCC_PRAGMAS
#pragma implementation
#endif

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#ifdef _WIN32
#  include <shlobj.h>
#  include <mbstring.h>
#endif
#include "goo/glibc.h"
#include "goo/gmem.h"
#include "goo/GooString.h"
#include "goo/GooList.h"
#include "goo/gfile.h"
#include "goo/gdir.h"
#include "Error.h"
#include "NameToCharCode.h"
#include "CharCodeToUnicode.h"
#include "UnicodeMap.h"
#include "CMap.h"
#include "BuiltinFontTables.h"
#include "FontEncodingTables.h"
#include "GlobalParams.h"
#include "GfxFont.h"

#ifdef WITH_FONTCONFIGURATION_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

#ifdef _MSC_VER
#  define strcasecmp stricmp
#else
#  include <strings.h>
#endif

#ifndef FC_WEIGHT_BOOK
#define FC_WEIGHT_BOOK 75
#endif

#include "NameToUnicodeTable.h"
#include "UnicodeMapTables.h"
#include "UnicodeMapFuncs.h"

//------------------------------------------------------------------------

#define cidToUnicodeCacheSize     4
#define unicodeToUnicodeCacheSize 4

//------------------------------------------------------------------------

GlobalParams *globalParams = nullptr;

#if defined(ENABLE_RELOCATABLE) && defined(_WIN32)

/* search for data relative to where we are installed */

static HMODULE hmodule;

extern "C" {
  /* Provide declaration to squelch -Wmissing-declarations warning */
  BOOL WINAPI
  DllMain (HINSTANCE hinstDLL,
	   DWORD     fdwReason,
	   LPVOID    lpvReserved);

  BOOL WINAPI
  DllMain (HINSTANCE hinstDLL,
	   DWORD     fdwReason,
	   LPVOID    lpvReserved)
  {
    switch (fdwReason) {
      case DLL_PROCESS_ATTACH:
	hmodule = hinstDLL;
	break;
    }

    return TRUE;
  }
}

static const char *
get_poppler_datadir (void)
{
  static char retval[MAX_PATH];
  static int beenhere = 0;

  unsigned char *p;

  if (beenhere)
    return retval;

  if (!GetModuleFileNameA (hmodule, (CHAR *) retval, sizeof(retval) - 20))
    return POPPLER_DATADIR;

  p = _mbsrchr ((unsigned char *) retval, '\\');
  *p = '\0';
  p = _mbsrchr ((unsigned char *) retval, '\\');
  if (p) {
    if (stricmp ((const char *) (p+1), "bin") == 0)
      *p = '\0';
  }
  strcat (retval, "\\share\\poppler");

  beenhere = 1;

  return retval;
}

#undef POPPLER_DATADIR
#define POPPLER_DATADIR get_poppler_datadir ()

#endif

//------------------------------------------------------------------------
// SysFontInfo
//------------------------------------------------------------------------

class SysFontInfo {
public:

  GooString *name;
  GBool bold;
  GBool italic;
  GBool oblique;
  GBool fixedWidth;
  GooString *path;
  SysFontType type;
  int fontNum;			// for TrueType collections
  GooString *substituteName;

  SysFontInfo(GooString *nameA, GBool boldA, GBool italicA, GBool obliqueA, GBool fixedWidthA,
	      GooString *pathA, SysFontType typeA, int fontNumA, GooString *substituteNameA);
  ~SysFontInfo();
  SysFontInfo(const SysFontInfo &) = delete;
  SysFontInfo& operator=(const SysFontInfo&) = delete;
  GBool match(SysFontInfo *fi);
  GBool match(GooString *nameA, GBool boldA, GBool italicA, GBool obliqueA, GBool fixedWidthA);
  GBool match(GooString *nameA, GBool boldA, GBool italicA);
};

SysFontInfo::SysFontInfo(GooString *nameA, GBool boldA, GBool italicA, GBool obliqueA, GBool fixedWidthA,
			 GooString *pathA, SysFontType typeA, int fontNumA, GooString *substituteNameA) {
  name = nameA;
  bold = boldA;
  italic = italicA;
  oblique = obliqueA;
  fixedWidth = fixedWidthA;
  path = pathA;
  type = typeA;
  fontNum = fontNumA;
  substituteName = substituteNameA;
}

SysFontInfo::~SysFontInfo() {
  delete name;
  delete path;
  delete substituteName;
}

GBool SysFontInfo::match(SysFontInfo *fi) {
  return !strcasecmp(name->getCString(), fi->name->getCString()) &&
         bold == fi->bold && italic == fi->italic && oblique == fi->oblique && fixedWidth == fi->fixedWidth;
}

GBool SysFontInfo::match(GooString *nameA, GBool boldA, GBool italicA, GBool obliqueA, GBool fixedWidthA) {
  return !strcasecmp(name->getCString(), nameA->getCString()) &&
         bold == boldA && italic == italicA && oblique == obliqueA && fixedWidth == fixedWidthA;
}

GBool SysFontInfo::match(GooString *nameA, GBool boldA, GBool italicA) {
  return !strcasecmp(name->getCString(), nameA->getCString()) &&
         bold == boldA && italic == italicA;
}

//------------------------------------------------------------------------
// SysFontList
//------------------------------------------------------------------------

class SysFontList {
public:

  SysFontList();
  ~SysFontList();
  SysFontList(const SysFontList &) = delete;
  SysFontList& operator=(const SysFontList &) = delete;
  SysFontInfo *find(const GooString *name, GBool isFixedWidth, GBool exact);

#ifdef _WIN32
  void scanWindowsFonts(GooString *winFontDir);
#endif
#ifdef WITH_FONTCONFIGURATION_FONTCONFIG
  void addFcFont(SysFontInfo *si) {fonts->push_back(si);}
#endif
private:

#ifdef _WIN32
  SysFontInfo *makeWindowsFont(const char *name, int fontNum,
			       const char *path);
#endif

  GooList *fonts;			// [SysFontInfo]
};

SysFontList::SysFontList() {
  fonts = new GooList();
}

SysFontList::~SysFontList() {
  deleteGooList<SysFontInfo>(fonts);
}

SysFontInfo *SysFontList::find(const GooString *name, GBool fixedWidth, GBool exact) {
  GooString *name2;
  GBool bold, italic, oblique;
  SysFontInfo *fi;
  char c;
  int n, i;

  name2 = name->copy();

  // remove space, comma, dash chars
  i = 0;
  while (i < name2->getLength()) {
    c = name2->getChar(i);
    if (c == ' ' || c == ',' || c == '-') {
      name2->del(i);
    } else {
      ++i;
    }
  }
  n = name2->getLength();

  // remove trailing "MT" (Foo-MT, Foo-BoldMT, etc.)
  if (n > 2 && !strcmp(name2->getCString() + n - 2, "MT")) {
    name2->del(n - 2, 2);
    n -= 2;
  }

  // look for "Regular"
  if (n > 7 && !strcmp(name2->getCString() + n - 7, "Regular")) {
    name2->del(n - 7, 7);
    n -= 7;
  }

  // look for "Italic"
  if (n > 6 && !strcmp(name2->getCString() + n - 6, "Italic")) {
    name2->del(n - 6, 6);
    italic = gTrue;
    n -= 6;
  } else {
    italic = gFalse;
  }

  // look for "Oblique"
  if (n > 6 && !strcmp(name2->getCString() + n - 7, "Oblique")) {
    name2->del(n - 7, 7);
    oblique = gTrue;
    n -= 6;
  } else {
    oblique = gFalse;
  }

  // look for "Bold"
  if (n > 4 && !strcmp(name2->getCString() + n - 4, "Bold")) {
    name2->del(n - 4, 4);
    bold = gTrue;
    n -= 4;
  } else {
    bold = gFalse;
  }

  // remove trailing "MT" (FooMT-Bold, etc.)
  if (n > 2 && !strcmp(name2->getCString() + n - 2, "MT")) {
    name2->del(n - 2, 2);
    n -= 2;
  }

  // remove trailing "PS"
  if (n > 2 && !strcmp(name2->getCString() + n - 2, "PS")) {
    name2->del(n - 2, 2);
    n -= 2;
  }

  // remove trailing "IdentityH"
  if (n > 9 && !strcmp(name2->getCString() + n - 9, "IdentityH")) {
    name2->del(n - 9, 9);
    n -= 9;
  }

  // search for the font
  fi = nullptr;
  for (i = 0; i < fonts->getLength(); ++i) {
    fi = (SysFontInfo *)fonts->get(i);
    if (fi->match(name2, bold, italic, oblique, fixedWidth)) {
      break;
    }
    fi = nullptr;
  }
  if (!fi && !exact && bold) {
    // try ignoring the bold flag
    for (i = 0; i < fonts->getLength(); ++i) {
      fi = (SysFontInfo *)fonts->get(i);
      if (fi->match(name2, gFalse, italic)) {
	break;
      }
      fi = nullptr;
    }
  }
  if (!fi && !exact && (bold || italic)) {
    // try ignoring the bold and italic flags
    for (i = 0; i < fonts->getLength(); ++i) {
      fi = (SysFontInfo *)fonts->get(i);
      if (fi->match(name2, gFalse, gFalse)) {
	break;
      }
      fi = nullptr;
    }
  }

  delete name2;
  return fi;
}


#define globalParamsLocker()	std::unique_lock<std::recursive_mutex> locker(mutex)
#define unicodeMapCacheLocker()	std::unique_lock<std::recursive_mutex> locker(unicodeMapCacheMutex)
#define cMapCacheLocker()	std::unique_lock<std::recursive_mutex> locker(cMapCacheMutex)

//------------------------------------------------------------------------
// parsing
//------------------------------------------------------------------------

GlobalParams::GlobalParams(const char *customPopplerDataDir)
  : popplerDataDir(customPopplerDataDir)
{
  initBuiltinFontTables();

  // scan the encoding in reverse because we want the lowest-numbered
  // index for each char name ('space' is encoded twice)
  macRomanReverseMap = new NameToCharCode();
  for (int i = 255; i >= 0; --i) {
    if (macRomanEncoding[i]) {
      macRomanReverseMap->add(macRomanEncoding[i], (CharCode)i);
    }
  }

  nameToUnicodeZapfDingbats = new NameToCharCode();
  nameToUnicodeText = new NameToCharCode();
  toUnicodeDirs = new GooList();
  sysFonts = new SysFontList();
  psExpandSmaller = gFalse;
  psShrinkLarger = gTrue;
  psLevel = psLevel2;
  textEncoding = new GooString("UTF-8");
#if defined(_WIN32)
  textEOL = eolDOS;
#elif defined(MACOS)
  textEOL = eolMac;
#else
  textEOL = eolUnix;
#endif
  textPageBreaks = gTrue;
  enableFreeType = gTrue;
  overprintPreview = gFalse;
  printCommands = gFalse;
  profileCommands = gFalse;
  errQuiet = gFalse;

  cidToUnicodeCache = new CharCodeToUnicodeCache(cidToUnicodeCacheSize);
  unicodeToUnicodeCache =
      new CharCodeToUnicodeCache(unicodeToUnicodeCacheSize);
  unicodeMapCache = new UnicodeMapCache();
  cMapCache = new CMapCache();

  baseFontsInitialized = gFalse;

  // set up the initial nameToUnicode tables
  for (int i = 0; nameToUnicodeZapfDingbatsTab[i].name; ++i) {
    nameToUnicodeZapfDingbats->add(nameToUnicodeZapfDingbatsTab[i].name, nameToUnicodeZapfDingbatsTab[i].u);
  }

  for (int i = 0; nameToUnicodeTextTab[i].name; ++i) {
    nameToUnicodeText->add(nameToUnicodeTextTab[i].name, nameToUnicodeTextTab[i].u);
  }

  // set up the residentUnicodeMaps table
  residentUnicodeMaps.reserve(6);
  UnicodeMap map = {"Latin1", gFalse, latin1UnicodeMapRanges, latin1UnicodeMapLen};
  residentUnicodeMaps.emplace(map.getEncodingName()->toStr(), std::move(map));
  map = {"ASCII7", gFalse, ascii7UnicodeMapRanges, ascii7UnicodeMapLen};
  residentUnicodeMaps.emplace(map.getEncodingName()->toStr(), std::move(map));
  map = {"Symbol", gFalse, symbolUnicodeMapRanges, symbolUnicodeMapLen};
  residentUnicodeMaps.emplace(map.getEncodingName()->toStr(), std::move(map));
  map = {"ZapfDingbats", gFalse, zapfDingbatsUnicodeMapRanges, zapfDingbatsUnicodeMapLen};
  residentUnicodeMaps.emplace(map.getEncodingName()->toStr(), std::move(map));
  map = {"UTF-8", gTrue, &mapUTF8};
  residentUnicodeMaps.emplace(map.getEncodingName()->toStr(), std::move(map));
  map = {"UTF-16", gTrue, &mapUTF16};
  residentUnicodeMaps.emplace(map.getEncodingName()->toStr(), std::move(map));

  scanEncodingDirs();
}

void GlobalParams::scanEncodingDirs() {
  GDir *dir;
  GDirEntry *entry;
  const char *dataRoot = popplerDataDir ? popplerDataDir : POPPLER_DATADIR;
  
  // allocate buffer large enough to append "/nameToUnicode"
  size_t bufSize = strlen(dataRoot) + strlen("/nameToUnicode") + 1;
  char *dataPathBuffer = new char[bufSize];
  
  snprintf(dataPathBuffer, bufSize, "%s/nameToUnicode", dataRoot);
  dir = new GDir(dataPathBuffer, gTrue);
  while (entry = dir->getNextEntry(), entry != nullptr) {
    if (!entry->isDir()) {
      parseNameToUnicode(entry->getFullPath());
    }
    delete entry;
  }
  delete dir;

  snprintf(dataPathBuffer, bufSize, "%s/cidToUnicode", dataRoot);
  dir = new GDir(dataPathBuffer, gFalse);
  while (entry = dir->getNextEntry(), entry != nullptr) {
    addCIDToUnicode(entry->getName(), entry->getFullPath());
    delete entry;
  }
  delete dir;

  snprintf(dataPathBuffer, bufSize, "%s/unicodeMap", dataRoot);
  dir = new GDir(dataPathBuffer, gFalse);
  while (entry = dir->getNextEntry(), entry != nullptr) {
    addUnicodeMap(entry->getName(), entry->getFullPath());
    delete entry;
  }
  delete dir;

  snprintf(dataPathBuffer, bufSize, "%s/cMap", dataRoot);
  dir = new GDir(dataPathBuffer, gFalse);
  while (entry = dir->getNextEntry(), entry != nullptr) {
    addCMapDir(entry->getName(), entry->getFullPath());
    toUnicodeDirs->push_back(entry->getFullPath()->copy());
    delete entry;
  }
  delete dir;
  
  delete[] dataPathBuffer;
}

void GlobalParams::parseNameToUnicode(const GooString *name) {
  char *tok1, *tok2;
  FILE *f;
  char buf[256];
  int line;
  Unicode u;
  char *tokptr;

  if (!(f = openFile(name->getCString(), "r"))) {
    error(errIO, -1, "Couldn't open 'nameToUnicode' file '{0:t}'",
	  name);
    return;
  }
  line = 1;
  while (getLine(buf, sizeof(buf), f)) {
    tok1 = strtok_r(buf, " \t\r\n", &tokptr);
    tok2 = strtok_r(nullptr, " \t\r\n", &tokptr);
    if (tok1 && tok2) {
      sscanf(tok1, "%x", &u);
      nameToUnicodeText->add(tok2, u);
    } else {
      error(errConfig, -1, "Bad line in 'nameToUnicode' file ({0:t}:{1:d})",
	    name, line);
    }
    ++line;
  }
  fclose(f);
}

void GlobalParams::addCIDToUnicode(const GooString *collection, const GooString *fileName) {
  cidToUnicodes[collection->toStr()] = fileName->toStr();
}

void GlobalParams::addUnicodeMap(const GooString *encodingName, const GooString *fileName) {
  unicodeMaps[encodingName->toStr()] = fileName->toStr();
}

void GlobalParams::addCMapDir(const GooString *collection, const GooString *dir) {
  cMapDirs.emplace(collection->toStr(), dir->toStr());
}

GBool GlobalParams::parseYesNo2(const char *token, GBool *flag) {
  if (!strcmp(token, "yes")) {
    *flag = gTrue;
  } else if (!strcmp(token, "no")) {
    *flag = gFalse;
  } else {
    return gFalse;
  }
  return gTrue;
}

GlobalParams::~GlobalParams() {
  freeBuiltinFontTables();

  delete macRomanReverseMap;

  delete nameToUnicodeZapfDingbats;
  delete nameToUnicodeText;
  deleteGooList<GooString>(toUnicodeDirs);
  delete sysFonts;
  delete textEncoding;

  delete cidToUnicodeCache;
  delete unicodeToUnicodeCache;
  delete unicodeMapCache;
  delete cMapCache;
}

//------------------------------------------------------------------------
// accessors
//------------------------------------------------------------------------

CharCode GlobalParams::getMacRomanCharCode(char *charName) {
  // no need to lock - macRomanReverseMap is constant
  return macRomanReverseMap->lookup(charName);
}

Unicode GlobalParams::mapNameToUnicodeAll(const char *charName) {
  // no need to lock - nameToUnicodeZapfDingbats and nameToUnicodeText are constant
  Unicode u = nameToUnicodeZapfDingbats->lookup(charName);
  if (!u)
    u = nameToUnicodeText->lookup(charName);
  return u;
}

Unicode GlobalParams::mapNameToUnicodeText(const char *charName) {
  // no need to lock - nameToUnicodeText is constant
  return nameToUnicodeText->lookup(charName);
}

UnicodeMap *GlobalParams::getResidentUnicodeMap(const GooString *encodingName) {
  UnicodeMap *map = nullptr;

  globalParamsLocker();
  const auto unicodeMap = residentUnicodeMaps.find(encodingName->toStr());
  if (unicodeMap != residentUnicodeMaps.end()) {
    map = &unicodeMap->second;
    map->incRefCnt();
  }

  return map;
}

FILE *GlobalParams::getUnicodeMapFile(const GooString *encodingName) {
  FILE *file = nullptr;

  globalParamsLocker();
  const auto unicodeMap = unicodeMaps.find(encodingName->toStr());
  if (unicodeMap != unicodeMaps.end()) {
    file = openFile(unicodeMap->second.c_str(), "r");
  }

  return file;
}

FILE *GlobalParams::findCMapFile(const GooString *collection, const GooString *cMapName) {
  FILE *file = nullptr;

  globalParamsLocker();
  const auto cMapDirs = this->cMapDirs.equal_range(collection->toStr());
  for (auto cMapDir = cMapDirs.first; cMapDir != cMapDirs.second; ++cMapDir) {
    auto* const path = new GooString(cMapDir->second);
    appendToPath(path, cMapName->getCString());
    file = openFile(path->getCString(), "r");
    delete path;
    if (file) {
      break;
    }
  }

  return file;
}

FILE *GlobalParams::findToUnicodeFile(const GooString *name) {
  GooString *dir, *fileName;
  FILE *f;
  int i;

  globalParamsLocker();
  for (i = 0; i < toUnicodeDirs->getLength(); ++i) {
    dir = (GooString *)toUnicodeDirs->get(i);
    fileName = appendToPath(dir->copy(), name->getCString());
    f = openFile(fileName->getCString(), "r");
    delete fileName;
    if (f) {
      return f;
    }
  }
  return nullptr;
}

#ifdef WITH_FONTCONFIGURATION_FONTCONFIG
static GBool findModifier(const char *name, const char *modifier, const char **start)
{
  const char *match;

  if (name == nullptr)
    return gFalse;

  match = strstr(name, modifier);
  if (match) {
    if (*start == nullptr || match < *start)
      *start = match;
    return gTrue;
  }
  else {
    return gFalse;
  }
}

static const char *getFontLang(GfxFont *font)
{
  const char *lang;

  // find the language we want the font to support
  if (font->isCIDFont())
  {
    const GooString *collection = ((GfxCIDFont *)font)->getCollection();
    if (collection)
    {
      if (strcmp(collection->getCString(), "Adobe-GB1") == 0)
        lang = "zh-cn"; // Simplified Chinese
      else if (strcmp(collection->getCString(), "Adobe-CNS1") == 0)
        lang = "zh-tw"; // Traditional Chinese
      else if (strcmp(collection->getCString(), "Adobe-Japan1") == 0)
        lang = "ja"; // Japanese
      else if (strcmp(collection->getCString(), "Adobe-Japan2") == 0)
        lang = "ja"; // Japanese
      else if (strcmp(collection->getCString(), "Adobe-Korea1") == 0)
        lang = "ko"; // Korean
      else if (strcmp(collection->getCString(), "Adobe-UCS") == 0)
        lang = "xx";
      else if (strcmp(collection->getCString(), "Adobe-Identity") == 0)
        lang = "xx";
      else
      {
        error(errUnimplemented, -1, "Unknown CID font collection, please report to poppler bugzilla.");
        lang = "xx";
      }
    }
    else lang = "xx";
  }
  else lang = "xx";
  return lang;
}

static FcPattern *buildFcPattern(GfxFont *font, const GooString *base14Name)
{
  int weight = -1,
      slant = -1,
      width = -1,
      spacing = -1;
  const char *family;
  const char *start;
  FcPattern *p;

  // this is all heuristics will be overwritten if font had proper info
  char *fontName = strdup(((base14Name == nullptr) ? font->getName() : base14Name)->getCString());

  const char *modifiers = strchr (fontName, ',');
  if (modifiers == nullptr)
    modifiers = strchr (fontName, '-');

  // remove the - from the names, for some reason, Fontconfig does not
  // understand "MS-Mincho" but does with "MS Mincho"
  const int len = strlen(fontName);
  for (int i = 0; i < len; i++)
    fontName[i] = (fontName[i] == '-' ? ' ' : fontName[i]);

  start = nullptr;
  findModifier(modifiers, "Regular", &start);
  findModifier(modifiers, "Roman", &start);
  
  if (findModifier(modifiers, "Oblique", &start))
    slant = FC_SLANT_OBLIQUE;
  if (findModifier(modifiers, "Italic", &start))
    slant = FC_SLANT_ITALIC;
  if (findModifier(modifiers, "Bold", &start))
    weight = FC_WEIGHT_BOLD;
  if (findModifier(modifiers, "Light", &start))
    weight = FC_WEIGHT_LIGHT;
  if (findModifier(modifiers, "Medium", &start))
    weight = FC_WEIGHT_MEDIUM;
  if (findModifier(modifiers, "Condensed", &start))
    width = FC_WIDTH_CONDENSED;
  
  if (start) {
    // There have been "modifiers" in the name, crop them to obtain
    // the family name
    family = strndup(fontName, modifiers - fontName);
    free(fontName);
    fontName = nullptr;
  }
  else {
    family = fontName;
    fontName = nullptr;
  }
  
  // use font flags
  if (font->isFixedWidth())
    spacing = FC_MONO;
  if (font->isBold())
    weight = FC_WEIGHT_BOLD;
  if (font->isItalic())
    slant = FC_SLANT_ITALIC;
  
  bool freeFamily = true;
  // if the FontDescriptor specified a family name use it
  if (font->getFamily()) {
    free((char*)family);
    family = font->getFamily()->getCString();
    freeFamily = false;
  }
  
  // if the FontDescriptor specified a weight use it
  switch (font -> getWeight())
  {
    case GfxFont::W100: weight = FC_WEIGHT_EXTRALIGHT; break; 
    case GfxFont::W200: weight = FC_WEIGHT_LIGHT; break; 
    case GfxFont::W300: weight = FC_WEIGHT_BOOK; break; 
    case GfxFont::W400: weight = FC_WEIGHT_NORMAL; break; 
    case GfxFont::W500: weight = FC_WEIGHT_MEDIUM; break; 
    case GfxFont::W600: weight = FC_WEIGHT_DEMIBOLD; break; 
    case GfxFont::W700: weight = FC_WEIGHT_BOLD; break; 
    case GfxFont::W800: weight = FC_WEIGHT_EXTRABOLD; break; 
    case GfxFont::W900: weight = FC_WEIGHT_BLACK; break; 
    default: break; 
  }
  
  // if the FontDescriptor specified a width use it
  switch (font -> getStretch())
  {
    case GfxFont::UltraCondensed: width = FC_WIDTH_ULTRACONDENSED; break; 
    case GfxFont::ExtraCondensed: width = FC_WIDTH_EXTRACONDENSED; break; 
    case GfxFont::Condensed: width = FC_WIDTH_CONDENSED; break; 
    case GfxFont::SemiCondensed: width = FC_WIDTH_SEMICONDENSED; break; 
    case GfxFont::Normal: width = FC_WIDTH_NORMAL; break; 
    case GfxFont::SemiExpanded: width = FC_WIDTH_SEMIEXPANDED; break; 
    case GfxFont::Expanded: width = FC_WIDTH_EXPANDED; break; 
    case GfxFont::ExtraExpanded: width = FC_WIDTH_EXTRAEXPANDED; break; 
    case GfxFont::UltraExpanded: width = FC_WIDTH_ULTRAEXPANDED; break; 
    default: break; 
  }
  
  const char *lang = getFontLang(font);
  
  p = FcPatternBuild(nullptr,
                    FC_FAMILY, FcTypeString, family,
                    FC_LANG, FcTypeString, lang,
                    NULL);
  if (slant != -1) FcPatternAddInteger(p, FC_SLANT, slant);
  if (weight != -1) FcPatternAddInteger(p, FC_WEIGHT, weight);
  if (width != -1) FcPatternAddInteger(p, FC_WIDTH, width);
  if (spacing != -1) FcPatternAddInteger(p, FC_SPACING, spacing);

  if (freeFamily)
    free((char*)family);
  return p;
}
#endif

GooString *GlobalParams::findFontFile(const GooString *fontName) {
  GooString *path = nullptr;

  setupBaseFonts(nullptr);
  globalParamsLocker();
  const auto fontFile = fontFiles.find(fontName->toStr());
  if (fontFile != fontFiles.end()) {
    path = new GooString(fontFile->second);
  }

  return path;
}

/* if you can't or don't want to use Fontconfig, you need to implement
   this function for your platform. For Windows, it's in GlobalParamsWin.cc
*/
#ifdef WITH_FONTCONFIGURATION_FONTCONFIG
// not needed for fontconfig
void GlobalParams::setupBaseFonts(char *) {
}

GooString *GlobalParams::findBase14FontFile(const GooString *base14Name, GfxFont *font) {
  SysFontType type;
  int fontNum;
  
  return findSystemFontFile(font, &type, &fontNum, nullptr, base14Name);
}

GooString *GlobalParams::findSystemFontFile(GfxFont *font,
					  SysFontType *type,
					  int *fontNum, GooString *substituteFontName, const GooString *base14Name) {
  SysFontInfo *fi = nullptr;
  FcPattern *p=nullptr;
  GooString *path = nullptr;
  const GooString *fontName = font->getName();
  GooString substituteName;
  if (!fontName) return nullptr;

  globalParamsLocker();

  if ((fi = sysFonts->find(fontName, font->isFixedWidth(), gTrue))) {
    path = fi->path->copy();
    *type = fi->type;
    *fontNum = fi->fontNum;
    substituteName.Set(fi->substituteName->getCString());
  } else {
    FcChar8* s;
    char * ext;
    FcResult res;
    FcFontSet *set;
    int i;
    FcLangSet *lb = nullptr;
    p = buildFcPattern(font, base14Name);

    if (!p)
      goto fin;
    FcConfigSubstitute(nullptr, p, FcMatchPattern);
    FcDefaultSubstitute(p);
    set = FcFontSort(nullptr, p, FcFalse, nullptr, &res);
    if (!set)
      goto fin;

    // find the language we want the font to support
    const char *lang = getFontLang(font);
    if (strcmp(lang,"xx") != 0) {
      lb = FcLangSetCreate();
      FcLangSetAdd(lb,(FcChar8 *)lang);
    }

    /*
      scan twice.
      first: fonts support the language
      second: all fonts (fall back)
    */
    while (fi == nullptr)
    {
      for (i = 0; i < set->nfont; ++i)
      {
	res = FcPatternGetString(set->fonts[i], FC_FILE, 0, &s);
	if (res != FcResultMatch || !s)
	  continue;
	if (lb != nullptr) {
	  FcLangSet *l;
	  res = FcPatternGetLangSet(set->fonts[i], FC_LANG, 0, &l);
	  if (res != FcResultMatch || !FcLangSetContains(l,lb)) {
	    continue;
	  }
	}
	FcChar8* s2;
        res = FcPatternGetString(set->fonts[i], FC_FULLNAME, 0, &s2);
        if (res == FcResultMatch && s2) {
          substituteName.Set((char*)s2);
        } else {
          // fontconfig does not extract fullname for some fonts
          // create the fullname from family and style
          res = FcPatternGetString(set->fonts[i], FC_FAMILY, 0, &s2);
          if (res == FcResultMatch && s2) {
            substituteName.Set((char*)s2);
            res = FcPatternGetString(set->fonts[i], FC_STYLE, 0, &s2);
            if (res == FcResultMatch && s2) {
              GooString *style = new GooString((char*)s2);
              if (style->cmp("Regular") != 0) {
                substituteName.append(" ");
                substituteName.append(style);
              }
              delete style;
            }
          }
        }
	ext = strrchr((char*)s,'.');
	if (!ext)
	  continue;
	if (!strncasecmp(ext,".ttf",4) || !strncasecmp(ext, ".ttc", 4) || !strncasecmp(ext, ".otf", 4))
	{
	  int weight, slant;
	  GBool bold = font->isBold();
	  GBool italic = font->isItalic();
	  GBool oblique = gFalse;
	  FcPatternGetInteger(set->fonts[i], FC_WEIGHT, 0, &weight);
	  FcPatternGetInteger(set->fonts[i], FC_SLANT, 0, &slant);
	  if (weight == FC_WEIGHT_DEMIBOLD || weight == FC_WEIGHT_BOLD 
	      || weight == FC_WEIGHT_EXTRABOLD || weight == FC_WEIGHT_BLACK)
	  {
	    bold = gTrue;
	  }
	  if (slant == FC_SLANT_ITALIC)
	    italic = gTrue;
	  if (slant == FC_SLANT_OBLIQUE)
	    oblique = gTrue;
	  *fontNum = 0;
	  *type = (!strncasecmp(ext,".ttc",4)) ? sysFontTTC : sysFontTTF;
	  FcPatternGetInteger(set->fonts[i], FC_INDEX, 0, fontNum);
	  fi = new SysFontInfo(fontName->copy(), bold, italic, oblique, font->isFixedWidth(),
			       new GooString((char*)s), *type, *fontNum, substituteName.copy());
	  sysFonts->addFcFont(fi);
	  path = new GooString((char*)s);
	}
	else if (!strncasecmp(ext,".pfa",4) || !strncasecmp(ext,".pfb",4)) 
	{
	  int weight, slant;
	  GBool bold = font->isBold();
	  GBool italic = font->isItalic();
	  GBool oblique = gFalse;
	  FcPatternGetInteger(set->fonts[i], FC_WEIGHT, 0, &weight);
	  FcPatternGetInteger(set->fonts[i], FC_SLANT, 0, &slant);
	  if (weight == FC_WEIGHT_DEMIBOLD || weight == FC_WEIGHT_BOLD 
	      || weight == FC_WEIGHT_EXTRABOLD || weight == FC_WEIGHT_BLACK)
	  {
		bold = gTrue;
	  }
	  if (slant == FC_SLANT_ITALIC)
	    italic = gTrue;
	  if (slant == FC_SLANT_OBLIQUE)
	    oblique = gTrue;
	  *fontNum = 0;
	  *type = (!strncasecmp(ext,".pfa",4)) ? sysFontPFA : sysFontPFB;
	  FcPatternGetInteger(set->fonts[i], FC_INDEX, 0, fontNum);
	  fi = new SysFontInfo(fontName->copy(), bold, italic, oblique, font->isFixedWidth(),
			       new GooString((char*)s), *type, *fontNum, substituteName.copy());
	  sysFonts->addFcFont(fi);
	  path = new GooString((char*)s);
	}
	else
	  continue;
	break;
      }
      if (lb != nullptr) {
        FcLangSetDestroy(lb);
        lb = nullptr;
      } else {
        /* scan all fonts of the list */
        break;
      }
    }
    FcFontSetDestroy(set);
  }
  if (path == nullptr && (fi = sysFonts->find(fontName, font->isFixedWidth(), gFalse))) {
    path = fi->path->copy();
    *type = fi->type;
    *fontNum = fi->fontNum;
  }
  if (substituteFontName) {
    substituteFontName->Set(substituteName.getCString());
  }
fin:
  if (p)
    FcPatternDestroy(p);

  return path;
}

#elif WITH_FONTCONFIGURATION_WIN32
#include "GlobalParamsWin.cc"

GooString *GlobalParams::findBase14FontFile(const GooString *base14Name, GfxFont *font) {
  return findFontFile(base14Name);
}
#else
GooString *GlobalParams::findBase14FontFile(const GooString *base14Name, GfxFont *font) {
  return findFontFile(base14Name);
}

static struct {
  const char *name;
  const char *t1FileName;
  const char *ttFileName;
} displayFontTab[] = {
  {"Courier",               "n022003l.pfb", "cour.ttf"},
  {"Courier-Bold",          "n022004l.pfb", "courbd.ttf"},
  {"Courier-BoldOblique",   "n022024l.pfb", "courbi.ttf"},
  {"Courier-Oblique",       "n022023l.pfb", "couri.ttf"},
  {"Helvetica",             "n019003l.pfb", "arial.ttf"},
  {"Helvetica-Bold",        "n019004l.pfb", "arialbd.ttf"},
  {"Helvetica-BoldOblique", "n019024l.pfb", "arialbi.ttf"},
  {"Helvetica-Oblique",     "n019023l.pfb", "ariali.ttf"},
  {"Symbol",                "s050000l.pfb", nullptr},
  {"Times-Bold",            "n021004l.pfb", "timesbd.ttf"},
  {"Times-BoldItalic",      "n021024l.pfb", "timesbi.ttf"},
  {"Times-Italic",          "n021023l.pfb", "timesi.ttf"},
  {"Times-Roman",           "n021003l.pfb", "times.ttf"},
  {"ZapfDingbats",          "d050000l.pfb", nullptr},
  {nullptr, nullptr, nullptr}
};

static const char *displayFontDirs[] = {
  "/usr/share/ghostscript/fonts",
  "/usr/local/share/ghostscript/fonts",
  "/usr/share/fonts/default/Type1",
  "/usr/share/fonts/default/ghostscript",
  "/usr/share/fonts/type1/gsfonts",
  nullptr
};

void GlobalParams::setupBaseFonts(char *dir) {
  GooString *fontName;
  GooString *fileName;
  FILE *f;
  int i, j;

  for (i = 0; displayFontTab[i].name; ++i) {
    if (fontFiles.count(displayFontTab[i].name) > 0) {
      continue;
    }
    fontName = new GooString(displayFontTab[i].name);
    fileName = nullptr;
    if (dir) {
      fileName = appendToPath(new GooString(dir), displayFontTab[i].t1FileName);
      if ((f = fopen(fileName->getCString(), "rb"))) {
	      fclose(f);
      } else {
	      delete fileName;
	      fileName = nullptr;
      }
    }
    for (j = 0; !fileName && displayFontDirs[j]; ++j) {
      fileName = appendToPath(new GooString(displayFontDirs[j]),
			      displayFontTab[i].t1FileName);
      if ((f = fopen(fileName->getCString(), "rb"))) {
	      fclose(f);
      } else {
	      delete fileName;
	      fileName = nullptr;
      }
    }
    if (!fileName) {
      error(errConfig, -1, "No display font for '{0:s}'",
	    displayFontTab[i].name);
      delete fontName;
      continue;
    }
    addFontFile(fontName, fileName);
  }

}

GooString *GlobalParams::findSystemFontFile(GfxFont *font,
					  SysFontType *type,
					  int *fontNum, GooString * /*substituteFontName*/,
					  const GooString * /*base14Name*/) {
  SysFontInfo *fi;
  GooString *path;

  const GooString *fontName = font->getName();
  if (!fontName) return nullptr;

  path = nullptr;
  globalParamsLocker();
  if ((fi = sysFonts->find(fontName, font->isFixedWidth(), gFalse))) {
    path = fi->path->copy();
    *type = fi->type;
    *fontNum = fi->fontNum;
  }

  return path;
}
#endif

GBool GlobalParams::getPSExpandSmaller() {
  globalParamsLocker();
  return psExpandSmaller;
}

GBool GlobalParams::getPSShrinkLarger() {
  globalParamsLocker();
  return psShrinkLarger;
}

PSLevel GlobalParams::getPSLevel() {
  globalParamsLocker();
  return psLevel;
}

GooString *GlobalParams::getTextEncodingName() {
  globalParamsLocker();
  return textEncoding->copy();
}

EndOfLineKind GlobalParams::getTextEOL() {
  globalParamsLocker();
  return textEOL;
}

GBool GlobalParams::getTextPageBreaks() {
  globalParamsLocker();
  return textPageBreaks;
}

GBool GlobalParams::getEnableFreeType() {
  globalParamsLocker();
  return enableFreeType;
}

GBool GlobalParams::getPrintCommands() {
  globalParamsLocker();
  return printCommands;
}

GBool GlobalParams::getProfileCommands() {
  globalParamsLocker();
  return profileCommands;
}

GBool GlobalParams::getErrQuiet() {
  // no locking -- this function may get called from inside a locked
  // section
  return errQuiet;
}

CharCodeToUnicode *GlobalParams::getCIDToUnicode(GooString *collection) {
  CharCodeToUnicode *ctu;

  globalParamsLocker();
  if (!(ctu = cidToUnicodeCache->getCharCodeToUnicode(collection))) {
    const auto cidToUnicode = cidToUnicodes.find(collection->toStr());
    if (cidToUnicode != cidToUnicodes.end()) {
      if((ctu = CharCodeToUnicode::parseCIDToUnicode(cidToUnicode->second.c_str(), collection))) {
        cidToUnicodeCache->add(ctu);
      }
    }
  }

  return ctu;
}

UnicodeMap *GlobalParams::getUnicodeMap(GooString *encodingName) {
  return getUnicodeMap2(encodingName);
}

UnicodeMap *GlobalParams::getUnicodeMap2(GooString *encodingName) {
  UnicodeMap *map;

  if (!(map = getResidentUnicodeMap(encodingName))) {
    unicodeMapCacheLocker();
    map = unicodeMapCache->getUnicodeMap(encodingName);
  }

  return map;
}

CMap *GlobalParams::getCMap(const GooString *collection, GooString *cMapName, Stream *stream) {
  cMapCacheLocker();
  return cMapCache->getCMap(collection, cMapName, stream);
}

UnicodeMap *GlobalParams::getTextEncoding() {
  return getUnicodeMap2(textEncoding);
}

GooList *GlobalParams::getEncodingNames()
{
  auto* const result = new GooList;
  for (const auto& unicodeMap : residentUnicodeMaps) {
    result->push_back(new GooString(unicodeMap.first));
  }
  for (const auto& unicodeMap : unicodeMaps) {
    result->push_back(new GooString(unicodeMap.first));
  }
  return result;
}

//------------------------------------------------------------------------
// functions to set parameters
//------------------------------------------------------------------------

void GlobalParams::addFontFile(GooString *fontName, GooString *path) {
  globalParamsLocker();
  fontFiles[fontName->toStr()] = path->toStr();
}

void GlobalParams::setPSExpandSmaller(GBool expand) {
  globalParamsLocker();
  psExpandSmaller = expand;
}

void GlobalParams::setPSShrinkLarger(GBool shrink) {
  globalParamsLocker();
  psShrinkLarger = shrink;
}

void GlobalParams::setPSLevel(PSLevel level) {
  globalParamsLocker();
  psLevel = level;
}

void GlobalParams::setTextEncoding(char *encodingName) {
  globalParamsLocker();
  delete textEncoding;
  textEncoding = new GooString(encodingName);
}

GBool GlobalParams::setTextEOL(char *s) {
  globalParamsLocker();
  if (!strcmp(s, "unix")) {
    textEOL = eolUnix;
  } else if (!strcmp(s, "dos")) {
    textEOL = eolDOS;
  } else if (!strcmp(s, "mac")) {
    textEOL = eolMac;
  } else {
    return gFalse;
  }
  return gTrue;
}

void GlobalParams::setTextPageBreaks(GBool pageBreaks) {
  globalParamsLocker();
  textPageBreaks = pageBreaks;
}

GBool GlobalParams::setEnableFreeType(char *s) {
  globalParamsLocker();
  return parseYesNo2(s, &enableFreeType);
}

void GlobalParams::setOverprintPreview(GBool overprintPreviewA) {
  globalParamsLocker();
  overprintPreview = overprintPreviewA;
}

void GlobalParams::setPrintCommands(GBool printCommandsA) {
  globalParamsLocker();
  printCommands = printCommandsA;
}

void GlobalParams::setProfileCommands(GBool profileCommandsA) {
  globalParamsLocker();
  profileCommands = profileCommandsA;
}

void GlobalParams::setErrQuiet(GBool errQuietA) {
  globalParamsLocker();
  errQuiet = errQuietA;
}
