//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _UIERRORMSG_H_
#define _UIERRORMSG_H_

#include "UI.h"   // Parent class

#include "SymbolShape.h"



namespace Zap
{

using namespace UI;

class AbstractMessageUserInterface : public UserInterface
{
   typedef UserInterface Parent;

private:
   S32 mMaxLines;

public:
   explicit AbstractMessageUserInterface(ClientGame *game);      // Constructor
   virtual ~AbstractMessageUserInterface();                      // Destructor

   static const S32 MAX_LINES = 9;
   SymbolShapePtr mMessage[MAX_LINES];

   SymbolShapePtr mTitle;
   SymbolShapePtr mInstr;
   void onActivate();
   void setMessage(const string &message);

   void setMaxLines(S32 lines);     // Display no more than this number of lines
   void setTitle(const string &title);
   void setInstr(const string &instr);
   void render();
   void quit();
   virtual void reset();
   virtual bool onKeyDown(InputCode inputCode);
};


////////////////////////////////////////
////////////////////////////////////////

class ErrorMessageUserInterface : public AbstractMessageUserInterface
{
   typedef AbstractMessageUserInterface Parent;

public:
   explicit ErrorMessageUserInterface(ClientGame *game);      // Constructor
   virtual ~ErrorMessageUserInterface();

   bool onKeyDown(InputCode inputCode);
};


}

#endif


