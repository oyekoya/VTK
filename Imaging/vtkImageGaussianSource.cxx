/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkImageGaussianSource.cxx
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
#include "vtkImageGaussianSource.h"
#include "vtkObjectFactory.h"

#include <math.h>

vtkCxxRevisionMacro(vtkImageGaussianSource, "1.23");
vtkStandardNewMacro(vtkImageGaussianSource);

//----------------------------------------------------------------------------
vtkImageGaussianSource::vtkImageGaussianSource()
{
  this->Maximum = 1.0;
  this->Center[0] = 0.0;
  this->Center[1] = 0.0;
  this->Center[2] = 0.0;

  this->WholeExtent[0] = 0;  this->WholeExtent[1] = 255;
  this->WholeExtent[2] = 0;  this->WholeExtent[3] = 255;
  this->WholeExtent[4] = 0;  this->WholeExtent[5] = 0;
  this->StandardDeviation = 100.0;
}


//----------------------------------------------------------------------------
void vtkImageGaussianSource::SetWholeExtent(int xMin, int xMax, 
                                            int yMin, int yMax,
                                            int zMin, int zMax)
{
  int modified = 0;
  
  if (this->WholeExtent[0] != xMin)
    {
    modified = 1;
    this->WholeExtent[0] = xMin ;
    }
  if (this->WholeExtent[1] != xMax)
    {
    modified = 1;
    this->WholeExtent[1] = xMax ;
    }
  if (this->WholeExtent[2] != yMin)
    {
    modified = 1;
    this->WholeExtent[2] = yMin ;
    }
  if (this->WholeExtent[3] != yMax)
    {
    modified = 1;
    this->WholeExtent[3] = yMax ;
    }
  if (this->WholeExtent[4] != zMin)
    {
    modified = 1;
    this->WholeExtent[4] = zMin ;
    }
  if (this->WholeExtent[5] != zMax)
    {
    modified = 1;
    this->WholeExtent[5] = zMax ;
    }
  if (modified)
    {
    this->Modified();
    }
}

//----------------------------------------------------------------------------
void vtkImageGaussianSource::ExecuteInformation()
{
  vtkImageData *output = this->GetOutput();
  
  output->SetWholeExtent(this->WholeExtent);
  output->SetScalarType(VTK_FLOAT);
  output->SetNumberOfScalarComponents(1);
}

void vtkImageGaussianSource::ExecuteData(vtkDataObject *output)
{
  float *outPtr;
  int idxX, idxY, idxZ;
  int maxX, maxY, maxZ;
  int outIncX, outIncY, outIncZ;
  int *outExt;
  float sum;
  float yContrib, zContrib;
  float temp, temp2;
  unsigned long count = 0;
  unsigned long target;
  
  vtkImageData *data = this->AllocateOutputData(output);

  if (data->GetScalarType() != VTK_FLOAT)
    {
    vtkErrorMacro("Execute: This source only outputs floats");
    }
  
  outExt = data->GetExtent();
  
  // find the region to loop over
  maxX = outExt[1] - outExt[0];
  maxY = outExt[3] - outExt[2]; 
  maxZ = outExt[5] - outExt[4];
  
  // Get increments to march through data 
  data->GetContinuousIncrements(outExt, outIncX, outIncY, outIncZ);
  outPtr = (float *) data->GetScalarPointer(outExt[0],outExt[2],outExt[4]);
  
  target = (unsigned long)((maxZ+1)*(maxY+1)/50.0);
  target++;

  // Loop through ouput pixels
  temp2 = 1.0 / (2.0 * this->StandardDeviation * this->StandardDeviation);

  for (idxZ = 0; idxZ <= maxZ; idxZ++)
    {
    zContrib = this->Center[2] - (idxZ + outExt[4]);
    zContrib = zContrib*zContrib;
    for (idxY = 0; !this->AbortExecute && idxY <= maxY; idxY++)
      {
      if (!(count%target))
        {
        this->UpdateProgress(count/(50.0*target));
        }
      count++;
      yContrib = this->Center[1] - (idxY + outExt[2]);
      yContrib = yContrib*yContrib;
      for (idxX = 0; idxX <= maxX; idxX++)
        {
        // Pixel operation
        sum = zContrib + yContrib;
        temp = this->Center[0] - (idxX + outExt[0]);
        sum = sum + (temp * temp);
        *outPtr = this->Maximum * exp(-sum * temp2);
        outPtr++;
        }
      outPtr += outIncY;
      }
    outPtr += outIncZ;
    }
}

void vtkImageGaussianSource::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  os << indent << "Maximum: " << this->Maximum << "\n";

  os << indent << "StandardDeviation: " << this->StandardDeviation << "\n";

  os << indent << "Center: ( "
     << this->Center[0] << ", "
     << this->Center[1] << ", "
     << this->Center[2] << " )\n";

}


