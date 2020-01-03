//========================================================================
//
// Link.h
//
// Copyright 1996-2003 Glyph & Cog, LLC
//
//========================================================================

//========================================================================
//
// Modified under the Poppler project - http://poppler.freedesktop.org
//
// All changes made under the Poppler project to this file are licensed
// under GPL version 2 or later
//
// Copyright (C) 2006, 2008 Pino Toscano <pino@kde.org>
// Copyright (C) 2008 Hugo Mercier <hmercier31@gmail.com>
// Copyright (C) 2010, 2011 Carlos Garcia Campos <carlosgc@gnome.org>
// Copyright (C) 2012 Tobias Koening <tobias.koenig@kdab.com>
// Copyright (C) 2018, 2019 Albert Astals Cid <aacid@kde.org>
// Copyright (C) 2018 Klarälvdalens Datakonsult AB, a KDAB Group company, <info@kdab.com>. Work sponsored by the LiMux project of the city of Munich
// Copyright (C) 2018 Intevation GmbH <intevation@intevation.de>
// Copyright (C) 2019, 2020 Oliver Sander <oliver.sander@tu-dresden.de>
//
// To see a description of the changes please see the Changelog file that
// came with your tarball or type make ChangeLog if you are building from git
//
//========================================================================

#ifndef LINK_H
#define LINK_H

#include "Object.h"
#include <memory>
#include <set>

class GooString;
class Array;
class Dict;
class Sound;
class MediaRendition;
class AnnotLink;
class Annots;

//------------------------------------------------------------------------
// LinkAction
//------------------------------------------------------------------------

enum LinkActionKind {
  actionGoTo,			// go to destination
  actionGoToR,			// go to destination in new file
  actionLaunch,			// launch app (or open document)
  actionURI,			// URI
  actionNamed,			// named action
  actionMovie,			// movie action
  actionRendition,		// rendition action
  actionSound,			// sound action
  actionJavaScript,		// JavaScript action
  actionOCGState,               // Set-OCG-State action
  actionHide,			// Hide action
  actionUnknown			// anything else
};

class LinkAction {
public:

  LinkAction();
  LinkAction(const LinkAction &) = delete;
  LinkAction& operator=(const LinkAction &other) = delete;

  // Destructor.
  virtual ~LinkAction();

  // Was the LinkAction created successfully?
  virtual bool isOk() const = 0;

  // Check link action type.
  virtual LinkActionKind getKind() const = 0;

  // Parse a destination (old-style action) name, string, or array.
  static LinkAction *parseDest(const Object *obj);

  // Parse an action dictionary.
  static LinkAction *parseAction(const Object *obj, const GooString *baseURI = nullptr);

  // A List of the next actions to execute in order.
  // The list contains pointer to LinkAction objects.
  const std::vector<LinkAction*> *nextActions() const;

  // Sets the next action list.
  void setNextActions(std::vector<LinkAction*>&& actions);

private:
  static LinkAction *parseAction(const Object *obj, const GooString *baseURI, std::set<int> *seenNextActions);

  std::vector<LinkAction*> nextActionList;
};

//------------------------------------------------------------------------
// LinkDest
//------------------------------------------------------------------------

enum LinkDestKind {
  destXYZ,
  destFit,
  destFitH,
  destFitV,
  destFitR,
  destFitB,
  destFitBH,
  destFitBV
};

class LinkDest {
public:

  // Build a LinkDest from the array.
  LinkDest(const Array *a);

  // Copy a LinkDest.
  LinkDest *copy() const { return new LinkDest(this); }

  // Was the LinkDest created successfully?
  bool isOk() const { return ok; }

  // Accessors.
  LinkDestKind getKind() const { return kind; }
  bool isPageRef() const { return pageIsRef; }
  int getPageNum() const { return pageNum; }
  Ref getPageRef() const { return pageRef; }
  double getLeft() const { return left; }
  double getBottom() const { return bottom; }
  double getRight() const { return right; }
  double getTop() const { return top; }
  double getZoom() const { return zoom; }
  bool getChangeLeft() const { return changeLeft; }
  bool getChangeTop() const { return changeTop; }
  bool getChangeZoom() const { return changeZoom; }

private:

  LinkDestKind kind;		// destination type
  bool pageIsRef;		// is the page a reference or number?
  union {
    Ref pageRef;		// reference to page
    int pageNum;		// one-relative page number
  };
  double left, bottom;		// position
  double right, top;
  double zoom;			// zoom factor
  bool changeLeft, changeTop;	// which position components to change:
  bool changeZoom;		//   destXYZ uses all three;
				//   destFitH/BH use changeTop;
				//   destFitV/BV use changeLeft
  bool ok;			// set if created successfully

