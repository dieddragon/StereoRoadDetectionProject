#include "GUI.h"


//
// Helper code to handle a FlyCapture error.
//
#define _HANDLE_FLYCAPTURE_ERROR( error, function ) \
   if( error != FLYCAPTURE_OK ) \
   { \
		std::cout << function << flycaptureErrorToString( error ) << std::endl; \
   } \

//
// Macro to check, report on, and handle Triclops API error codes.
//
#define _HANDLE_TRICLOPS_ERROR( error, description )	\
{ \
   if( error != TriclopsErrorOk ) \
   { \
   std::cout << "Triclops Error " << triclopsErrorToString( error ) << " at line " << __LINE__ << description << std::endl; \
   } \
} \


//Constructor
Bumblebee2::Bumblebee2() :
widthImage(320),
heightImage(240),
minDisparity(0),
maxDisparity(150),
maskSizeStereo(15),
indexCamera(0),
videoMode(FLYCAPTURE_VIDEOMODE_ANY),
frameRate(FLYCAPTURE_FRAMERATE_ANY),
iMaxCols(0),
iMaxRows(0),
surfaceValidationSize(20),
isTextureValidation(false),
isSurfaceValidation(false),
isBackForthValidation(true),
isSubpixelInterpolation(true),
textureValidationThreshold(20.0),
surfaceValidationDifference(0.0)
{
   //int len = sizeof(unsigned char)*widthImage*heightImage;
   //strBlue = (unsigned char*)malloc(len);
   //strGreen = (unsigned char*)malloc(len);
   //strRed = (unsigned char*)malloc(len);
}

//Destructor
Bumblebee2::~Bumblebee2() { }


//getters
int Bumblebee2::getWidthImage()
{
	return widthImage;
}

int Bumblebee2::getHeightImage()
{
	return heightImage;
}

int Bumblebee2::getMinDisparity()
{
	return minDisparity;
}

int Bumblebee2::getMaxDisparity()
{
	return maxDisparity;
}

int Bumblebee2::getMaskSizeStereo()
{
	return maskSizeStereo;
}

int Bumblebee2::getIndexCamera()
{
	return indexCamera;
}

int Bumblebee2::getVideoMode()
{
	return videoMode;
}

int Bumblebee2::getFrameRate()
{
	return frameRate;
}

int Bumblebee2::getSurfaceValidationSize()
{
	return surfaceValidationSize;
}

bool Bumblebee2::getIsTextureValidation()
{
	return isTextureValidation;
}

bool Bumblebee2::getIsSurfaceValidation()
{
	return isSurfaceValidation;
}

bool Bumblebee2::getIsBackForthValidation()
{
	return isBackForthValidation;
}

bool Bumblebee2::getIsSubpixelInterpolation()
{
	return isSubpixelInterpolation;
}

float Bumblebee2::getTextureValidationThreshold()
{
	return textureValidationThreshold;
}

float Bumblebee2::getSurfaceValidationDifference()
{
	return surfaceValidationDifference;
}

FlyCaptureImage Bumblebee2::getFlyCaptureImage()
{
	return flycaptureImage;
}

TriclopsImage16 Bumblebee2::getDepthImage16()
{
	return depthImage16;
}

TriclopsImage Bumblebee2::getMonoImage()
{
	return monoImage;
}

TriclopsColorImage Bumblebee2::getColorImageLeft()
{
	return colorImageLeft;
}

TriclopsColorImage Bumblebee2::getColorImageRight()
{
	return colorImageRight;
}

//setters
void Bumblebee2::setWidthImage(int width)
{
	widthImage = width;
}

void Bumblebee2::setHeightImage(int height)
{
	heightImage = height;
}

void Bumblebee2::setMinDisparity(int minDisp)
{
	if (minDisp >= 0)
		minDisparity = minDisp;
	else
		minDisparity = 0;
}

void Bumblebee2::setMaxDisparity(int maxDisp)
{
	if (maxDisp >= 0)
		maxDisparity = maxDisp;
	else
		maxDisparity = 128;
}

