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

#ifndef __qSlicerInteractiveUKFFooBarWidget_h
#define __qSlicerInteractiveUKFFooBarWidget_h

// Qt includes
#include <QWidget>

// FooBar Widgets includes
#include "qSlicerInteractiveUKFModuleWidgetsExport.h"

class qSlicerInteractiveUKFFooBarWidgetPrivate;

/// \ingroup Slicer_QtModules_InteractiveUKF
class Q_SLICER_MODULE_INTERACTIVEUKF_WIDGETS_EXPORT qSlicerInteractiveUKFFooBarWidget
  : public QWidget
{
  Q_OBJECT
public:
  typedef QWidget Superclass;
  qSlicerInteractiveUKFFooBarWidget(QWidget *parent=0);
  virtual ~qSlicerInteractiveUKFFooBarWidget();

protected slots:

protected:
  QScopedPointer<qSlicerInteractiveUKFFooBarWidgetPrivate> d_ptr;

private:
  Q_DECLARE_PRIVATE(qSlicerInteractiveUKFFooBarWidget);
  Q_DISABLE_COPY(qSlicerInteractiveUKFFooBarWidget);
};

#endif
