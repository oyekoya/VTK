/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkImageMandelbrotSource.cxx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) 1993-2002 Ken Martin, Will Schroeder, Bill Lorensen 
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkImageMandelbrotSource.h"
#include "vtkImageData.h"
#include "vtkObjectFactory.h"

vtkCxxRevisionMacro(vtkImageMandelbrotSource, "1.30");
vtkStandardNewMacro(vtkImageMandelbrotSource);

//----------------------------------------------------------------------------
vtkImageMandelbrotSource::vtkImageMandelbrotSource()
{
  this->MaximumNumberOfIterations = 100;
  this->WholeExtent[0] = 0;
  this->WholeExtent[1] = 250;
  this->WholeExtent[2] = 0;
  this->WholeExtent[3] = 250;
  this->WholeExtent[4] = 0;
  this->WholeExtent[5] = 0;
  
  this->SampleCX[0] = 0.01;
  this->SampleCX[1] = 0.01;
  this->SampleCX[2] = 0.01;
  this->SampleCX[3] = 0.01;
  
  this->OriginCX[0] = -1.75;
  this->OriginCX[1] = -1.25;
  this->OriginCX[2] = 0.0;
  this->OriginCX[3] = 0.0;

  this->ProjectionAxes[0] = 0;
  this->ProjectionAxes[1] = 1;
  this->ProjectionAxes[2] = 2;
}

//----------------------------------------------------------------------------
vtkImageMandelbrotSource::~vtkImageMandelbrotSource()
{
}

//----------------------------------------------------------------------------
void vtkImageMandelbrotSource::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "OriginC: (" << this->OriginCX[0] << ", "
     << this->OriginCX[1] << ")\n";
  os << indent << "OriginX: (" << this->OriginCX[2] << ", "
     << this->OriginCX[3] << ")\n";

  os << indent << "SampleC: (" << this->SampleCX[0] << ", "
     << this->SampleCX[1] << ")\n";
  os << indent << "SampleX: (" << this->SampleCX[2] << ", "
     << this->SampleCX[3] << ")\n";

  os << indent << "WholeExtent: (" << this->WholeExtent[0] << ", "
     << this->WholeExtent[1] << ", " << this->WholeExtent[2] << ", "
     << this->WholeExtent[3] << ", " << this->WholeExtent[4] << ", " 
     << this->WholeExtent[5] << ")\n";
  os << "MaximumNumberOfIterations: " << this->MaximumNumberOfIterations << endl;

  os << indent << "ProjectionAxes: (" << this->ProjectionAxes[0] << ", "
     << this->ProjectionAxes[1] << this->ProjectionAxes[2] << ")\n";

}

//----------------------------------------------------------------------------
/*
void vtkImageMandelbrotSource::SetOriginCX(double cReal, double cImag, 
                                           double xReal, double xImag)
{
  int modified = 0;

  if (cReal != this->OriginCX[0])
    {
    this->OriginCX[0]  = cReal;
    modified = 1;
    }
  if (cImag != this->OriginCX[1])
    {
    this->OriginCX[1]  = cImag;
    modified = 1;
    }
  if (xReal != this->OriginCX[2])
    {
    this->OriginCX[2]  = xReal;
    modified = 1;
    }
  if (xImag != this->OriginCX[3])
    {
    this->OriginCX[3]  = xImag;
    modified = 1;
    }

  if (modified)
    {
    this->Modified();
    }
}
*/
//----------------------------------------------------------------------------
void vtkImageMandelbrotSource::SetWholeExtent(int extent[6])
{
  int idx, modified = 0;
  
  for (idx = 0; idx < 6; ++idx)
    {
    if (this->WholeExtent[idx] != extent[idx])
      {
      this->WholeExtent[idx] = extent[idx];
      this->Modified();
      }
    }
  if (modified)
    {
    this->Modified();
    }
}

//----------------------------------------------------------------------------
void vtkImageMandelbrotSource::SetWholeExtent(int minX, int maxX, 
                                            int minY, int maxY,
                                            int minZ, int maxZ)
{
  int extent[6];
  
  extent[0] = minX;  extent[1] = maxX;
  extent[2] = minY;  extent[3] = maxY;
  extent[4] = minZ;  extent[5] = maxZ;
  this->SetWholeExtent(extent);
}


//----------------------------------------------------------------------------
void vtkImageMandelbrotSource::ExecuteInformation()
{
  int idx, axis;
  float origin[3];
  float spacing[3];
  vtkImageData *output = this->GetOutput();
  
  output->SetWholeExtent(this->WholeExtent);
  for (idx = 0; idx < 3; ++idx)
    {
    axis = this->ProjectionAxes[idx];
    if (axis >= 0 && axis < 4)
      {
      origin[idx] = this->OriginCX[axis];
      spacing[idx] = this->SampleCX[axis];
      }
    else
      {
      vtkErrorMacro("Bad projection axis.");
      origin[idx] = 0.0;
      spacing[idx] = 1.0;
      }
    }

  output->SetSpacing(spacing);
  output->SetOrigin(origin);
  output->SetNumberOfScalarComponents(1);
  output->SetScalarType(VTK_FLOAT);
}

//----------------------------------------------------------------------------
// We may want separate zooms for mandelbrot and julia.
void vtkImageMandelbrotSource::Zoom(double factor)
{
  if (factor == 1.0)
    {
    return;
    }
  this->Modified();
  this->SampleCX[0] *= factor;
  this->SampleCX[1] *= factor;
  this->SampleCX[2] *= factor;
  this->SampleCX[3] *= factor;
}

