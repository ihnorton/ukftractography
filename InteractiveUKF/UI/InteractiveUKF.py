import os
import unittest
import vtk, qt, ctk, slicer
from slicer.ScriptedLoadableModule import *
import logging

from slicer.util import findChild, findChildren

#
# InteractiveUKF
#

class InteractiveUKF(ScriptedLoadableModule):
  """Uses ScriptedLoadableModule base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def __init__(self, parent):
    import string
    ScriptedLoadableModule.__init__(self, parent)
    self.parent.title = "Interactive UKF"
    self.parent.categories = ["Diffusion"]
    self.parent.dependencies = []
    self.parent.contributors = ["Isaiah Norton (BWH)"]
    self.parent.helpText = """
"""
    self.parent.helpText += self.getDefaultModuleDocumentationLink()
    self.parent.acknowledgementText = """
Supported by NA-MIC, NAC, BIRN, NCIGT, and the Slicer Community. See http://www.slicer.org for details.  Module implemented by Steve Pieper.
"""

#
# InteractiveUKFWidget
#

class It():
  def __init__(self, node):
    self.node = node

  def __enter__(self):
    return self.node

  def __exit__(self, type, value, traceback):
    return False

class InteractiveUKFWidget(ScriptedLoadableModuleWidget):
  """Uses ScriptedLoadableModuleWidget base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """
  def __del__(self):
    self.unsetConnections()

  def setup(self):
    ScriptedLoadableModuleWidget.setup(self)

    self.logic = None
    self.grayscaleNode = None
    self.labelNode = None
    self.fileName = None
    self.fileDialog = None


    # Instantiate and connect widgets ...
    #

    self.tabFrame = qt.QTabWidget(self.parent)
    self.tabFrame.setLayout(qt.QHBoxLayout())
    self.parent.layout().addWidget(self.tabFrame)
    
    self.cliWidget = slicer.modules.ukftractography.createNewWidgetRepresentation()
    self.cliWidget.setMRMLScene(slicer.mrmlScene)

    ######################
    # Customize the widget
    #

    # labels and other anonymous UI elements
    cliw = self.cliWidget
    for name in ["Apply", "AutoRun", "Restore Defaults", "Cancel", "Parameter set:",
                 "Input Label Map", "ROI label to use for seeding",
                 "Signal Parameters (Expert Only)", "Not Used: Debug/Develop Only"]:
      w = slicer.util.findChildren(widget=cliw, text=name)
      if len(w) > 0:
        w = w[0]
        cliw.layout().removeWidget(w)
        w.hide() 

    # named selector widgets correspond to CLI argument name
    for name in ["labels", "seedsFile"]:
      with It(slicer.util.findChild(widget=cliw, name=name)) as w:
        cliw.layout().removeWidget(w)
        w.hide()

    with It(slicer.util.findChildren(cliw, text="Tensor Model (default)")[0]) as w:
      w.collapsed = True

    #
    # Finished customizing
    ######################

    ######################

    # get handles to important selectors
    self.dwiSelector = slicer.util.findChild(cliw, "dwiFile")
    self.fiberBundleSelector = slicer.util.findChild(cliw, "tracts")
    self.maskSelector = slicer.util.findChild(cliw, "maskFile")

    # add widget to tab frame
    self.tabFrame.addTab(self.cliWidget, "Setup")

    # Interactor frame  
    self.interactFrame = qt.QFrame(self.parent)
    self.interactFrame.setLayout(qt.QVBoxLayout())
    self.tabFrame.addTab(self.interactFrame, "Interact")
    
    # selector widget and frame
    self.markupSelectorFrame = qt.QFrame(self.parent)
    self.markupSelectorFrame.setLayout(qt.QVBoxLayout())
    self.parent.layout().addWidget(self.markupSelectorFrame)

    self.markupSelectorLabel = qt.QLabel("Interactive markup node: ", self.markupSelectorFrame)
    self.markupSelectorLabel.setToolTip( "Select the markup node for interactive seeding.")
    self.markupSelectorFrame.layout().addWidget(self.markupSelectorLabel)

    with It(slicer.qMRMLNodeComboBox(self.parent)) as w:
      w.nodeTypes = ["vtkMRMLMarkupsFiducialNode"]
      w.selectNodeUponCreation = False
      w.addEnabled = False
      w.removeEnabled = False
      w.noneEnabled = True
      w.showHidden = False
      w.showChildNodeTypes = False
      w.setMRMLScene(slicer.mrmlScene)
      w.connect('currentNodeChanged(bool)', self.markupNodeSelected)

      self.markupSelectorFrame.layout().addWidget(w)
      self.markupSelector = w

    # enable checkbox
    self.enableCBFrame = qt.QFrame(self.markupSelectorFrame)
    self.enableCBFrame.setLayout(qt.QHBoxLayout())
    
    self.enableSeedingCB = qt.QCheckBox()
    self.enableCBFrame.layout().addWidget(self.enableSeedingCB)

    self.enableSeedingCBLabel = qt.QLabel("Enable interactive seeding")
    self.enableCBFrame.layout().addWidget(self.enableSeedingCBLabel)
    self.enableCBFrame.layout().addStretch(0)

    self.markupSelectorFrame.layout().addWidget(self.enableCBFrame)
    self.interactFrame.layout().addWidget(self.markupSelectorFrame)

    self.optsFrame = qt.QFrame(self.parent)
    self.optsFrame.setLayout(qt.QVBoxLayout())
    self.interactFrame.layout().addWidget(self.optsFrame)

    # Move seeding options frame to interactor 
    with It(slicer.util.findChildren(cliw, text="Tractography Options")[0]) as w:
      self.tractOpts = w
      cliw.layout().removeWidget(w)
      self.optsFrame.layout().addWidget(w)
      w.collapsed = True

      self.c_seedsPerVoxel = findChild(w, "seedsPerVoxel")
      self.c_seedingThreshold = findChild(w, "seedingThreshold")
      self.c_stoppingFA = findChild(w, "stoppingFA")
      self.c_stoppingThreshold = findChild(w, "stoppingThreshold")

      self.c_numTensor = findChild(w, "numTensor")
      self.c_stepLength = findChild(w, "stepLength")
      self.c_Qm = findChild(w, "Qm")
      self.c_recordLength = findChild(w, "recordLength")
      self.c_recordLength.setValue(2)
      # TODO numThread?
      # TODO maxTract, NMSE    

    # Move tensor options frame to interactor 
    with It(slicer.util.findChildren(cliw, text="Tensor Model (default)")[0]) as w:
      self.tractTensorOpts = w
      cliw.layout().removeWidget(w)
      self.optsFrame.layout().addWidget(w)

    # Move NODDI options frame to interactor 
    with It(slicer.util.findChildren(cliw, text="NODDI Model")[0]) as w:
      self.noddiOpts = w
      cliw.layout().removeWidget(w)
      self.optsFrame.layout().addWidget(w)
      self.c_noddi = findChild(w, "noddi")

    self.tabFrame.connect('currentChanged(int)', self.onTabChanged)
    self.enableSeedingCB.connect('stateChanged(int)', self.onSeedingCBChanged)
    self.logic = slicer.vtkSlicerInteractiveUKFLogic()

    # This needs to be last so that all widgets are pushed to top
    self.interactFrame.layout().addStretch(0) 

  def enableInteraction(self):
    self.interactFrame.enabled = True
    self.optsFrame.enabled = False
    self.enableSeedingCB.enabled = False 

    self.c_recordLength.connect('valueChanged(double)', self.on_recordLength)
    self.c_Qm.connect('valueChanged(double)', self.on_Qm)
    self.c_seedsPerVoxel.connect('valueChanged(double)', self.on_seedsPerVoxel)
    self.c_seedingThreshold.connect('valueChanged(double)', self.on_seedingThreshold)
    self.c_stoppingFA.connect('valueChanged(double)', self.on_stoppingFA)
    self.c_stoppingThreshold.connect('valueChanged(double)', self.on_stoppingThreshold)
    self.c_numTensor.connect('valueChanged()', self.on_numTensor)
    self.c_stepLength.connect('valueChanged(double)', self.on_stepLength)
    self.c_noddi.connect('stateChanged(int)', self.on_noddi)

  def disableInteraction(self):
    self.interactFrame.enabled = False
    self.optsFrame.enabled = False
    self.enableSeedingCB.enabled = False 

    self.c_seedsPerVoxel.disconnect('valueChanged(double)', self.on_seedsPerVoxel)
    self.c_seedingThreshold.disconnect('valueChanged(double)', self.on_seedingThreshold)
    self.c_stoppingFA.disconnect('valueChanged(double)', self.on_stoppingFA)
    self.c_stoppingThreshold.disconnect('valueChanged(double)', self.on_stoppingThreshold)
    self.c_numTensor.disconnect('valueChanged()', self.on_numTensor)
    self.c_stepLength.disconnect('valueChanged(double)', self.on_stepLength)
    self.c_Qm.disconnect('valueChanged(double)', self.on_Qm)
    self.c_recordLength.disconnect('valueChanged(double)', self.on_recordLength)
    self.c_noddi.disconnect('stateChanged(int)', self.on_noddi)

  def markupNodeSelected(self, state):
    self.optsFrame.enabled = state
    self.enableSeedingCB.enabled = state

  def onTabChanged(self, index):
    # I/O selector tab
    if index == 0:
      self.disableInteraction()
      return
    
    # interaction tab
    elif index == 1:
      # run CLI widget to set inputs and calculate averaged volume
      self.cliWidget.apply(1) # 1: run synchronously
      
      dwi = self.dwiSelector.currentNode()
      fbnode = self.fiberBundleSelector.currentNode()
      mask = self.maskSelector.currentNode()
      markups = self.markupSelector.currentNode()
  
      # disable interaction and return if no nodes selected
      if (dwi == None or fbnode == None):
        self.interactFrame.enabled = 0
        return

      self.logic.SetInputVolumes(dwi, mask, None)
      self.enableInteraction()
      
      if (markups != None):
        self.enableSeedingCB.enabled = True
    else:
      raise("Unhandled tab!")

  def onMarkupsChanged(self, markupNode, event):
    self.runSeeding(markupNode)

  def onSeedingCBChanged(self, state):
    markups = self.markupSelector.currentNode()
    if state == 0 or markups == None:
      markups.RemoveObservers(slicer.vtkMRMLMarkupsNode.PointModifiedEvent,
                              self.onMarkupsChanged)
      return

    markups.AddObserver(slicer.vtkMRMLMarkupsNode.PointModifiedEvent,
                        self.onMarkupsChanged)

  def runSeeding(self, markupNode):
    dwi = self.dwiSelector.currentNode()
    fbnode = self.fiberBundleSelector.currentNode()

    if markupNode == None or fbnode == None:
      raise("No markup node selected in InteractiveUKF.py:runSeeding(...)")
      return

    self.logic.RunFromSeedPoints(dwi, fbnode, markupNode)

  def rerunSeeding(self):
    markups = self.markupSelector.currentNode()
    # TODO safety check 
    self.runSeeding(markups)

  def on_seedsPerVoxel(self, value):
    self.logic.set_seedsPerVoxel(value)
    self.rerunSeeding()

  def on_seedingThreshold(self, value):
    self.logic.set_seedingThreshold(value)
    self.rerunSeeding()

  def on_stoppingFA(self, value):
    if not self.logic: return
    self.logic.set_stoppingFA(value)
    self.rerunSeeding()

  def on_stoppingThreshold(self, value):
    if not self.logic: return
    self.logic.set_stoppingThreshold(value)
    self.rerunSeeding()
  
  def on_numTensor(self):
    bg = self.c_numTensors.children()[0]
    val = bg.checkedButton().text
    self.logic.set_numTensor(int(val))
    self.rerunSeeding()

  def on_stepLength(self, value):
    if not self.logic: return
    self.logic.set_stoppingThreshold(value)
    self.rerunSeeding()
 
  def on_Qm(self, value):
    if not self.logic: return
    self.logic.set_Qm(value)
    self.rerunSeeding()
 
  def on_recordLength(self,value):
    if not self.logic: return
    self.logic.set_recordLength(value)
    self.rerunSeeding()

  def on_noddi(self,value):
    if not self.logic: return
    print("on_noddi: ", value)
    if value == 2: # Qt::Checked
      self.logic.set_noddi(True)
    else:
      self.logic.set_noddi(False)
    self.rerunSeeding()

#on_seedsPerVoxel
#on_seedingThreshold
#on_stoppingThreshold
#on_numTensor
#on_stepLength
#on_Qm
#on_recordLength



#
# InteractiveUKFLogic
#

class InteractiveUKFLogic(ScriptedLoadableModuleLogic):
  """Implement the logic to calculate label statistics.
  Nodes are passed in as arguments.
  Results are stored as 'statistics' instance variable.
  Uses ScriptedLoadableModuleLogic base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def __init__(self):
    pass

