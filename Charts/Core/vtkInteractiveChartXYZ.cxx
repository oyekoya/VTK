/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkInteractiveChartXYZ.h

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkInteractiveChartXYZ.h"

#include "vtkAnnotationLink.h"
#include "vtkAxis.h"
#include "vtkCommand.h"
#include "vtkContext2D.h"
#include "vtkContext3D.h"
#include "vtkContextKeyEvent.h"
#include "vtkContextMouseEvent.h"
#include "vtkContextScene.h"
#include "vtkFloatArray.h"
#include "vtkIdTypeArray.h"
#include "vtkLookupTable.h"
#include "vtkMath.h"
#include "vtkOpenGLContextDevice3D.h"
#include "vtkPlane.h"
#include "vtkPoints2D.h"
#include "vtkTable.h"
#include "vtkTextProperty.h"
#include "vtkTransform.h"
#include "vtkUnsignedCharArray.h"
#include "vtkVector.h"
#include "vtkVectorOperators.h"

#include "vtkPen.h"
#include "vtkAnnotationLink.h"
#include "vtkSelection.h"
#include "vtkSelectionNode.h"

#include "vtkObjectFactory.h"

#include <vector>
#include <cassert>

using std::vector;

vtkStandardNewMacro(vtkInteractiveChartXYZ)

vtkInteractiveChartXYZ::vtkInteractiveChartXYZ()
{
  this->Translation->Identity();
  this->Translation->PostMultiply();
  this->Scale->Identity();
  this->Scale->PostMultiply();
  this->Interactive = true;
  this->InitialRender = true;
  this->NumberOfComponents = 0;
  this->InitializeAxesBoundaryPoints();
}

vtkInteractiveChartXYZ::~vtkInteractiveChartXYZ()
{
}

void vtkInteractiveChartXYZ::PrintSelf(ostream &os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
}

void vtkInteractiveChartXYZ::Update()
{
  if (this->Link)
    {
    // Copy the row numbers so that we can do the highlight...
    if (!this->points.empty())
      {
      vtkSelection *selection =
          vtkSelection::SafeDownCast(this->Link->GetOutputDataObject(2));
      if (selection->GetNumberOfNodes())
        {
        vtkSelectionNode *node = selection->GetNode(0);
        vtkIdTypeArray *idArray =
            vtkIdTypeArray::SafeDownCast(node->GetSelectionList());
        if (this->selectedPointsBuidTime > idArray->GetMTime() ||
            this->GetMTime() > this->selectedPointsBuidTime)
          {
          this->selectedPoints.resize(idArray->GetNumberOfTuples());
          for (vtkIdType i = 0; i < idArray->GetNumberOfTuples(); ++i)
            {
            this->selectedPoints[i] = this->points[idArray->GetValue(i)];
            }
          this->selectedPointsBuidTime.Modified();
          }
        }
      }
    }
}