  LinkDest(const LinkDest *dest);
};

//------------------------------------------------------------------------
// LinkGoTo
//------------------------------------------------------------------------

class LinkGoTo: public LinkAction {
public:

  // Build a LinkGoTo from a destination (dictionary, name, or string).
  LinkGoTo(const Object *destObj);

  // Destructor.
  ~LinkGoTo() override;

  // Was the LinkGoTo created successfully?
  bool isOk() const override { return dest || namedDest; }

  // Accessors.
  LinkActionKind getKind() const override { return actionGoTo; }
  const LinkDest *getDest() const { return dest; }
  const GooString *getNamedDest() const { return namedDest; }

private:

  LinkDest *dest;		// regular destination (nullptr for remote
				//   link with bad destination)
  GooString *namedDest;	// named destination (only one of dest and
				//   and namedDest may be non-nullptr)
};

//------------------------------------------------------------------------
// LinkGoToR
//------------------------------------------------------------------------

class LinkGoToR: public LinkAction {
public:

  // Build a LinkGoToR from a file spec (dictionary) and destination
  // (dictionary, name, or string).
  LinkGoToR(Object *fileSpecObj, Object *destObj);

  // Destructor.
  ~LinkGoToR() override;

  // Was the LinkGoToR created successfully?
  bool isOk() const override { return fileName && (dest || namedDest); }

  // Accessors.
  LinkActionKind getKind() const override { return actionGoToR; }
  const GooString *getFileName() const { return fileName; }
  const LinkDest *getDest() const { return dest; }
  const GooString *getNamedDest() const { return namedDest; }

private:

  GooString *fileName;		// file name
  LinkDest *dest;		// regular destination (nullptr for remote
				//   link with bad destination)
  GooString *namedDest;	// named destination (only one of dest and
				//   and namedDest may be non-nullptr)
};

//------------------------------------------------------------------------
// LinkLaunch
//------------------------------------------------------------------------

class LinkLaunch: public LinkAction {
public:

  // Build a LinkLaunch from an action dictionary.
  LinkLaunch(const Object *actionObj);

  // Destructor.
  ~LinkLaunch() override;

  // Was the LinkLaunch created successfully?
  bool isOk() const override { return fileName != nullptr; }

  // Accessors.
  LinkActionKind getKind() const override { return actionLaunch; }
  const GooString *getFileName() const { return fileName; }
  const GooString *getParams() const { return params; }

private:

  GooString *fileName;		// file name
  GooString *params;		// parameters
};

//------------------------------------------------------------------------
// LinkURI
//------------------------------------------------------------------------

class LinkURI: public LinkAction {
public:

  // Build a LinkURI given the URI (string) and base URI.
  LinkURI(const Object *uriObj, const GooString *baseURI);

  // Destructor.
  ~LinkURI() override;

  // Was the LinkURI created successfully?
  bool isOk() const override { return uri != nullptr; }

  // Accessors.
  LinkActionKind getKind() const override { return actionURI; }
  const GooString *getURI() const { return uri; }

private:

  GooString *uri;			// the URI
};

//------------------------------------------------------------------------
// LinkNamed
//------------------------------------------------------------------------

class LinkNamed: public LinkAction {
public:

  // Build a LinkNamed given the action name.
  LinkNamed(const Object *nameObj);

  ~LinkNamed() override;

  bool isOk() const override { return name != nullptr; }

  LinkActionKind getKind() const override { return actionNamed; }
  const GooString *getName() const { return name; }

private:

  GooString *name;
};


//------------------------------------------------------------------------
// LinkMovie
//------------------------------------------------------------------------

class LinkMovie: public LinkAction {
public:

  enum OperationType {
    operationTypePlay,
    operationTypePause,
    operationTypeResume,
    operationTypeStop
  };

  LinkMovie(const Object *obj);
  ~LinkMovie() override;

  bool isOk() const override { return hasAnnotRef() || hasAnnotTitle(); }
  LinkActionKind getKind() const override { return actionMovie; }

  // a movie action stores either an indirect reference to a movie annotation
  // or the movie annotation title

  bool hasAnnotRef() const { return annotRef != Ref::INVALID(); }
  bool hasAnnotTitle() const { return annotTitle != nullptr; }
  const Ref *getAnnotRef() const { return &annotRef; }
  const GooString *getAnnotTitle() const { return annotTitle; }

  OperationType getOperation() const { return operation; }

private:

  Ref annotRef;            // Annotation
  GooString *annotTitle;   // T

  OperationType operation; // Operation
};


//------------------------------------------------------------------------
// LinkRendition
//------------------------------------------------------------------------

