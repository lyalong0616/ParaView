/*=========================================================================

  Program:   ParaView
  Module:    vtkXDMFReaderModule.cxx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

Copyright (c) 2000-2001 Kitware Inc. 469 Clifton Corporate Parkway,
Clifton Park, NY, 12065, USA.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

 * Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

 * Neither the name of Kitware nor the names of any contributors may be used
   to endorse or promote products derived from this software without specific 
   prior written permission.

 * Modified source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/
#include "vtkXDMFReaderModule.h"

#include "vtkCollectionIterator.h"
#include "vtkKWFrame.h"
#include "vtkKWLabeledFrame.h"
#include "vtkKWMessageDialog.h"
#include "vtkKWOptionMenu.h"
#include "vtkObjectFactory.h"
#include "vtkPVApplication.h"
#include "vtkPVData.h"
#include "vtkPVFileEntry.h"
#include "vtkPVWidgetCollection.h"
#include "vtkPVWindow.h"
#include "vtkString.h"
#include "vtkVector.txx"
#include "vtkVectorIterator.txx"

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkXDMFReaderModule);
vtkCxxRevisionMacro(vtkXDMFReaderModule, "1.2");

int vtkXDMFReaderModuleCommand(ClientData cd, Tcl_Interp *interp,
                        int argc, char *argv[]);

//----------------------------------------------------------------------------
vtkXDMFReaderModule::vtkXDMFReaderModule()
{
  this->DomainMenu = 0;
  this->GridMenu = 0;

  this->Grid = 0;
  this->Domain = 0;
}

//----------------------------------------------------------------------------
vtkXDMFReaderModule::~vtkXDMFReaderModule()
{
}

//----------------------------------------------------------------------------
int vtkXDMFReaderModule::Initialize(const char* fname, 
                                   vtkPVReaderModule*& clone)
{ 
  if (this->ClonePrototypeInternal(reinterpret_cast<vtkPVSource*&>(clone)) 
      != VTK_OK)
    {
    vtkErrorMacro("Error creating reader " << this->GetClassName()
                  << endl);
    clone = 0;
    return VTK_ERROR;
    }
  
  const char* res = clone->GetVTKSourceTclName();
  this->Script("%s Set%s %s", res, "FileName", fname);
  this->SetGrid(0);
  this->SetDomain(0);

  return VTK_OK;
}

//----------------------------------------------------------------------------
int vtkXDMFReaderModule::ReadFileInformation(const char* fname)
{
  vtkPVApplication* pvApp = this->GetPVApplication();
  if ( !this->Grid || !this->Domain )
    {
    // Prompt user
    const char* res = this->GetVTKSourceTclName();

    // Change the hardcoded "FileName" to something more elaborated
    this->Script("%s UpdateInformation", res);

    vtkKWMessageDialog* dlg = vtkKWMessageDialog::New();
    dlg->SetTitle("Domain and Grid Selection");
    dlg->SetMasterWindow(this->GetPVWindow());
    dlg->Create(pvApp,0);
    char *msg="Select Domain and Grid";
    dlg->SetText(msg);
    vtkKWLabeledFrame* frame = vtkKWLabeledFrame::New();
    frame->SetParent(dlg->GetMessageDialogFrame());
    frame->Create(pvApp, 0);
    frame->SetLabel("Domain and Grid Selection");

    this->DomainMenu = vtkKWOptionMenu::New();
    this->DomainMenu->SetParent(frame->GetFrame());
    this->DomainMenu->Create(pvApp, 0);
    this->UpdateDomains(res);

    
    this->GridMenu = vtkKWOptionMenu::New();
    this->GridMenu->SetParent(frame->GetFrame());
    this->GridMenu->Create(pvApp, 0);
    this->UpdateGrids(res);

    this->Script("%s configure -height 1", this->DomainMenu->GetWidgetName());
    this->Script("%s configure -height 2", this->GridMenu->GetWidgetName());
    this->Script("pack %s -expand yes -fill x -side top -pady 2", 
                 this->DomainMenu->GetWidgetName());
    this->Script("pack %s -expand yes -fill x -side top -pady 2", 
                 this->GridMenu->GetWidgetName());
    this->Script("pack %s -expand yes -fill x -side bottom -pady 2", 
                 frame->GetWidgetName());
    
    if ( dlg->Invoke() )
      {
      this->SetDomain(this->DomainMenu->GetValue());
      this->SetGrid(this->GridMenu->GetValue());
      }

    frame->Delete();
    dlg->Delete();
    
    this->DomainMenu->Delete();
    this->DomainMenu = 0;
    this->GridMenu->Delete();
    this->GridMenu = 0;
    
    if ( this->Domain )
      {
      this->Script("%s SetDomainName \"%s\"", res, this->Domain);
      }
    
    if ( this->Grid )
      {
      this->Script("%s SetGridName \"%s\"", res, this->Grid);
      }

    this->Script("%s UpdateInformation", res);
    }

  if ( this->Domain )
    {
    pvApp->BroadcastScript("%s SetDomainName \"%s\"", 
                           this->GetVTKSourceTclName(), 
                           this->Domain);
    pvApp->AddTraceEntry("$kw(%s) SetDomain {%s}", 
                         this->GetTclName(), 
                         this->Domain);
    }
    
  if ( this->Grid )
    {
    pvApp->BroadcastScript("%s SetGridName \"%s\"", this->GetVTKSourceTclName(), 
                           this->Grid);
    pvApp->AddTraceEntry("$kw(%s) SetGrid {%s}", this->GetTclName(), this->Grid);
    }

  int retVal = VTK_OK;

  this->Script("%s UpdateInformation", 
               this->GetVTKSourceTclName());
  retVal = this->InitializeClone(0, 1);

  if (retVal != VTK_OK)
    {
    return retVal;
    }

  retVal =  this->Superclass::ReadFileInformation(fname);
  if (retVal != VTK_OK)
    {
    return retVal;
    }

  // We called UpdateInformation, we need to update the widgets.
  vtkCollectionIterator* it = this->GetWidgets()->NewIterator();
  for ( it->InitTraversal(); !it->IsDoneWithTraversal(); it->GoToNextItem())
    {
    vtkPVWidget *pvw = static_cast<vtkPVWidget*>(it->GetObject());
    pvw->ModifiedCallback();
    }
  it->Delete();
  this->UpdateParameterWidgets();
  return VTK_OK;
}

//----------------------------------------------------------------------------
int vtkXDMFReaderModule::Finalize(const char* fname)
{
  return this->Superclass::Finalize(fname);
}

//----------------------------------------------------------------------------
void vtkXDMFReaderModule::UpdateGrids(const char* ob)
{
  int cc;
  int num;
  this->Script("%s UpdateInformation", ob);
  num = atoi(this->Script("%s GetNumberOfGrids", ob));
  this->GridMenu->ClearEntries();
  for ( cc = 0; cc < num; cc ++ )
    {
    char* gname = vtkString::Duplicate(this->Script("%s GetGridName %d", ob, cc));
    this->GridMenu->AddEntry(gname);
    if ( !cc )
      {
      this->GridMenu->SetValue(gname);
      }
    delete [] gname;
    }
}

//----------------------------------------------------------------------------
void vtkXDMFReaderModule::UpdateDomains(const char* ob)
{
  int cc;
  int num;
  char buffer[1024];
  this->Script("%s UpdateInformation", ob);
  num = atoi(this->Script("%s GetNumberOfDomains", ob));
  this->DomainMenu->ClearEntries();
  for ( cc = 0; cc < num; cc ++ )
    {
    char* dname = vtkString::Duplicate(this->Script("%s GetDomainName %d", ob, cc));
    sprintf(buffer, "UpdateGrids %s", ob);
    this->DomainMenu->AddEntryWithCommand(dname, this->GetTclName(), buffer);
    if ( !cc )
      {
      this->DomainMenu->SetValue(dname);
      }
    delete [] dname;
    }
 
}

//----------------------------------------------------------------------------
void vtkXDMFReaderModule::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);
  os << indent << "Grid: " << (this->Grid?this->Grid:"(none)") << endl;
  os << indent << "Domain: " << (this->Domain?this->Domain:"(none)") << endl;
}