bool vtkInteractiveChartXYZ::Paint(vtkContext2D *painter)
{
  if (!this->Visible || this->points.size() == 0)
    return false;

  // Get the 3D context.
  vtkContext3D *context = painter->GetContext3D();

  if (!context)
    return false;

  this->Update();

  // Check if the scene changed size
  bool resizeHappened = this->CheckForSceneResize();

  // Calculate the transforms required for the current rotation.
  this->CalculateTransforms();

  // Update the points that fall inside our axes
  this->UpdateClippedPoints();
  if (this->clipped_points.size() > 0)
    {
    context->PushMatrix();
    context->AppendTransform(this->ContextTransform.GetPointer());

    // First lets draw the points in 3d.
    context->ApplyPen(this->Pen.GetPointer());
    if (this->NumberOfComponents == 0)
      {
      context->DrawPoints(this->clipped_points[0].GetData(), this->clipped_points.size());
      }
    else
      {
      context->DrawPoints(this->clipped_points[0].GetData(), this->clipped_points.size(),
                          this->ClippedColors->GetPointer(0), this->NumberOfComponents);
      }

    // Now to render the selected points.
    if (!this->selectedPoints.empty())
      {
      context->ApplyPen(this->SelectedPen.GetPointer());
      context->DrawPoints(this->selectedPoints[0].GetData(), this->selectedPoints.size());
      }
    context->PopMatrix();
    }

  // Now to draw the axes - pretty basic for now but could be extended.
  context->PushMatrix();
  context->AppendTransform(this->Box.GetPointer());
  context->ApplyPen(this->AxisPen.GetPointer());

  vtkVector3f box[4];
  box[0] = vtkVector3f(0, 0, 0);
  box[1] = vtkVector3f(0, 1, 0);
  box[2] = vtkVector3f(1, 1, 0);
  box[3] = vtkVector3f(1, 0, 0);
  context->DrawLine(box[0], box[1]);
  context->DrawLine(box[1], box[2]);
  context->DrawLine(box[2], box[3]);
  context->DrawLine(box[3], box[0]);
  for (int i = 0; i < 4; ++i)
    {
    box[i].SetZ(1);
    }
  context->DrawLine(box[0], box[1]);
  context->DrawLine(box[1], box[2]);
  context->DrawLine(box[2], box[3]);
  context->DrawLine(box[3], box[0]);
  context->DrawLine(vtkVector3f(0, 0, 0), vtkVector3f(0, 0, 1));
  context->DrawLine(vtkVector3f(1, 0, 0), vtkVector3f(1, 0, 1));
  context->DrawLine(vtkVector3f(0, 1, 0), vtkVector3f(0, 1, 1));
  context->DrawLine(vtkVector3f(1, 1, 0), vtkVector3f(1, 1, 1));

  // Now draw the axes labels in 2D
  vtkNew<vtkTextProperty> textProperties;
  textProperties->SetJustificationToCentered();
  textProperties->SetVerticalJustificationToCentered();
  textProperties->SetColor(0.0, 0.0, 0.0);
  textProperties->SetFontFamilyToArial();
  textProperties->SetFontSize(14);
  painter->ApplyTextProp(textProperties.GetPointer());

  float bounds[4];

  painter->ComputeStringBounds(this->XAxisLabel, bounds);
  float xLabelPos[4] = { 0.5, 0 - bounds[3], 0, 1};

  painter->ComputeStringBounds(this->YAxisLabel, bounds);
  float yLabelPos[4] = { 0 - bounds[3], 0.5, 0, 1};
  float zLabelPos[4] = { 0, 0, 0.5, 1};
  this->Box->TransformPoint(xLabelPos, xLabelPos);
  this->Box->TransformPoint(yLabelPos, yLabelPos);
  this->Box->TransformPoint(zLabelPos, zLabelPos);

/*
  vtkNew<vtkMatrix4x4> modelview;
  cout << "*** result ***" << endl;
  context->GetDevice()->GetMatrix(modelview.GetPointer());
  modelview->PrintSelf(cout, vtkIndent());
*/

  context->PopMatrix();

  painter->DrawString(xLabelPos[0], xLabelPos[1], this->XAxisLabel);
  painter->DrawString(zLabelPos[0], zLabelPos[1], this->ZAxisLabel);

  textProperties->SetOrientation(90);
  painter->ApplyTextProp(textProperties.GetPointer());
  painter->DrawString(yLabelPos[0], yLabelPos[1], this->YAxisLabel);

  // If necessary, rescale the axes so they fits our scene nicely
  if (resizeHappened)
    {
    this->RescaleAxes();
    }

  return true;
}

void vtkInteractiveChartXYZ::UpdateClippedPoints()
{
  this->clipped_points.clear();
  this->ClippedColors->Reset();
  for( size_t i = 0; i < this->points.size(); ++i )
    {
    const unsigned char rgb[3] =
      {
      this->Colors->GetValue(i * this->NumberOfComponents),
      this->Colors->GetValue(i * this->NumberOfComponents + 1),
      this->Colors->GetValue(i * this->NumberOfComponents + 2)
      };
    if( !this->PointShouldBeClipped(this->points[i]) )
      {
      this->clipped_points.push_back(this->points[i]);
      this->ClippedColors->InsertNextTupleValue(&rgb[0]);
      this->ClippedColors->InsertNextTupleValue(&rgb[1]);
      this->ClippedColors->InsertNextTupleValue(&rgb[2]);
      }
    }
}