void Bumblebee2::setMaskSizeStereo(int maskSize)
{
	if (maskSize >= 1)
		maskSizeStereo = maskSize;
	else
		maskSizeStereo = 1;
}

void Bumblebee2::setIndexCamera(int index)
{
	if (index >= 0)
		indexCamera = index;
	else
		indexCamera = 0;
}

void Bumblebee2::setVideoMode(int mode)
{
	if (mode >= 0)
		videoMode = mode;
	else
		videoMode = FLYCAPTURE_VIDEOMODE_ANY;
}

void Bumblebee2::setFrameRate(int rate)
{
	if (rate >= 0)
		frameRate = rate;
	else
		frameRate = FLYCAPTURE_FRAMERATE_ANY;
}

void Bumblebee2::setSurfaceValidationSize(int size)
{
	if (size >= 0)
		surfaceValidationSize = size;
	else
		surfaceValidationSize = 0;
}

void Bumblebee2::setIsTextureValidation(bool b)
{
	isTextureValidation = b;
}

void Bumblebee2::setIsSurfaceValidation(bool b)
{
	isSurfaceValidation = b;
}

void Bumblebee2::setIsBackForthValidation(bool b)
{
	isBackForthValidation = b;
}

void Bumblebee2::setIsSubpixelInterpolation(bool b)
{
	isSubpixelInterpolation = b;
}

void Bumblebee2::setTextureValidationThreshold(float threshold)
{
	if (threshold >= 0.0) textureValidationThreshold = threshold;
	else textureValidationThreshold = 0.0;
}

void Bumblebee2::setSurfaceValidationDifference(float difference)
{
	if (difference >= 0.0) surfaceValidationDifference = difference;
	else surfaceValidationDifference = 0.0;
}

//Methods
void Bumblebee2::initCamera()
{
	//Get the default flycapture context
	std::cout << "Creating context for determining buffer size.\n" << std::endl;
    fe = flycaptureCreateContext( &flycapture );
	_HANDLE_FLYCAPTURE_ERROR( fe, "flycaptureCreateContext()" );

	//Initialize the camera
	std::cout << "Initializing camera " << indexCamera << std::endl;
	fe = flycaptureInitialize( flycapture, indexCamera );
	_HANDLE_FLYCAPTURE_ERROR( fe, "flycaptureInitialize()" );

	
	// Save the camera's calibration file, and return the path 
   fe = flycaptureGetCalibrationFileFromCamera( flycapture, &szCalFile );
   _HANDLE_FLYCAPTURE_ERROR( fe, "flycaptureGetCalibrationFileFromCamera()" );
   
   // Create a Triclops context from the cameras calibration file
   te = triclopsGetDefaultContextFromFile( &triclops, szCalFile );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsGetDefaultContextFromFile()" );
	
	// Get camera information
   fe = flycaptureGetCameraInfo( flycapture, &pInfo );
   _HANDLE_FLYCAPTURE_ERROR( fe, "flycatpureGetCameraInfo()" );  
   
   if (pInfo.CameraType == FLYCAPTURE_COLOR)
   {
      pixelFormat = FLYCAPTURE_RAW16;
   } 
   else 
   {
      pixelFormat = FLYCAPTURE_MONO16;
   }


	unsigned long ulValue;
	flycaptureGetCameraRegister( flycapture, 0x1F28, &ulValue );
	 
	if ( ( ulValue & 0x2 ) == 0 )
	{
	   // Hi-res BB2
	   iMaxCols = 1024; 
	   iMaxRows = 768;   
	}
	else
	{
	   // Low-res BB2
	   iMaxCols = 640;
	   iMaxRows = 480;
	}

    // Start transferring images from the camera to the computer
   fe = flycaptureStartCustomImage( 
      flycapture, 3, 0, 0, iMaxCols, iMaxRows, 100, pixelFormat);
   _HANDLE_FLYCAPTURE_ERROR( fe, "flycaptureStart()" );

   setStereoParams();
}