class InteractiveUKFTest(ScriptedLoadableModuleTest):
  """
  This is the test case for your scripted module.
  Uses ScriptedLoadableModuleTest base class, available at:
  https://github.com/Slicer/Slicer/blob/master/Base/Python/slicer/ScriptedLoadableModule.py
  """

  def setUp(self):
    """ Do whatever is needed to reset the state - typically a scene clear will be enough.
    """
    slicer.mrmlScene.Clear(0)

  def runTest(self,scenario=None):
    """Run as few or as many tests as needed here.
    """
    self.setUp()
    self.test_InteractiveUKFBasic()

  def test_InteractiveUKFBasic(self):
    """
    This tests some aspects of the label statistics
    """

    self.delayDisplay("Starting test_InteractiveUKFBasic")
    #
    # first, get some data
    #
    import SampleData
    sampleDataLogic = SampleData.SampleDataLogic()
    mrHead = sampleDataLogic.downloadMRHead()
    ctChest = sampleDataLogic.downloadCTChest()
    self.delayDisplay('Two data sets loaded')

    volumesLogic = slicer.modules.volumes.logic()

    mrHeadLabel = volumesLogic.CreateAndAddLabelVolume( slicer.mrmlScene, mrHead, "mrHead-label" )

    warnings = volumesLogic.CheckForLabelVolumeValidity(ctChest, mrHeadLabel)

    self.delayDisplay("Warnings for mismatch:\n%s" % warnings)

    self.assertNotEqual( warnings, "" )

    warnings = volumesLogic.CheckForLabelVolumeValidity(mrHead, mrHeadLabel)

    self.delayDisplay("Warnings for match:\n%s" % warnings)

    self.assertEqual( warnings, "" )

    self.delayDisplay('test_InteractiveUKFBasic passed!')