void vtkInteractiveChartXYZ::SetInput(vtkTable *input, const vtkStdString &xName,
                           const vtkStdString &yName, const vtkStdString &zName)
{
  this->Superclass::SetInput(input, xName, yName, zName);
  this->XAxisLabel = xName;
  this->YAxisLabel = yName;
  this->ZAxisLabel = zName;
}

void vtkInteractiveChartXYZ::SetScene(vtkContextScene *scene)
{
  this->Superclass::SetScene(scene);
  this->SceneWidth = this->Scene->GetSceneWidth();
  this->SceneHeight = this->Scene->GetSceneHeight();
}

void vtkInteractiveChartXYZ::SetInput(vtkTable *input, const vtkStdString &xName,
                           const vtkStdString &yName, const vtkStdString &zName,
                           const vtkStdString &colorName)
{
  this->Superclass::SetInput(input, xName, yName, zName);
  this->XAxisLabel = xName;
  this->YAxisLabel = yName;
  this->ZAxisLabel = zName;

  vtkDataArray *colorArr =
      vtkDataArray::SafeDownCast(input->GetColumnByName(colorName.c_str()));

  assert(colorArr);
  assert(colorArr->GetNumberOfTuples() == this->points.size());

  this->NumberOfComponents = 3;

  //generate a color lookup table
  vtkNew<vtkLookupTable> lookupTable;
  double min = DBL_MAX;
  double max = DBL_MIN;
  for (int i = 0; i < this->points.size(); ++i)
    {
    double value = colorArr->GetComponent(i, 0);
    if (value > max)
      {
      max = value;
      }
    else if (value < min)
      {
      min = value;
      }
    }

  lookupTable->SetNumberOfTableValues(256);
  lookupTable->SetRange(min, max);
  lookupTable->Build();

  double color[3];

  for (int i = 0; i < this->points.size(); ++i)
    {
    double value = colorArr->GetComponent(i, 0);
    unsigned char *rgb = lookupTable->MapValue(value);
    const unsigned char constRGB[3] = { rgb[0], rgb[1], rgb[2] };
    this->Colors->InsertNextTupleValue(&constRGB[0]);
    this->Colors->InsertNextTupleValue(&constRGB[1]);
    this->Colors->InsertNextTupleValue(&constRGB[2]);
    }
}

//-----------------------------------------------------------------------------
bool vtkInteractiveChartXYZ::Hit(const vtkContextMouseEvent &vtkNotUsed(mouse))
{
  // If we are interactive, we want to catch anything that propagates to the
  // background, otherwise we do not want any mouse events.
  return this->Interactive;
}

//-----------------------------------------------------------------------------
bool vtkInteractiveChartXYZ::MouseButtonPressEvent(const vtkContextMouseEvent &mouse)
{
  if (mouse.GetButton() == vtkContextMouseEvent::LEFT_BUTTON)
    {
    return true;
    }
  return false;
}

//-----------------------------------------------------------------------------
bool vtkInteractiveChartXYZ::MouseMoveEvent(const vtkContextMouseEvent &mouse)
{
  if (mouse.GetButton() == vtkContextMouseEvent::LEFT_BUTTON)
    {
    if (mouse.GetModifiers() == vtkContextMouseEvent::SHIFT_MODIFIER)
      {
      return this->Spin(mouse);
      }
    else
      {
      return this->Rotate(mouse);
      }
    }
  if (mouse.GetButton() == vtkContextMouseEvent::RIGHT_BUTTON)
    {
    if (mouse.GetModifiers() == vtkContextMouseEvent::SHIFT_MODIFIER)
      {
      return this->Pan(mouse);
      }
    else
      {
      return this->Zoom(mouse);
      }
    }
  return false;
}

