/*=========================================================================

   Program: ParaView
   Module: pqStringVectorPropertyWidget.cxx

   Copyright (c) 2005-2012 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2. 

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=========================================================================*/

#include "pqStringVectorPropertyWidget.h"

#include "vtkPVXMLElement.h"
#include "vtkSMArraySelectionDomain.h"
#include "vtkSMDomain.h"
#include "vtkSMDomainIterator.h"
#include "vtkSMEnumerationDomain.h"
#include "vtkSMFileListDomain.h"
#include "vtkSMProperty.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMProxy.h"
#include "vtkSMSILDomain.h"
#include "vtkSMStringListDomain.h"
#include "vtkSMStringVectorProperty.h"

#include "pqApplicationCore.h"
#include "pqExodusIIVariableSelectionWidget.h"
#include "pqFileChooserWidget.h"
#include "pqProxySILModel.h"
#include "pqServerManagerModel.h"
#include "pqSILModel.h"
#include "pqSILWidget.h"
#include "pqTreeWidget.h"
#include "pqTreeWidgetSelectionHelper.h"

#include <QComboBox>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QTreeWidget>

pqStringVectorPropertyWidget::pqStringVectorPropertyWidget(vtkSMProperty *property,
                                                           vtkSMProxy *proxy,
                                                           QWidget *parent)
  : pqPropertyWidget(proxy, parent)
{
  vtkSMStringVectorProperty *ivp = vtkSMStringVectorProperty::SafeDownCast(property);
  if(!ivp)
    {
    return;
    }
  
  // find the domain
  vtkSMDomain *domain = 0;
  vtkSMDomainIterator *domainIter = ivp->NewDomainIterator();
  for(domainIter->Begin(); !domainIter->IsAtEnd(); domainIter->Next())
    {
    domain = domainIter->GetDomain();
    }
  domainIter->Delete();

  QHBoxLayout *layout = new QHBoxLayout;
  layout->setMargin(0);
  layout->setSpacing(0);

  if(vtkSMEnumerationDomain *ed = vtkSMEnumerationDomain::SafeDownCast(domain))
    {
    QComboBox *comboBox = new QComboBox(this);
    comboBox->setObjectName("ComboBox");

    for(unsigned int i = 0; i < ed->GetNumberOfEntries(); i++)
      {
      comboBox->addItem(ed->GetEntryText(i));
      }

    layout->addWidget(comboBox);
    }
  else if(vtkSMFileListDomain *fld = vtkSMFileListDomain::SafeDownCast(domain))
    {
    // dont show properties with filename
//    if(property->GetXMLLabel() == "FileName")
//      {
//      }

    pqFileChooserWidget *chooser = new pqFileChooserWidget(this);
    chooser->setObjectName("FileChooser");

    // decide whether to allow multiple files
    if(property->GetRepeatable())
      {
      // multiple file names allowed
      chooser->setForceSingleFile(false);

      this->addPropertyLink(chooser,
                            "filenames",
                            SIGNAL(filenamesChanged(QStringList)),
                            property);
      }
    else
      {
      // single file name
      chooser->setForceSingleFile(true);
      this->addPropertyLink(chooser,
                            "singleFilename",
                            SIGNAL(filenameChanged(QString)),
                            property);
      }

    // If there's a hint on the property indicating that this property expects a
    // directory name, then, we will use the directory mode.
    if (property->IsA("vtkSMStringVectorProperty") &&
        property->GetHints() &&
        property->GetHints()->FindNestedElementByName("UseDirectoryName"))
      {
      chooser->setUseDirectoryMode(true);
      }

    pqServerManagerModel *smm =
      pqApplicationCore::instance()->getServerManagerModel();
    chooser->setServer(smm->findServer(proxy->GetSession()));

    layout->addWidget(chooser);
    }
  else if(vtkSMStringListDomain *sld = vtkSMStringListDomain::SafeDownCast(domain))
    {
    QComboBox *comboBox = new QComboBox(this);
    comboBox->setObjectName("ComboBox");

    for(unsigned int i = 0; i < sld->GetNumberOfStrings(); i++)
      {
      comboBox->addItem(sld->GetString(i));
      }

    this->addPropertyLink(comboBox, "currentText", SIGNAL(currentIndexChanged(QString)), ivp);

    layout->addWidget(comboBox);
    }
  else if (vtkSMSILDomain* silDomain = vtkSMSILDomain::SafeDownCast(domain))
    {
    pqSILWidget* tree = new pqSILWidget(silDomain->GetSubTree(), this);
    tree->setObjectName("BlockSelectionWidget");

    pqSILModel* silModel = new pqSILModel(tree);

    // FIXME: This needs to be automated, we want the model to automatically
    // fetch the SIL when the domain is updated.
    silModel->update(silDomain->GetSIL());
    tree->setModel(silModel);

    this->addPropertyLink(tree->activeModel(), "values",
      SIGNAL(valuesChanged()), property);

    // FIXME: HIDE WIDGET LABEL
    layout->addWidget(tree);
    }
  else if(vtkSMArraySelectionDomain *asd = vtkSMArraySelectionDomain::SafeDownCast(domain))
    {
    pqExodusIIVariableSelectionWidget* selectorWidget =
      new pqExodusIIVariableSelectionWidget(this);
    selectorWidget->setObjectName("ArraySelectionWidget");
    selectorWidget->setRootIsDecorated(false);
    selectorWidget->setHeaderLabel(property->GetXMLLabel());
    this->addPropertyLink(
      selectorWidget, proxy->GetPropertyName(property),
      SIGNAL(widgetModified()), property);

    // FIXME: HIDE WIDGET LABEL

    layout->addWidget(selectorWidget);
   }
  this->setLayout(layout);
}