class Slicelet(object):
  """A slicer slicelet is a module widget that comes up in stand alone mode
  implemented as a python class.
  This class provides common wrapper functionality used by all slicer modlets.
  """
  # TODO: put this in a SliceletLib
  # TODO: parse command line arge


  def __init__(self, widgetClass=None):
    self.parent = qt.QFrame()
    self.parent.setLayout( qt.QVBoxLayout() )

    # TODO: should have way to pop up python interactor
    self.buttons = qt.QFrame()
    self.buttons.setLayout( qt.QHBoxLayout() )
    self.parent.layout().addWidget(self.buttons)
    self.addDataButton = qt.QPushButton("Add Data")
    self.buttons.layout().addWidget(self.addDataButton)
    self.addDataButton.connect("clicked()",slicer.app.ioManager().openAddDataDialog)
    self.loadSceneButton = qt.QPushButton("Load Scene")
    self.buttons.layout().addWidget(self.loadSceneButton)
    self.loadSceneButton.connect("clicked()",slicer.app.ioManager().openLoadSceneDialog)

    if widgetClass:
      self.widget = widgetClass(self.parent)
      self.widget.setup()
    self.parent.show()

class InteractiveUKFSlicelet(Slicelet):
  """ Creates the interface when module is run as a stand alone gui app.
  """

  def __init__(self):
    super(InteractiveUKFSlicelet,self).__init__(InteractiveUKFWidget)


if __name__ == "__main__":
  # TODO: need a way to access and parse command line arguments
  # TODO: ideally command line args should handle --xml

  import sys
  print( sys.argv )

  slicelet = InteractiveUKFSlicelet()