void Bumblebee2::destroyCamera()
{
	// Close the camera
   fe = flycaptureStop( flycapture );
   _HANDLE_FLYCAPTURE_ERROR( fe, "flycaptureStop()" );
   
   //Destroy the Flycapture context
   fe = flycaptureDestroyContext( flycapture );
   _HANDLE_FLYCAPTURE_ERROR( fe, "flycaptureDestroyContext()" );
   
   // Destroy the Triclops context
   te = triclopsDestroyContext( triclops ) ;
   _HANDLE_TRICLOPS_ERROR( te, "triclopsDestroyContext()" );
}

void Bumblebee2::setStereoParams()
{
   // Set up some stereo parameters:
   // Set to widthxheight output images
   te = triclopsSetResolution( triclops, heightImage, widthImage );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetResolution()");
   
   // Set disparity range
   te = triclopsSetDisparity( triclops, minDisparity, maxDisparity );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetDisparity()" );   
   
   // Lets turn off all validation except subpixel and surface
   // This works quite well
   te = triclopsSetTextureValidation( triclops, isTextureValidation );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetTextureValidation()" );
   te = triclopsSetTextureValidationThreshold( triclops, textureValidationThreshold );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetTextureValidationThreshold()" );
   te = triclopsSetBackForthValidation( triclops, isBackForthValidation );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetBackForthValidation()" );
   te = triclopsSetSurfaceValidation( triclops, isSurfaceValidation);
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetSurfaceValidation()" );
   te = triclopsSetSurfaceValidationSize( triclops, surfaceValidationSize );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetSurfaceValidationSize()" );
   te = triclopsSetSurfaceValidationDifference( triclops, surfaceValidationDifference  );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetSurfaceValidationDifference()" );
   te = triclopsSetStereoMask( triclops, maskSizeStereo );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetStereoMask()" );
   te = triclopsSetSubpixelInterpolation( triclops, isSubpixelInterpolation );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetSubpixelInterpolation()" );
}

void Bumblebee2::releaseBuffers()
{
	free(strBlue);
	free(strGreen);
	free(strRed);
}

void Bumblebee2::allocBuffers()
{
   int len = sizeof(unsigned char)*widthImage*heightImage;
   strBlue = (unsigned char*)malloc(len);
   strGreen = (unsigned char*)malloc(len);
   strRed = (unsigned char*)malloc(len);
}


void Bumblebee2::saveData(std::string szFileName)
{
	std::string pointFileDelimiter = ".pts";
	std::string imageGrayDelimiter = ".pgm";
	std::string imageColorDelimiter = ".ppm";
	std::string szPointFileName = szFileName + pointFileDelimiter;
	std::string szDepthFileName = szFileName + imageGrayDelimiter;
	std::string szColorLeftFileName = szFileName + "_Left_" + imageColorDelimiter;
	std::string szColorRightFileName = szFileName + "_Right_" + imageColorDelimiter;

   float	       x, y, z; 
   //int		       rl, gl, bl;
   int			   rr, gr, br;
   FILE*	       pointFile;
   int		       nPoints = 0;
   int		       pixelinc ;
   int		       i, j, k;
   unsigned short*     row;
   unsigned short      disparity;

   // Save points to disk
   pointFile = fopen( szPointFileName.c_str(), "w+" );

   // The format for the output file is:
   // <x> <y> <z> <red> <grn> <blu> <row> <col>
   // <x> <y> <z> <red> <grn> <blu> <row> <col>
   // ...

   
   // Determine the number of pixels spacing per row
   pixelinc = depthImage16.rowinc/2;
   for ( i = 0, k = 0; i < depthImage16.nrows; i++ )
   {
      row     = depthImage16.data + i * pixelinc;
      for ( j = 0; j < depthImage16.ncols; j++, k++ )
      {
			disparity = row[j];
	 
			// do not save invalid points
			if ( disparity < 0xFF00 )
			{
				// convert the 16 bit disparity value to floating point x,y,z
				triclopsRCD16ToXYZ( triclops, i, j, disparity, &x, &y, &z );
	    
				// look at points within a range
				if ( z < 25.0 )
				{
					if ( pixelFormat == FLYCAPTURE_RAW16 )
					{
						rr = (int)colorImageRight.red[k];
						gr = (int)colorImageRight.green[k];
						br = (int)colorImageRight.blue[k];		  
					}
					else
					{
					// For mono cameras, we just assign the same value to RGB
						rr = (int)monoImage.data[k];
						gr = (int)monoImage.data[k];
						br = (int)monoImage.data[k];
					}

					fprintf( pointFile, "%f %f %f %d %d %d %d %d\n", x, y, z, rr, gr, br, i, j );
					nPoints++;
				}
			}
	 }
  }

   fclose( pointFile );
   std::cout << "Points in file: " << nPoints << std::endl;

   //Saving images
   te = triclopsSaveImage16( &depthImage16, (char*)(szDepthFileName.c_str()) );
    _HANDLE_TRICLOPS_ERROR( te, "triclopsSaveImage16()" );
	te = triclopsSaveColorImage( &colorImageLeft, (char*)(szColorLeftFileName.c_str()) );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSaveImageLeft()" );
   te = triclopsSaveColorImage( &colorImageRight, (char*)(szColorRightFileName.c_str()) );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSaveImageRight()" );
}

