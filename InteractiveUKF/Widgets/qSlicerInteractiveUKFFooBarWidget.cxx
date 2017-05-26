/*==============================================================================

  Program: 3D Slicer

  Copyright (c) Kitware Inc.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  This file was originally developed by Jean-Christophe Fillion-Robin, Kitware Inc.
  and was partially funded by NIH grant 3P41RR013218-12S1

==============================================================================*/

// FooBar Widgets includes
#include "qSlicerInteractiveUKFFooBarWidget.h"
#include "ui_qSlicerInteractiveUKFFooBarWidget.h"

//-----------------------------------------------------------------------------
/// \ingroup Slicer_QtModules_InteractiveUKF
class qSlicerInteractiveUKFFooBarWidgetPrivate
  : public Ui_qSlicerInteractiveUKFFooBarWidget
{
  Q_DECLARE_PUBLIC(qSlicerInteractiveUKFFooBarWidget);
protected:
  qSlicerInteractiveUKFFooBarWidget* const q_ptr;

public:
  qSlicerInteractiveUKFFooBarWidgetPrivate(
    qSlicerInteractiveUKFFooBarWidget& object);
  virtual void setupUi(qSlicerInteractiveUKFFooBarWidget*);
};

// --------------------------------------------------------------------------
qSlicerInteractiveUKFFooBarWidgetPrivate
::qSlicerInteractiveUKFFooBarWidgetPrivate(
  qSlicerInteractiveUKFFooBarWidget& object)
  : q_ptr(&object)
{
}

// --------------------------------------------------------------------------
void qSlicerInteractiveUKFFooBarWidgetPrivate
::setupUi(qSlicerInteractiveUKFFooBarWidget* widget)
{
  this->Ui_qSlicerInteractiveUKFFooBarWidget::setupUi(widget);
}

//-----------------------------------------------------------------------------
// qSlicerInteractiveUKFFooBarWidget methods

//-----------------------------------------------------------------------------
qSlicerInteractiveUKFFooBarWidget
::qSlicerInteractiveUKFFooBarWidget(QWidget* parentWidget)
  : Superclass( parentWidget )
  , d_ptr( new qSlicerInteractiveUKFFooBarWidgetPrivate(*this) )
{
  Q_D(qSlicerInteractiveUKFFooBarWidget);
  d->setupUi(this);
}

//-----------------------------------------------------------------------------
qSlicerInteractiveUKFFooBarWidget
::~qSlicerInteractiveUKFFooBarWidget()
{
}