//----------------------------------------------------------------------------
void vtkImageMandelbrotSource::Pan(double x, double y, double z)
{
  int idx, axis;
  double pan[3];

  if (x == 0.0 && y == 0.0 && z == 0.0)
    {
    return;
    }

  this->Modified();
  pan[0]=x;    pan[1]=y;    pan[2]=z;
  for (idx = 0; idx < 3; ++idx)
    {
    axis = this->ProjectionAxes[idx];
    if (axis >= 0 && axis < 4)
      {
      this->OriginCX[axis] += this->SampleCX[axis] * pan[idx];
      }
    }
}

//----------------------------------------------------------------------------
void 
vtkImageMandelbrotSource::CopyOriginAndSample(vtkImageMandelbrotSource *source)
{
  int idx;

  for (idx = 0; idx < 4; ++idx)
    {
    this->OriginCX[idx] = source->OriginCX[idx];
    this->SampleCX[idx] = source->SampleCX[idx];
    }

  this->Modified();
}
//----------------------------------------------------------------------------
void vtkImageMandelbrotSource::ExecuteData(vtkDataObject *output)
{
  vtkImageData *data = this->AllocateOutputData(output);
  int *ext, a0, a1, a2;
  float *ptr;
  int min0, max0;
  int idx0, idx1, idx2;
  int inc0, inc1, inc2;
  double *origin, *sample;
  double p[4];
  unsigned long count = 0;
  unsigned long target;
  
  // Copy origin into pixel
  for (idx0 = 0; idx0 < 4; ++idx0)
    {
    p[idx0] = this->OriginCX[idx0];
    }

  ext = data->GetUpdateExtent();
  ptr = (float *)(data->GetScalarPointerForExtent(ext));

  vtkDebugMacro("Generating Extent: " << ext[0] << " -> " << ext[1] << ", "
                << ext[2] << " -> " << ext[3]);

  // Get min and max of axis 0 because it is the innermost loop.
  min0 = ext[0];
  max0 = ext[1];
  data->GetContinuousIncrements(ext, inc0, inc1, inc2);

  target = (unsigned long)((ext[5]-ext[4]+1)*(ext[3]-ext[2]+1)/50.0);
  target++;

  a0 = this->ProjectionAxes[0];
  a1 = this->ProjectionAxes[1];
  a2 = this->ProjectionAxes[2];
  origin = this->OriginCX;
  sample = this->SampleCX;

  if (a0<0 || a1<0 || a2<0 || a0>3 || a1>3 || a2>3)
    {
    vtkErrorMacro("Bad projection axis");
    return;
    }
  for (idx2 = ext[4]; idx2 <= ext[5]; ++idx2)
    {
    p[a2] = (double)(origin[a2]) + (double)(idx2)*(sample[a2]);
    for (idx1 = ext[2]; !this->AbortExecute && idx1 <= ext[3]; ++idx1)
      {
      if (!(count%target))
        {
        this->UpdateProgress(count/(50.0*target));
        }
      count++;
      p[a1] = (double)(origin[a1]) + (double)(idx1)*(sample[a1]);
      for (idx0 = min0; idx0 <= max0; ++idx0)
        {
        p[a0] = (double)(origin[a0]) + (double)(idx0)*(sample[a0]);

        *ptr = (float)(this->EvaluateSet(p));

        ++ptr;
        // inc0 is 0
        }
      ptr += inc1;
      }
    ptr += inc2;
    }
  
  // Name the array appropriately.
  data->GetPointData()->GetScalars()->SetName("Iterations");
}

//----------------------------------------------------------------------------
// This method selectively supersamples pixels closet to the mandelbrot set.
/*
void vtkImageMandelbrotSource::SuperSample(vtkImageData *data)
{
  int *ext;
  float *ptr0, *ptr1, *ptr2;
  int min0, max0;
  int idx0, idx1, idx2;
  int inc0, inc1, inc2;

  // Get min and max of axis 0 because it is the innermost loop.
  ext = data->GetUpdateExtent();
  min0 = ext[0];
  max0 = ext[1];
  data->GetIncrements(inc0, inc1, inc2);

  // loop through pixels ignoring the borders.
  ptr2 = (float *)(data->GetScalarPointer(min0+1, ext[2]+1, ext[4]+1));

  for (idx2 = ext[4]+1; idx2 < ext[5]; ++idx2)
    {
    ptr1 = ptr2;
    for (idx1 = ext[2]+1; idx1 < ext[3]; ++idx1)
      {
      ptr0 = ptr1;
      for (idx0 = min0+1; idx0 < max0; ++idx0)
        {
        *ptr0 = (float*)(this->EvaluateSet(p));

        ++ptr0;
        // inc0 is 1
        }
      ptr1 += inc1;
      }
    ptr2 += inc2;
    }
}
*/


//----------------------------------------------------------------------------
float vtkImageMandelbrotSource::EvaluateSet(double p[4])
{
  unsigned short count = 0;
  double v0, v1;
  double cReal, cImag, zReal, zImag;
  double zReal2, zImag2;

  cReal = p[0];
  cImag = p[1];
  zReal = p[2];
  zImag = p[3];

  zReal2 = zReal * zReal;
  zImag2 = zImag * zImag;
  v0 = 0.0;
  v1 = (zReal2 + zImag2);
  while ( v1 < 4.0 && count < this->MaximumNumberOfIterations)
    {
    zImag = 2.0 * zReal * zImag + cImag;
    zReal = zReal2 - zImag2 + cReal;
    zReal2 = zReal * zReal;
    zImag2 = zImag * zImag;
    ++count;
    v0 = v1;
    v1 = (zReal2 + zImag2);
    }

  if (count == this->MaximumNumberOfIterations)
    {
    return (float)count;
    }

  return (float)count + (4.0 - v0)/(v1 - v0);
}