//-----------------------------------------------------------------------------
bool vtkInteractiveChartXYZ::MouseWheelEvent(const vtkContextMouseEvent &mouse,
                                             int delta)
{
  // Ten "wheels" to double/halve zoom level
  float scaling = pow(2.0f, delta/10.0f);
  this->Scale->Scale(scaling, scaling, scaling);

  // Mark the scene as dirty
  this->Scene->SetDirty(true);

  this->InvokeEvent(vtkCommand::InteractionEvent);
  return true;
}

//-----------------------------------------------------------------------------
void vtkInteractiveChartXYZ::ZoomAxes(int delta)
{
  float scaling = pow(2.0f, delta/10.0f);
  this->BoxScale->Scale(scaling, scaling, scaling);

  // Mark the scene as dirty
  this->Scene->SetDirty(true);
}

//-----------------------------------------------------------------------------
bool vtkInteractiveChartXYZ::Rotate(const vtkContextMouseEvent &mouse)
{
  // Figure out how much the mouse has moved in plot coordinates
  vtkVector2d screenPos(mouse.GetScreenPos().Cast<double>().GetData());
  vtkVector2d lastScreenPos(mouse.GetLastScreenPos().Cast<double>().GetData());

  double dx = screenPos[0] - lastScreenPos[0];
  double dy = screenPos[1] - lastScreenPos[1];

  double delta_elevation = -20.0 / this->Scene->GetSceneHeight();
  double delta_azimuth = -20.0 / this->Scene->GetSceneWidth();

  double rxf = dx * delta_azimuth * 10.0;
  double ryf = dy * delta_elevation * 10.0;

  this->Rotation->RotateY(rxf);
  this->Rotation->RotateX(-ryf);

  // Mark the scene as dirty
  this->Scene->SetDirty(true);

  this->InvokeEvent(vtkCommand::InteractionEvent);
  return true;
}

//-----------------------------------------------------------------------------
bool vtkInteractiveChartXYZ::Pan(const vtkContextMouseEvent &mouse)
{
  // Figure out how much the mouse has moved in plot coordinates
  vtkVector2d screenPos(mouse.GetScreenPos().Cast<double>().GetData());
  vtkVector2d lastScreenPos(mouse.GetLastScreenPos().Cast<double>().GetData());

  double dx = (screenPos[0] - lastScreenPos[0]);
  double dy = (screenPos[1] - lastScreenPos[1]);

  this->Translation->Translate(dx, dy, 0.0);

  // Mark the scene as dirty
  this->Scene->SetDirty(true);

  this->InvokeEvent(vtkCommand::InteractionEvent);
  return true;
}

//-----------------------------------------------------------------------------
bool vtkInteractiveChartXYZ::Zoom(const vtkContextMouseEvent &mouse)
{
  // Figure out how much the mouse has moved and scale accordingly
  vtkVector2d screenPos(mouse.GetScreenPos().Cast<double>().GetData());
  vtkVector2d lastScreenPos(mouse.GetLastScreenPos().Cast<double>().GetData());

  float delta = 0.0f;
  if (this->Scene->GetSceneHeight() > 0)
    {
    delta = static_cast<float>(mouse.GetLastScreenPos()[1] - mouse.GetScreenPos()[1])/this->Scene->GetSceneHeight();
    }

  // Dragging full screen height zooms 4x.
  float scaling = pow(4.0f, delta);
  this->Scale->Scale(scaling, scaling, scaling);

  // Mark the scene as dirty
  this->Scene->SetDirty(true);

  this->InvokeEvent(vtkCommand::InteractionEvent);
  return true;
}

//-----------------------------------------------------------------------------
bool vtkInteractiveChartXYZ::Spin(const vtkContextMouseEvent &mouse)
{
  // Figure out how much the mouse has moved in plot coordinates
  vtkVector2d screenPos(mouse.GetScreenPos().Cast<double>().GetData());
  vtkVector2d lastScreenPos(mouse.GetLastScreenPos().Cast<double>().GetData());

  double newAngle =
    vtkMath::DegreesFromRadians(atan2(screenPos[1], screenPos[0]));
  double oldAngle =
    vtkMath::DegreesFromRadians(atan2(lastScreenPos[1], lastScreenPos[0]));

  this->Rotation->RotateZ(-(newAngle - oldAngle));

  // Mark the scene as dirty
  this->Scene->SetDirty(true);

  this->InvokeEvent(vtkCommand::InteractionEvent);
  return true;
}