class LinkRendition: public LinkAction {
public:
  /**
   * Describes the possible rendition operations.
   */
  enum RenditionOperation {
    NoRendition,
    PlayRendition,
    StopRendition,
    PauseRendition,
    ResumeRendition
  };

  LinkRendition(const Object *Obj);

  ~LinkRendition() override;

  bool isOk() const override { return true; }

  LinkActionKind getKind() const override { return actionRendition; }

  bool hasRenditionObject() const { return renditionObj.isDict(); }
  const Object* getRenditionObject() const { return &renditionObj; }

  bool hasScreenAnnot() const { return screenRef != Ref::INVALID(); }
  Ref getScreenAnnot() const { return screenRef; }

  RenditionOperation getOperation() const { return operation; }

  const MediaRendition* getMedia() const { return media; }

  const GooString *getScript() const { return js; }

private:

  Ref screenRef;
  Object renditionObj;
  RenditionOperation operation;

  MediaRendition* media;

  GooString *js;
};

//------------------------------------------------------------------------
// LinkSound
//------------------------------------------------------------------------

class LinkSound: public LinkAction {
public:

  LinkSound(const Object *soundObj);

  ~LinkSound() override;

  bool isOk() const override { return sound != nullptr; }

  LinkActionKind getKind() const override { return actionSound; }

  double getVolume() const { return volume; }
  bool getSynchronous() const { return sync; }
  bool getRepeat() const { return repeat; }
  bool getMix() const { return mix; }
  Sound *getSound() const { return sound; }

private:

  double volume;
  bool sync;
  bool repeat;
  bool mix;
  Sound *sound;
};

//------------------------------------------------------------------------
// LinkJavaScript
//------------------------------------------------------------------------

class LinkJavaScript: public LinkAction {
public:

  // Build a LinkJavaScript given the action name.
  LinkJavaScript(Object *jsObj);

  ~LinkJavaScript() override;

  bool isOk() const override { return js != nullptr; }

  LinkActionKind getKind() const override { return actionJavaScript; }
  const GooString *getScript() const { return js; }

  static Object createObject(XRef *xref, const GooString &js);

private:

  GooString *js;
};

//------------------------------------------------------------------------
// LinkOCGState
//------------------------------------------------------------------------
class LinkOCGState: public LinkAction {
public:
  LinkOCGState(const Object *obj);

  ~LinkOCGState() override = default;

  bool isOk() const override { return isValid; }

  LinkActionKind getKind() const override { return actionOCGState; }

  enum State { On, Off, Toggle};
  struct StateList {
    StateList() = default;
    ~StateList() = default;
    State st;
    std::vector<Ref> list;
  };

  const std::vector<StateList>& getStateList() const { return stateList; }
  bool getPreserveRB() const { return preserveRB; }

private:
  std::vector<StateList> stateList;
  bool isValid;
  bool preserveRB;
};

//------------------------------------------------------------------------
// LinkHide
//------------------------------------------------------------------------

class LinkHide: public LinkAction {
public:
  LinkHide(const Object *hideObj);

  ~LinkHide() override;

  bool isOk() const override { return targetName != nullptr; }
  LinkActionKind getKind() const override { return actionHide; }

  // According to spec the target can be either:
  // a) A text string containing the fully qualified name of the target
  //    field.
  // b) An indirect reference to an annotation dictionary.
  // c) An array of "such dictionaries or text strings".
  //
  // While b / c appear to be very uncommon and can't easily be
  // created with Adobe Acrobat DC. So only support hide
  // actions with named targets (yet).
  bool hasTargetName() const { return targetName != nullptr; }
  const GooString *getTargetName() const { return targetName; }

  // Should this action show or hide.
  bool isShowAction() const { return show; }

private:
  GooString *targetName;
  bool show;
};

//------------------------------------------------------------------------
// LinkUnknown
//------------------------------------------------------------------------

class LinkUnknown: public LinkAction {
public:

  // Build a LinkUnknown with the specified action type.
  LinkUnknown(const char *actionA);

  // Destructor.
  ~LinkUnknown() override;

  // Was the LinkUnknown create successfully?
  bool isOk() const override { return action != nullptr; }

  // Accessors.
  LinkActionKind getKind() const override { return actionUnknown; }
  const GooString *getAction() const { return action; }

private:

  GooString *action;		// action subtype
};

//------------------------------------------------------------------------
// Links
//------------------------------------------------------------------------

class Links {
public:

  // Extract links from array of annotations.
  Links(Annots *annots);

  // Destructor.
  ~Links();

  Links(const Links &) = delete;
  Links& operator=(const Links &) = delete;

  // Iterate through list of links.
  int getNumLinks() const { return links.size(); }
  AnnotLink *getLink(int i) const { return links[i]; }

private:

  std::vector<AnnotLink *> links;
};

#endif
