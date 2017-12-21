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

// SEM includes
#include <ModuleDescription.h>

// Slicer includes
#include <vtkMRMLScene.h>
#include <vtkNRRDWriter.h>
#include <vtkMRMLModelNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLDiffusionWeightedVolumeNode.h>
#include <vtkMRMLCommandLineModuleNode.h>
#include <vtkSlicerCLIModuleLogic.h>

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
#include <cli.h>

static Tractography* g_tracto;

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

static int CLILoader(int argc, char** argv)
{
  UKFSettings settings;
  ukf_parse_cli(argc, argv, settings);

  g_tracto = new Tractography(settings);

  return EXIT_SUCCESS;
}

//---------------------------------------------------------------------------
bool vtkSlicerInteractiveUKFLogic::InitTractography(vtkMRMLCommandLineModuleNode* n)
{
  assert(this->GetMRMLScene());
  assert(this->GetMRMLApplicationLogic());

  std::stringstream entrypoint;
  entrypoint << "slicer:";
  entrypoint << reinterpret_cast<void*>(&CLILoader);

  vtkNew<vtkMRMLCommandLineModuleNode> cli;
  cli->Copy(n);
  cli->GetModuleDescription().SetTarget(entrypoint.str());
  cli->GetModuleDescription().SetType("SharedObjectModule");

  /* Make a copy of the node to run directly */
  vtkNew<vtkSlicerCLIModuleLogic> cli_logic;
  cli_logic->SetMRMLScene(this->GetMRMLScene());
  cli_logic->SetMRMLApplicationLogic(this->GetMRMLApplicationLogic());
//  cli_logic->SetAllowInMemoryTransfer(true);
  cli_logic->ApplyAndWait(cli.GetPointer(), false);

  return EXIT_SUCCESS;
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
void vtkSlicerInteractiveUKFLogic::SetInputVolumes(
    vtkMRMLDiffusionWeightedVolumeNode* dwiNode,
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

  std::cout << "Addr of g_tracto: " << g_tracto << std::endl;

  Tractography* tract = dynamic_cast<Tractography*>(g_tracto);
  if (!tract)
    {
    std::cerr << "No g_tracto!" << std::endl;
    return;
    }

  tract->SetData(nrrd, mask, seed, false /*normalizedDWIData*/);
  tract->UpdateFilterModelType();
}

//---------------------------------------------------------------------------
void vtkSlicerInteractiveUKFLogic::RunFromSeedPoints
      (vtkMRMLDiffusionWeightedVolumeNode* dwiNode,
       vtkMRMLModelNode* fbNode,
       vtkMRMLMarkupsFiducialNode* markupsNode)
{
  assert(fbNode->IsA("vtkMRMLFiberBundleNode"));
  assert(markupsNode->IsA("vtkMRMLMarkupsNode"));
  assert(dwiNode->IsA("vtkMRMLDiffusionWeightedVolumeNode"));

  Tractography* tract = dynamic_cast<Tractography*>(g_tracto);
  if (!tract)
    {
    std::cerr << "No g_tracto!" << std::endl;
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

  for (size_t i = 0; i < markupsNode->GetNumberOfFiducials(); i++)
    {
    vec3_t pos_in, pos_out;
    markupsNode->GetNthFiducialPosition(i, pos_in.data());
    RASxfmIJK->TransformPoint(pos_in.data(), pos_out.data());
    pos_out = vec3_t(pos_out[2], pos_out[1], pos_out[0]); // axis order for nrrd
    seeds.push_back(pos_out);
    }

  tract->SetSeeds(seeds);

  tract->SetOutputPolyData(pd);
  tract->Run();

  fbNode->SetAndObservePolyData(pd);
}

void vtkSlicerInteractiveUKFLogic::set_seedsPerVoxel(double val)      { g_tracto->_seeds_per_voxel = val; };
void vtkSlicerInteractiveUKFLogic::set_stoppingFA(double val)         { g_tracto->_fa_min = val; };
void vtkSlicerInteractiveUKFLogic::set_seedingThreshold(double val)   { g_tracto->_seeding_threshold = val; };
void vtkSlicerInteractiveUKFLogic::set_stoppingThreshold(double val)  { g_tracto->_mean_signal_min = val; };
void vtkSlicerInteractiveUKFLogic::set_numTensor(size_t val)          { g_tracto->_num_tensors = val; };
void vtkSlicerInteractiveUKFLogic::set_stepLength(double val)         { g_tracto->_stepLength = val; };
void vtkSlicerInteractiveUKFLogic::set_recordLength(double val)
  { g_tracto->_steps_per_record = val/g_tracto->_stepLength; };

void vtkSlicerInteractiveUKFLogic::set_noddi(bool val)                { g_tracto->_noddi = val; }