//-----------------------------------------------------------------------------
bool vtkInteractiveChartXYZ::KeyPressEvent(const vtkContextKeyEvent &key)
{
  switch (key.GetKeyCode())
    {
    // Change view to 2D, YZ chart
    case 'x':
      this->LookDownX();
      break;
    case 'X':
      this->LookUpX();
      break;
    // Change view to 2D, XZ chart
    case 'y':
      this->LookDownY();
      break;
    case 'Y':
      this->LookUpY();
      break;
    // Change view to 2D, XY chart
    case 'z':
      this->LookDownZ();
      break;
    case 'Z':
      this->LookUpZ();
      break;
    }

  return true;
}

//-----------------------------------------------------------------------------
void vtkInteractiveChartXYZ::LookDownX()
{
  this->InvokeEvent(vtkCommand::InteractionEvent);
  this->Rotation->Identity();
  this->Rotation->RotateY(90.0);
  this->Scene->SetDirty(true);
}

//-----------------------------------------------------------------------------
void vtkInteractiveChartXYZ::LookDownY()
{
  this->Rotation->Identity();
  this->Rotation->RotateX(90.0);
  this->InvokeEvent(vtkCommand::InteractionEvent);
  this->Scene->SetDirty(true);
}

//-----------------------------------------------------------------------------
void vtkInteractiveChartXYZ::LookDownZ()
{
  this->Rotation->Identity();
  this->InvokeEvent(vtkCommand::InteractionEvent);
  this->Scene->SetDirty(true);
}

//-----------------------------------------------------------------------------
void vtkInteractiveChartXYZ::LookUpX()
{
  this->InvokeEvent(vtkCommand::InteractionEvent);
  this->Rotation->Identity();
  this->Rotation->RotateY(-90.0);
  this->Scene->SetDirty(true);
}

//-----------------------------------------------------------------------------
void vtkInteractiveChartXYZ::LookUpY()
{
  this->Rotation->Identity();
  this->Rotation->RotateX(-90.0);
  this->InvokeEvent(vtkCommand::InteractionEvent);
  this->Scene->SetDirty(true);
}

//-----------------------------------------------------------------------------
void vtkInteractiveChartXYZ::LookUpZ()
{
  this->Rotation->Identity();
  this->Rotation->RotateZ(180.0);
  this->InvokeEvent(vtkCommand::InteractionEvent);
  this->Scene->SetDirty(true);
}

