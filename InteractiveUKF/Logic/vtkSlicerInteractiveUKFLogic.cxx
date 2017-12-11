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
#include <vtkTransform.h>
#include <vtkMatrix4x4.h>

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
    {
    std::cerr << "No tracto_blob!" << std::endl;
    return;
    }

  tract->SetData(nrrd, mask, seed, false /*normalizedDWIData*/);
}

//---------------------------------------------------------------------------
void vtkSlicerInteractiveUKFLogic::RunFromSeedPoints
      (vtkMRMLDiffusionWeightedVolumeNode* dwiNode,
       vtkMRMLModelNode* fbNode,
       vtkMRMLMarkupsFiducialNode* markupsNode)
       // int pointId)
{
  assert(fbNode->IsA("vtkMRMLFiberBundleNode"));

  Tractography* tract = dynamic_cast<Tractography*>(tracto_blob);
  if (!tract)
    {
    std::cerr << "No tracto_blob!" << std::endl;
    return;
    }

  vtkPolyData* pd = fbNode->GetPolyData();
  if (pd == NULL)
    {
    pd = vtkPolyData::New();
    fbNode->SetAndObservePolyData(pd);
    pd->Delete();
    }

  vtkNew<vtkTransform> RASxfmIJK;
  vtkNew<vtkMatrix4x4> RAStoIJK;
  dwiNode->GetRASToIJKMatrix(RAStoIJK.GetPointer());
  RASxfmIJK->SetMatrix(RAStoIJK.GetPointer());
  stdVec_t seeds;

  for (size_t i = 0; i < markupsNode->GetNumberOfMarkups(); i++)
    {
    vec3_t pos_in, pos_out;
    markupsNode->GetMarkupPoint(0, i,pos_in.data());
    RASxfmIJK->TransformPoint(pos_in.data(), pos_out.data());
    pos_out = vec3_t(pos_out[2], pos_out[1], pos_out[0]); // axis order for nrrd
    seeds.push_back(pos_out);
    }

  tract->SetSeeds(seeds);

  tract->SetOutputPolyData(pd);
  tract->Run();

  pd->Modified();
  fbNode->Modified();
}

void vtkSlicerInteractiveUKFLogic::set_seedsPerVoxel(double val)      { tracto_blob->_seeds_per_voxel = val; };
void vtkSlicerInteractiveUKFLogic::set_stoppingFA(double val)         { tracto_blob->_fa_min = val; };
void vtkSlicerInteractiveUKFLogic::set_seedingThreshold(double val)   { tracto_blob->_seeding_threshold = val; };
void vtkSlicerInteractiveUKFLogic::set_stoppingThreshold(double val)  { tracto_blob->_mean_signal_min = val; };
void vtkSlicerInteractiveUKFLogic::set_numTensor(size_t val)          { tracto_blob->_num_tensors = val; };
void vtkSlicerInteractiveUKFLogic::set_stepLength(double val)         { tracto_blob->_stepLength = val; };
void vtkSlicerInteractiveUKFLogic::set_Qm(double val)                 { /* TODO */ };
void vtkSlicerInteractiveUKFLogic::set_recordLength(double val)
  { tracto_blob->_steps_per_record = val/tracto_blob->_stepLength; };

void vtkSlicerInteractiveUKFLogic::set_noddi(bool val)                { tracto_blob->_noddi = val; }

