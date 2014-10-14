//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _EDITOR_WORK_UNIT_H_
#define _EDITOR_WORK_UNIT_H_

#include "Point.h"
#include "tnlVector.h"
#include <boost/shared_ptr.hpp>

using namespace TNL;

namespace Zap { 

// Forward declarations of Zap objects must be in Zap namespace but not in Editor namespace
class BfObject;
class Level;
class EditorUserInterface;
class Geometry;

namespace Editor
{

enum EditorAction {
   ActionCreate,
   ActionDelete,
   ActionChange,
   /*ActionChangeAttribs,
   ActionChangeTeams*/
};


class EditorWorkUnit
{
private:
   EditorAction mAction;

protected:
   boost::shared_ptr<Level> mLevel;
   EditorUserInterface *mEditor;      // Will be NULL in testing

public:
   EditorWorkUnit(boost::shared_ptr<Level> level, EditorUserInterface *editor, EditorAction action);  // Constructor
   virtual ~EditorWorkUnit();                                                                         // Destructor

   virtual void undo() = 0;
   virtual void redo() = 0;
};


////////////////////////////////////
////////////////////////////////////

class EditorWorkUnitCreate : public EditorWorkUnit
{
   typedef EditorWorkUnit Parent;

private:
   BfObject *mCreatedObject;

public:
   // Constructor
   EditorWorkUnitCreate(const boost::shared_ptr<Level> &level, 
                        EditorUserInterface *editor,
                        const BfObject *bfObjects);

   virtual ~EditorWorkUnitCreate();                                     // Destructor

   void undo();
   void redo();
};


////////////////////////////////////
////////////////////////////////////

class EditorWorkUnitDelete : public EditorWorkUnit
{
   typedef EditorWorkUnit Parent;

private:
   BfObject *mDeletedObject;

public:
   // Constructor
   EditorWorkUnitDelete(const boost::shared_ptr<Level> &level, 
                        EditorUserInterface *editor,
                        const BfObject *bfObjects);   

   virtual ~EditorWorkUnitDelete();    // Destructor

   void undo();
   void redo();
};



////////////////////////////////////
////////////////////////////////////

class EditorWorkUnitChange : public EditorWorkUnit
{
   typedef EditorWorkUnit Parent;

private:
   BfObject *mOrigObject;
   BfObject *mChangedObject;

public:
   // Constructor
   EditorWorkUnitChange(const boost::shared_ptr<Level> &level, 
                        EditorUserInterface *editor,
                        const BfObject *origObject,
                        const BfObject *changedObject);
    
    // Destructor
   virtual ~EditorWorkUnitChange();                                    

   void undo();
   void redo();
};


////////////////////////////////////
////////////////////////////////////

class EditorWorkUnitGroup : public EditorWorkUnit
{
   typedef EditorWorkUnit Parent;

private:
   Vector<EditorWorkUnit *> mWorkUnits;

public:
   // Constructor
   EditorWorkUnitGroup(const boost::shared_ptr<Level> &level, 
                        EditorUserInterface *editor,
                        const Vector<EditorWorkUnit *> &workUnits);
    
    // Destructor
   virtual ~EditorWorkUnitGroup();                                    

   void undo();
   void redo();
};



} } // Nested namespace

#endif