void vtkInteractiveChartXYZ::CalculateTransforms()
{
  // Calculate the correct translation vector so that rotation and scale
  // are applied about the middle of the axes box.
  vtkVector3f translation(
        (axes[0]->GetPosition2()[0] - axes[0]->GetPosition1()[0]) / 2.0
        + axes[0]->GetPosition1()[0],
        (axes[1]->GetPosition2()[1] - axes[1]->GetPosition1()[1]) / 2.0
        + axes[1]->GetPosition1()[1],
        (axes[2]->GetPosition2()[1] - axes[2]->GetPosition1()[1]) / 2.0
        + axes[2]->GetPosition1()[1]);
  vtkVector3f mtranslation = -1.0 * translation;

  this->ContextTransform->Identity();
  this->ContextTransform->Concatenate(this->Translation.GetPointer());
  this->ContextTransform->Translate(translation.GetData());
  this->ContextTransform->Concatenate(this->Rotation.GetPointer());
  this->ContextTransform->Concatenate(this->BoxScale.GetPointer());
  this->ContextTransform->Concatenate(this->Scale.GetPointer());
  this->ContextTransform->Translate(mtranslation.GetData());
  this->ContextTransform->Translate(axes[0]->GetPosition1()[0] - this->Geometry.X(),
                                    axes[1]->GetPosition1()[1] - this->Geometry.Y(),
                                    axes[2]->GetPosition1()[1]);
  this->ContextTransform->Concatenate(this->Transform.GetPointer());

  // Next construct the transform for the box axes.
  double scale[3] = { 300, 300, 300 };
  for (int i = 0; i < 3; ++i)
    {
    if (i == 0)
      scale[i] = axes[i]->GetPosition2()[0] - axes[i]->GetPosition1()[0];
    else
      scale[i] = axes[i]->GetPosition2()[1] - axes[i]->GetPosition1()[1];
    }

  this->Box->Identity();
  this->Box->PostMultiply();
  this->Box->Translate(-0.5, -0.5, -0.5);
  this->Box->Concatenate(this->Rotation.GetPointer());
  this->Box->Concatenate(this->BoxScale.GetPointer());
  this->Box->Translate(0.5, 0.5, 0.5);
  this->Box->Scale(scale);
  this->Box->Translate(axes[0]->GetPosition1()[0],
                       axes[1]->GetPosition1()[1],
                       axes[2]->GetPosition1()[1]);

  // setup clipping planes
  vtkVector3d cube[8];
  vtkVector3d transformedCube[8];

  cube[0] = vtkVector3d(0, 0, 0);
  cube[1] = vtkVector3d(0, 0, 1);
  cube[2] = vtkVector3d(0, 1, 0);
  cube[3] = vtkVector3d(0, 1, 1);
  cube[4] = vtkVector3d(1, 0, 0);
  cube[5] = vtkVector3d(1, 0, 1);
  cube[6] = vtkVector3d(1, 1, 0);
  cube[7] = vtkVector3d(1, 1, 1);

  for (int i = 0; i < 8; ++i)
    {
    this->Box->TransformPoint(cube[i].GetData(), transformedCube[i].GetData());
    }

  double norm1[3];
  double norm2[3];
  double norm3[3];
  double norm4[3];
  double norm5[3];
  double norm6[3];

  //face 0,1,2,3 opposes face 4,5,6,7
  vtkMath::Cross((transformedCube[1] - transformedCube[0]).GetData(),
                 (transformedCube[2] - transformedCube[0]).GetData(), norm1);
  this->Face1->SetNormal(norm1);
  this->Face1->SetOrigin(transformedCube[3].GetData());

  vtkMath::Cross((transformedCube[5] - transformedCube[4]).GetData(),
                 (transformedCube[6] - transformedCube[4]).GetData(), norm2);
  this->Face2->SetNormal(norm2);
  this->Face2->SetOrigin(transformedCube[7].GetData());

  //face 0,1,4,5 opposes face 2,3,6,7
  vtkMath::Cross((transformedCube[1] - transformedCube[0]).GetData(),
                 (transformedCube[4] - transformedCube[0]).GetData(), norm3);
  this->Face3->SetNormal(norm3);
  this->Face3->SetOrigin(transformedCube[5].GetData());

  vtkMath::Cross((transformedCube[3] - transformedCube[2]).GetData(),
                 (transformedCube[6] - transformedCube[2]).GetData(), norm4);
  this->Face4->SetNormal(norm4);
  this->Face4->SetOrigin(transformedCube[7].GetData());

  //face 0,2,4,6 opposes face 1,3,5,7
  vtkMath::Cross((transformedCube[2] - transformedCube[0]).GetData(),
                 (transformedCube[4] - transformedCube[0]).GetData(), norm5);
  this->Face5->SetNormal(norm5);
  this->Face5->SetOrigin(transformedCube[6].GetData());

  vtkMath::Cross((transformedCube[3] - transformedCube[1]).GetData(),
                 (transformedCube[5] - transformedCube[1]).GetData(), norm6);
  this->Face6->SetNormal(norm6);
  this->Face6->SetOrigin(transformedCube[7].GetData());

  this->MaxDistance = this->Face1->DistanceToPlane(transformedCube[7].GetData());
}

