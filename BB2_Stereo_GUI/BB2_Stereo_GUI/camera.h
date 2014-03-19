#ifndef CAMERA_H__
#define CAMERA_H__

#include "triclops.h"
#include "PGRFlyCapture.h"
#include "PGRFlyCapturePlus.h"
#include "PGRFlyCaptureStereo.h"


//void initCamera();
//void destroyCamera();

//void grabImage(TriclopsImage &trDispImage, TriclopsImage &trRefImage, FlyCaptureImage &fImage, FlyCaptureImage &fColorImage);
//void grabColorAndStereo(FlyCaptureImage	   &flycaptureImage, TriclopsImage16 &depthImage16, TriclopsImage &monoImage, TriclopsColorImage  &colorImageRight, TriclopsColorImage  &colorImageLeft);

void expandGrayscale(TriclopsImage image, unsigned char* &expandedData);
void expandColor(TriclopsColorImage &image, unsigned char* &expandedData);
void expandDepth(TriclopsImage16  &image16, unsigned char* &expandedData);


#endif CAMERA_H