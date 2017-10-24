import os
import unittest
import vtk, qt, ctk, slicer
from slicer.ScriptedLoadableModule import *
import logging

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

    # Customize the widget
    cliw = self.cliWidget
    for name in ["Apply", "AutoRun", "Restore Defaults", "Cancel", "Parameter set:", "Input Label Map"]:
      w = slicer.util.findChildren(widget=cliw, text=name)
      if len(w) > 0:
        cliw.layout().removeWidget(w[0])
        w[0].hide() 

    for w in slicer.util.findChildren(widget=cliw, className="qMRMLNodeComboBox"):
      if w.toolTip == "<p>Seeds for diffusion. If not specified, full brain tractography will be performed, and the algorithm will start from every voxel in the brain mask where the Generalized Anisotropy is bigger than 0.18</p>":
        cliw.layout().removeWidget(w)
        w.hide()

  
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
    self.markupSelectorFrame.setLayout(qt.QHBoxLayout())
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

      self.markupSelectorFrame.layout().addWidget(w)
      self.markupSelector = w

    # enable checkbox
    self.enableCBFrame = qt.QFrame(self.parent)
    self.enableCBFrame.setLayout(qt.QHBoxLayout())
    self.parent.layout().addWidget(self.enableCBFrame)
    
    self.enableSeedingCB = qt.QCheckBox()
    self.enableCBFrame.layout().addWidget(self.enableSeedingCB)

    self.enableSeedingCBLabel = qt.QLabel("Enable interactive seeding")
    self.enableCBFrame.layout().addWidget(self.enableSeedingCBLabel)
    self.enableCBFrame.layout().addStretch(0)

    self.interactFrame.layout().addWidget(self.markupSelectorFrame)
    self.interactFrame.layout().addWidget(self.enableCBFrame)
    self.interactFrame.layout().addStretch(0)

    self.tabFrame.connect('currentChanged(int)', self.onTabChanged)
    self.enableSeedingCB.connect('stateChanged(int)', self.onSeedingCBChanged)
    self.logic = slicer.vtkSlicerInteractiveUKFLogic()

  def onTabChanged(self, index):
    print "onTabChanged"
    if index != 1:
      return

    dwi = self.dwiSelector.currentNode()
    fbnode = self.fiberBundleSelector.currentNode()
    mask = self.maskSelector.currentNode()
  
    # disable interaction and return if no nodes selected
    if (dwi == None or fbnode == None):
      self.interactFrame.enabled = 0
      return

    cliNode = self.cliWidget.currentCommandLineModuleNode()
    #print cliNode
    #print self.logic
   
    self.cliWidget.apply(1) # 1: run synchronously
    self.interactFrame.enabled = 1
    self.logic.SetInputVolumes(dwi, mask, None)

  def onMarkupsChanged(self, markupNode, event):
    self.runSeeding(markupNode, 0)

  def onSeedingCBChanged(self, state):
    markups = self.markupSelector.currentNode()
    if state == 0 or markups == None:
      markups.RemoveObserver(self.onMarkupsChanged)
      return

    markups.AddObserver(slicer.vtkMRMLMarkupsNode.PointModifiedEvent,
                        self.onMarkupsChanged)

  def runSeeding(self, markupNode, pointId):
    dwi = self.dwiSelector.currentNode()
    fbnode = self.fiberBundleSelector.currentNode()

    if markupNode == None:
      return

    self.logic.RunFromSeedPoints(dwi, fbnode, markupNode, pointId)


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