bool vtkInteractiveChartXYZ::PointShouldBeClipped(vtkVector3f point)
{
  double pointD[3];
  pointD[0] = point.GetData()[0];
  pointD[1] = point.GetData()[1];
  pointD[2] = point.GetData()[2];

  double transformedPoint[3];
  this->ContextTransform->TransformPoint(pointD, transformedPoint);

  double d1 = this->Face1->DistanceToPlane(transformedPoint);
  double d2 = this->Face2->DistanceToPlane(transformedPoint);
  double d3 = this->Face3->DistanceToPlane(transformedPoint);
  double d4 = this->Face4->DistanceToPlane(transformedPoint);
  double d5 = this->Face5->DistanceToPlane(transformedPoint);
  double d6 = this->Face6->DistanceToPlane(transformedPoint);

  if (d1 > this->MaxDistance || d2 > this->MaxDistance ||
      d3 > this->MaxDistance || d4 > this->MaxDistance ||
      d5 > this->MaxDistance || d6 > this->MaxDistance)
    {
    return true;
    }
  return false;
}

void vtkInteractiveChartXYZ::ScaleUpAxes()
{
  float point[3];
  int sceneWidth = this->Scene->GetSceneWidth();
  int sceneHeight = this->Scene->GetSceneHeight();
  float scaleStep = pow(2.0f, 1.0f/10.0f);
  float stepBack = pow(2.0f, -1.0f/10.0f);
  int numSteps = 0;
  bool shouldScaleUp = true;

  while (shouldScaleUp)
    {
    for (int i = 0; i < 14; ++i)
      {
      point[0] = this->AxesBoundaryPoints[i][0];
      point[1] = this->AxesBoundaryPoints[i][1];
      point[2] = this->AxesBoundaryPoints[i][2];
      this->FutureBox->TransformPoint(point, point);
      if (point[0] < 0 || point[0] > sceneWidth ||
          point[1] < 0 || point[1] > sceneHeight)
        {
        shouldScaleUp = false;
        }
      }
    if (shouldScaleUp)
      {
      this->FutureBoxScale->Scale(scaleStep, scaleStep, scaleStep);
      ++numSteps;
      }
    }
  // this while loop overshoots the mark by one step,
  // so we take a step back afterwards.
  this->FutureBoxScale->Scale(stepBack, stepBack, stepBack);

  if (numSteps > 1)
    {
      this->ZoomAxes(numSteps - 1);
      this->Scene->SetDirty(true);
    }
}

void vtkInteractiveChartXYZ::ScaleDownAxes()
{
  float point[3];
  int sceneWidth = this->Scene->GetSceneWidth();
  int sceneHeight = this->Scene->GetSceneHeight();

  float scaleStep = pow(2.0f, -1.0f/10.0f);
  int numSteps = 0;
  bool shouldScaleDown = true;

  while (shouldScaleDown)
    {
    shouldScaleDown = false;
    for (int i = 0; i < 14; ++i)
      {
      point[0] = this->AxesBoundaryPoints[i][0];
      point[1] = this->AxesBoundaryPoints[i][1];
      point[2] = this->AxesBoundaryPoints[i][2];
      this->FutureBox->TransformPoint(point, point);
      if (point[0] < 0 || point[0] > sceneWidth ||
          point[1] < 0 || point[1] > sceneHeight)
        {
        shouldScaleDown = true;
        break;
        }
      }
    if (shouldScaleDown)
      {
      this->FutureBoxScale->Scale(scaleStep, scaleStep, scaleStep);
      ++numSteps;
      }
    }
  if (numSteps > 0)
    {
      this->ZoomAxes(-numSteps);
      this->Scene->SetDirty(true);
    }
}

void vtkInteractiveChartXYZ::InitializeFutureBox()
{
  double scale[3] = { 300, 300, 300 };
  for (int i = 0; i < 3; ++i)
    {
    if (i == 0)
      scale[i] = axes[i]->GetPosition2()[0] - axes[i]->GetPosition1()[0];
    else
      scale[i] = axes[i]->GetPosition2()[1] - axes[i]->GetPosition1()[1];
    }

  this->FutureBoxScale->DeepCopy(this->BoxScale.GetPointer());

  this->FutureBox->Identity();
  this->FutureBox->PostMultiply();
  this->FutureBox->Translate(-0.5, -0.5, -0.5);
  this->FutureBox->Concatenate(this->Rotation.GetPointer());
  this->FutureBox->Concatenate(this->FutureBoxScale.GetPointer());
  this->FutureBox->Translate(0.5, 0.5, 0.5);
  this->FutureBox->Scale(scale);
  this->FutureBox->Translate(axes[0]->GetPosition1()[0],
                             axes[1]->GetPosition1()[1],
                             axes[2]->GetPosition1()[1]);
}