void Bumblebee2::RCDToXYZ(int r, int c, float &x, float &y, float &z)
{
	if (r >= 0 && r < heightImage && c >= 0 && c < widthImage )
	{
		int		       pixelinc ;
		unsigned short*     row;
		unsigned short      disparity;

		pixelinc = depthImage16.rowinc/2;
		row     = depthImage16.data + r * pixelinc;
		disparity = row[c];
		triclopsRCD16ToXYZ( triclops, r, c, disparity, &x, &y, &z );
	}
	else
	{
		x = -1;
		y = -1;
		z = -1;
	}

}


void Bumblebee2::grabColorAndStereo()
{
	TriclopsColorImage dummyRightImage = {0};


	// Grab an image from the camera
   fe = flycaptureGrabImage2( flycapture, &flycaptureImage );
   _HANDLE_FLYCAPTURE_ERROR( fe, "flycaptureGrabImage()" );

   // Extract information from the FlycaptureImage
   int imageCols = flycaptureImage.iCols;
   int imageRows = flycaptureImage.iRows;
   int imageRowInc = flycaptureImage.iRowInc;
   int iSideBySideImages = flycaptureImage.iNumImages;
   unsigned long timeStampSeconds = flycaptureImage.timeStamp.ulSeconds;
   unsigned long timeStampMicroSeconds = flycaptureImage.timeStamp.ulMicroSeconds;

   // Create buffers for holding the color and mono images
   unsigned char* rowIntColor = 
      new unsigned char[ imageCols * imageRows * iSideBySideImages * 4 ];
   unsigned char* rowIntMono = 
      new unsigned char[ imageCols * imageRows * iSideBySideImages ];

   
   // Create a temporary FlyCaptureImage for preparing the stereo image
   FlyCaptureImage tempColorImage;
   FlyCaptureImage tempMonoImage;

   //FlyCaptureImage tempColorImage;
   
   tempColorImage.pData = rowIntColor;
   tempMonoImage.pData = rowIntMono;
   
   // Convert the pixel interleaved raw data to row interleaved format
   fe = flycapturePrepareStereoImage( flycapture, flycaptureImage, &tempMonoImage, &tempColorImage  );
   _HANDLE_FLYCAPTURE_ERROR( fe, "flycapturePrepareStereoImage()" );


   // Pointers to positions in the color buffer that correspond to the beginning
   // of the red, green and blue sections
   unsigned char* redColorRight = NULL;
   unsigned char* greenColorRight = NULL;
   unsigned char* blueColorRight = NULL; 
   unsigned char* redColorLeft = NULL;
   unsigned char* greenColorLeft = NULL;
   unsigned char* blueColorLeft = NULL; 
   
   redColorRight = rowIntColor;
   redColorLeft = rowIntColor + 4 * imageCols;
   greenColorRight = redColorRight + ( 4 * imageCols );
   greenColorLeft = redColorLeft + ( 4 * imageCols );
   blueColorRight = redColorRight + ( 4 * imageCols );
   blueColorLeft = redColorLeft + ( 4 * imageCols );

   

   // Pointers to positions in the mono buffer that correspond to the beginning
   // of the red, green and blue sections
   unsigned char* redMono = NULL;
   unsigned char* greenMono = NULL;
   unsigned char* blueMono = NULL; 

   redMono = rowIntMono;
   greenMono = redMono + imageCols;
   blueMono = redMono + imageCols;


   // Use the row interleaved images to build up a packed TriclopsInput.
   // A packed triclops input will contain a single image with 32 bpp.
   te = triclopsBuildPackedTriclopsInput(
      imageCols,
      imageRows,
      imageRowInc * 4,
      timeStampSeconds,
      timeStampMicroSeconds,
      redColorRight,
      &colorDataRight );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsBuildPackedTriclopsInput()" );

      // Use the row interleaved images to build up a packed TriclopsInput.
   // A packed triclops input will contain a single image with 32 bpp.
   te = triclopsBuildPackedTriclopsInput(
      imageCols,
      imageRows,
      imageRowInc * 4,
      timeStampSeconds,
      timeStampMicroSeconds,
      redColorLeft,
      &colorDataLeft );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsBuildPackedTriclopsInput()" );

   // Use the row interleaved images to build up an RGB TriclopsInput.  
   // An RGB triclops input will contain the 3 raw images (1 from each camera).
   te = triclopsBuildRGBTriclopsInput(
      imageCols, 
      imageRows, 
      imageRowInc,  
      timeStampSeconds, 
      timeStampMicroSeconds, 
      redMono, 
      greenMono, 
      blueMono, 
      &stereoData);
   _HANDLE_TRICLOPS_ERROR( te, "triclopsBuildRGBTriclopsInput()" );

   // Preprocessing the images
   te = triclopsRectify( triclops, &stereoData );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsRectify()" );
   
   // Stereo processing
   te = triclopsStereo( triclops ) ;
   _HANDLE_TRICLOPS_ERROR( te, "triclopsStereo()" );
   
   // Retrieve the interpolated depth image from the context
   te = triclopsGetImage16( triclops, 
			    TriImg16_DISPARITY, 
			    TriCam_REFERENCE, 
			    &depthImage16 );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsGetImage16()" );

   // Rectify the color image if applicable
   if ( pixelFormat == FLYCAPTURE_RAW16 )
   {
      te = triclopsRectifyColorImage( triclops, 
	 TriCam_REFERENCE, 
	 &colorDataRight, 
	 &dummyRightImage );
      _HANDLE_TRICLOPS_ERROR( te, "triclopsRectifyColorImage()" );

	
	//strBlue = (unsigned char*)malloc(len);
	//strGreen = (unsigned char*)malloc(len);
	//strRed = (unsigned char*)malloc(len);
	int len = sizeof(unsigned char)*dummyRightImage.ncols*dummyRightImage.nrows;
	memcpy(strBlue, dummyRightImage.blue, len);
	memcpy(strGreen, dummyRightImage.green, len);
	memcpy(strRed, dummyRightImage.red, len);

	colorImageRight.blue = strBlue;
	colorImageRight.green = strGreen;
	colorImageRight.red = strRed;
	colorImageRight.ncols = dummyRightImage.ncols;
	colorImageRight.nrows = dummyRightImage.nrows;
	colorImageRight.rowinc = dummyRightImage.rowinc;
	  
	  te = triclopsRectifyColorImage( triclops, 
	 TriCam_REFERENCE, 
	 &colorDataLeft, 
	 &colorImageLeft );
      _HANDLE_TRICLOPS_ERROR( te, "triclopsRectifyColorImage()" );
	  
   }
   else
   {
      te = triclopsGetImage( triclops,
	 TriImg_RECTIFIED,
	 TriCam_REFERENCE,
	 &monoImage );
      _HANDLE_TRICLOPS_ERROR( te, "triclopsGetImage()" );
   }

   
      // Delete the image buffer, it is not needed once the TriclopsInput
   // has been built
   delete [] rowIntColor;
   redColorRight = NULL;
   greenColorRight = NULL;
   blueColorRight = NULL;
   redColorLeft = NULL;
   greenColorLeft = NULL;
   blueColorLeft = NULL;

   delete [] rowIntMono;   
   redMono = NULL;
   greenMono = NULL;
   blueMono = NULL;

   /*
   delete [] strBlue;
   delete [] strGreen;
   delete []strRed;
   */

}



