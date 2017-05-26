/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

// InteractiveUKF Logic includes
#include "vtkSlicerInteractiveUKFLogic.h"

// MRML includes
#include <vtkMRMLScene.h>
#include <vtkNRRDWriter.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLDiffusionWeightedVolumeNode.h>

// VTK includes
#include <vtkNew.h>
#include <vtkIntArray.h>
#include <vtkObjectFactory.h>
#include <vtkPolyData.h>

// Teem includes
#include "teem/nrrd.h"

// STD includes
#include <cassert>

// UKF includes
#include <tractography.h>


extern Tractography* tracto_blob;

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerInteractiveUKFLogic);

//----------------------------------------------------------------------------
vtkSlicerInteractiveUKFLogic::vtkSlicerInteractiveUKFLogic()
{
}

//----------------------------------------------------------------------------
vtkSlicerInteractiveUKFLogic::~vtkSlicerInteractiveUKFLogic()
{
}

//----------------------------------------------------------------------------
void vtkSlicerInteractiveUKFLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkSlicerInteractiveUKFLogic::SetMRMLSceneInternal(vtkMRMLScene * newScene)
{
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
  events->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());
}

//-----------------------------------------------------------------------------
void vtkSlicerInteractiveUKFLogic::RegisterNodes()
{
  assert(this->GetMRMLScene() != 0);
}

//---------------------------------------------------------------------------
void vtkSlicerInteractiveUKFLogic::UpdateFromMRMLScene()
{
  assert(this->GetMRMLScene() != 0);
}

//---------------------------------------------------------------------------
void vtkSlicerInteractiveUKFLogic
::OnMRMLSceneNodeAdded(vtkMRMLNode* vtkNotUsed(node))
{
}

//---------------------------------------------------------------------------
void vtkSlicerInteractiveUKFLogic
::OnMRMLSceneNodeRemoved(vtkMRMLNode* vtkNotUsed(node))
{
}

//---------------------------------------------------------------------------
void setWriterProps(vtkMRMLVolumeNode* vol, vtkNRRDWriter* writer)
{
  vtkNew<vtkMatrix4x4> ijkToRas;
  vol->GetIJKToRASMatrix(ijkToRas.GetPointer());
  writer->SetIJKToRASMatrix(ijkToRas.GetPointer());
}


//---------------------------------------------------------------------------
void vtkSlicerInteractiveUKFLogic
::SetInputVolumes(vtkMRMLDiffusionWeightedVolumeNode* dwiNode,
                  vtkMRMLScalarVolumeNode* maskNode,
                  vtkMRMLScalarVolumeNode* seedNode)
{
  vtkNew<vtkNRRDWriter> writer;

  writer->SetInputConnection(dwiNode->GetImageDataConnection());
  setWriterProps((vtkMRMLVolumeNode*)dwiNode, writer.GetPointer());
  vtkNew<vtkMatrix4x4> mf;
  dwiNode->GetMeasurementFrameMatrix(mf.GetPointer());
  writer->SetMeasurementFrameMatrix(mf.GetPointer());

  vtkDoubleArray* grads = NULL;
  vtkDoubleArray* bValues = NULL;
  grads = dwiNode->GetDiffusionGradients();
  bValues = dwiNode->GetBValues();

  if (grads)
    writer->SetDiffusionGradients(grads);
  if (bValues)
    writer->SetBValues(bValues);

  Nrrd* nrrd = (Nrrd*)writer->MakeNRRD();

  vtkNew<vtkNRRDWriter> maskWriter;
  maskWriter->SetInputConnection(maskNode->GetImageDataConnection());
  setWriterProps((vtkMRMLVolumeNode*)maskNode, maskWriter.GetPointer());

  Nrrd* mask = (Nrrd*)maskWriter->MakeNRRD();

  vtkNew<vtkNRRDWriter> seedWriter;
  Nrrd* seed = NULL;
  if (seedNode)
    {
    seedWriter->SetInputConnection(seedNode->GetImageDataConnection());
    setWriterProps((vtkMRMLVolumeNode*)seedNode, seedWriter.GetPointer());
    seed = (Nrrd*)seedWriter->MakeNRRD();
    }

  std::cout << "Addr of tracto_blob: " << tracto_blob << std::endl;

  Tractography* tract = dynamic_cast<Tractography*>(tracto_blob);
  if (!tract)
    std::cerr << "No tracto_blob!" << std::endl;

  tract->SetData(nrrd, mask, seed, false /*normalizedDWIData*/);
}

//---------------------------------------------------------------------------
void vtkSlicerInteractiveUKFLogic
::RunFromSeedPoints(vtkMRMLMarkupsFiducialNode* markupNode,
                    vtkMRMLModelNode* fbNode)
{
  assert(fbNode->IsA("vtkMRMLFiberBundleNode"));

  Tractography* tract = dynamic_cast<Tractography*>(tracto_blob);
  assert(tract);

  vtkPolyData* pd = fbNode->GetPolyData();
  if (pd == NULL)
    {
    pd = vtkPolyData::New();
    fbNode->SetAndObservePolyData(pd);
    pd->Delete();
    }

  tract->SetOutputPolyData(pd);
  tract->Run();
}