bool vtkInteractiveChartXYZ::CheckForSceneResize()
{
  int currentWidth = this->Scene->GetSceneWidth();
  int currentHeight = this->Scene->GetSceneHeight();
  if (this->SceneWidth != currentWidth ||
      this->SceneHeight != currentHeight)
    {
    // treat the initial render as a special case, as the scene size
    // has not been recorded yet
    if (this->SceneWidth > 0)
      {
      int dx = (currentWidth - this->SceneWidth) / 2;
      int dy = (currentHeight - this->SceneHeight) / 2;

      vtkVector2f axisPt = axes[0]->GetPosition1();
      axisPt[0] += dx;
      axisPt[1] += dy;
      axes[0]->SetPoint1(axisPt);
      axisPt = axes[0]->GetPosition2();
      axisPt[0] += dx;
      axisPt[1] += dy;
      axes[0]->SetPoint2(axisPt);
      axisPt = axes[1]->GetPosition1();
      axisPt[0] += dx;
      axisPt[1] += dy;
      axes[1]->SetPoint1(axisPt);
      axisPt = axes[1]->GetPosition2();
      axisPt[0] += dx;
      axisPt[1] += dy;
      axes[1]->SetPoint2(axisPt);
      axisPt = axes[2]->GetPosition1();
      axisPt[0] += dx;
      axes[2]->SetPoint1(axisPt);
      axisPt = axes[2]->GetPosition2();
      axisPt[0] += dx;
      axes[2]->SetPoint2(axisPt);
      this->RecalculateTransform();
      }
    else
      {
      this->SceneWidth = currentWidth;
      this->SceneHeight = currentHeight;
      this->InitializeFutureBox();
      this->ScaleUpAxes();
      this->ScaleDownAxes();
      }
    return true;
    }
  return false;
}

void vtkInteractiveChartXYZ::RescaleAxes()
{
  int currentWidth = this->Scene->GetSceneWidth();
  int currentHeight = this->Scene->GetSceneHeight();
  this->InitializeFutureBox();
  if (currentWidth * currentHeight < this->SceneWidth * this->SceneHeight)
    {
    this->ScaleDownAxes();
    }
  else
    {
    this->ScaleUpAxes();
    }
  this->SceneWidth = currentWidth;
  this->SceneHeight = currentHeight;
}

void vtkInteractiveChartXYZ::InitializeAxesBoundaryPoints()
{
  int currentPoint = 0;
  for (int i = 0; i < 2; ++i)
    {
    for (int j = 0; j < 2; ++j)
      {
      for (int k = 0; k < 2; ++k)
        {
        this->AxesBoundaryPoints[currentPoint][0] = i;
        this->AxesBoundaryPoints[currentPoint][1] = j;
        this->AxesBoundaryPoints[currentPoint][2] = k;
        ++currentPoint;
        }
      }
    }

  for (int i = 0; i < 3; ++i)
    {
    this->AxesBoundaryPoints[currentPoint][0] = 0.5;
    this->AxesBoundaryPoints[currentPoint][1] = 0.5;
    this->AxesBoundaryPoints[currentPoint][2] = 0.5;
    this->AxesBoundaryPoints[currentPoint][i] += sqrt(0.75);
    ++currentPoint;
    this->AxesBoundaryPoints[currentPoint][0] = 0.5;
    this->AxesBoundaryPoints[currentPoint][1] = 0.5;
    this->AxesBoundaryPoints[currentPoint][2] = 0.5;
    this->AxesBoundaryPoints[currentPoint][i] -= sqrt(0.75);
    ++currentPoint;
    }
}
