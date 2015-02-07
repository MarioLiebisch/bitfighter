//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _UIDIAGNOSTIC_H_
#define _UIDIAGNOSTIC_H_

#include "UI.h"

namespace Zap
{

// Diagnostics UI
class DiagnosticUserInterface : public UserInterface
{
   typedef UserInterface Parent;
private:
   bool mActive;
   S32 mCurPage;
public:
   explicit DiagnosticUserInterface(ClientGame *game, UIManager *uiManager);     // Constructor
   virtual ~DiagnosticUserInterface();

   void onActivate();
   void idle(U32 t);
   void render() const;
   void quit();
   bool onKeyDown(InputCode inputCode);
   bool isActive() const;
};


}

#endif