void Bumblebee2::grabColorAndStereo(FlyCaptureImage	&flycaptureImage, TriclopsImage16 &depthImage16, TriclopsImage &monoImage, TriclopsColorImage  &colorImageRight, TriclopsColorImage  &colorImageLeft)
{
	TriclopsColorImage dummyRightImage = {0};


	// Grab an image from the camera
   fe = flycaptureGrabImage2( flycapture, &flycaptureImage );
   _HANDLE_FLYCAPTURE_ERROR( fe, "flycaptureGrabImage()" );

   // Extract information from the FlycaptureImage
   int imageCols = flycaptureImage.iCols;
   int imageRows = flycaptureImage.iRows;
   int imageRowInc = flycaptureImage.iRowInc;
   int iSideBySideImages = flycaptureImage.iNumImages;
   unsigned long timeStampSeconds = flycaptureImage.timeStamp.ulSeconds;
   unsigned long timeStampMicroSeconds = flycaptureImage.timeStamp.ulMicroSeconds;

   // Create buffers for holding the color and mono images
   unsigned char* rowIntColor = 
      new unsigned char[ imageCols * imageRows * iSideBySideImages * 4 ];
   unsigned char* rowIntMono = 
      new unsigned char[ imageCols * imageRows * iSideBySideImages ];

   
   // Create a temporary FlyCaptureImage for preparing the stereo image
   FlyCaptureImage tempColorImage;
   FlyCaptureImage tempMonoImage;

   //FlyCaptureImage tempColorImage;
   
   tempColorImage.pData = rowIntColor;
   tempMonoImage.pData = rowIntMono;
   
   // Convert the pixel interleaved raw data to row interleaved format
   fe = flycapturePrepareStereoImage( flycapture, flycaptureImage, &tempMonoImage, &tempColorImage  );
   _HANDLE_FLYCAPTURE_ERROR( fe, "flycapturePrepareStereoImage()" );


   // Pointers to positions in the color buffer that correspond to the beginning
   // of the red, green and blue sections
   unsigned char* redColorRight = NULL;
   unsigned char* greenColorRight = NULL;
   unsigned char* blueColorRight = NULL; 
   unsigned char* redColorLeft = NULL;
   unsigned char* greenColorLeft = NULL;
   unsigned char* blueColorLeft = NULL; 
   
   redColorRight = rowIntColor;
   redColorLeft = rowIntColor + 4 * imageCols;
   greenColorRight = redColorRight + ( 4 * imageCols );
   greenColorLeft = redColorLeft + ( 4 * imageCols );
   blueColorRight = redColorRight + ( 4 * imageCols );
   blueColorLeft = redColorLeft + ( 4 * imageCols );

   

   // Pointers to positions in the mono buffer that correspond to the beginning
   // of the red, green and blue sections
   unsigned char* redMono = NULL;
   unsigned char* greenMono = NULL;
   unsigned char* blueMono = NULL; 

   redMono = rowIntMono;
   greenMono = redMono + imageCols;
   blueMono = redMono + imageCols;


   // Use the row interleaved images to build up a packed TriclopsInput.
   // A packed triclops input will contain a single image with 32 bpp.
   te = triclopsBuildPackedTriclopsInput(
      imageCols,
      imageRows,
      imageRowInc * 4,
      timeStampSeconds,
      timeStampMicroSeconds,
      redColorRight,
      &colorDataRight );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsBuildPackedTriclopsInput()" );

      // Use the row interleaved images to build up a packed TriclopsInput.
   // A packed triclops input will contain a single image with 32 bpp.
   te = triclopsBuildPackedTriclopsInput(
      imageCols,
      imageRows,
      imageRowInc * 4,
      timeStampSeconds,
      timeStampMicroSeconds,
      redColorLeft,
      &colorDataLeft );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsBuildPackedTriclopsInput()" );

   // Use the row interleaved images to build up an RGB TriclopsInput.  
   // An RGB triclops input will contain the 3 raw images (1 from each camera).
   te = triclopsBuildRGBTriclopsInput(
      imageCols, 
      imageRows, 
      imageRowInc,  
      timeStampSeconds, 
      timeStampMicroSeconds, 
      redMono, 
      greenMono, 
      blueMono, 
      &stereoData);
   _HANDLE_TRICLOPS_ERROR( te, "triclopsBuildRGBTriclopsInput()" );

   // Preprocessing the images
   te = triclopsRectify( triclops, &stereoData );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsRectify()" );
   
   // Stereo processing
   te = triclopsStereo( triclops ) ;
   _HANDLE_TRICLOPS_ERROR( te, "triclopsStereo()" );
   
   // Retrieve the interpolated depth image from the context
   te = triclopsGetImage16( triclops, 
			    TriImg16_DISPARITY, 
			    TriCam_REFERENCE, 
			    &depthImage16 );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsGetImage16()" );

   // Rectify the color image if applicable
   if ( pixelFormat == FLYCAPTURE_RAW16 )
   {
      te = triclopsRectifyColorImage( triclops, 
	 TriCam_REFERENCE, 
	 &colorDataRight, 
	 &dummyRightImage );
      _HANDLE_TRICLOPS_ERROR( te, "triclopsRectifyColorImage()" );

	
	//strBlue = (unsigned char*)malloc(len);
	//strGreen = (unsigned char*)malloc(len);
	//strRed = (unsigned char*)malloc(len);
	int len = sizeof(unsigned char)*dummyRightImage.ncols*dummyRightImage.nrows;
	memcpy(strBlue, dummyRightImage.blue, len);
	memcpy(strGreen, dummyRightImage.green, len);
	memcpy(strRed, dummyRightImage.red, len);

	colorImageRight.blue = strBlue;
	colorImageRight.green = strGreen;
	colorImageRight.red = strRed;
	colorImageRight.ncols = dummyRightImage.ncols;
	colorImageRight.nrows = dummyRightImage.nrows;
	colorImageRight.rowinc = dummyRightImage.rowinc;
	  
	  te = triclopsRectifyColorImage( triclops, 
	 TriCam_REFERENCE, 
	 &colorDataLeft, 
	 &colorImageLeft );
      _HANDLE_TRICLOPS_ERROR( te, "triclopsRectifyColorImage()" );
	  
   }
   else
   {
      te = triclopsGetImage( triclops,
	 TriImg_RECTIFIED,
	 TriCam_REFERENCE,
	 &monoImage );
      _HANDLE_TRICLOPS_ERROR( te, "triclopsGetImage()" );
   }

   
      // Delete the image buffer, it is not needed once the TriclopsInput
   // has been built
   delete [] rowIntColor;
   redColorRight = NULL;
   greenColorRight = NULL;
   blueColorRight = NULL;
   redColorLeft = NULL;
   greenColorLeft = NULL;
   blueColorLeft = NULL;

   delete [] rowIntMono;   
   redMono = NULL;
   greenMono = NULL;
   blueMono = NULL;

   /*
   delete [] strBlue;
   delete [] strGreen;
   delete []strRed;
   */

}