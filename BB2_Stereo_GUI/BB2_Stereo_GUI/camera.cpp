#include "GUI.h"

/*

//
// Camera bus index to use.  0 = first camera on the bus.
//
#define _CAMERA_INDEX 0

//
// Video mode and frame rate to use.
//
#define _VIDEOMODE   FLYCAPTURE_VIDEOMODE_ANY
#define _FRAMERATE   FLYCAPTURE_FRAMERATE_ANY

//
// Register defines
// 
#define INITIALIZE         0x000
#define CAMERA_POWER       0x610

//Global variables
//Flycapture related
FlyCaptureContext	   flycapture;
//FlyCaptureImage	   flycaptureImage;
FlyCaptureInfoEx	   pInfo;
FlyCapturePixelFormat   pixelFormat;
FlyCaptureError	   fe;
//Triclops related
TriclopsInput       stereoData;
TriclopsInput       colorDataRight;
TriclopsInput		colorDataLeft;
//TriclopsImage16     depthImage16;
//TriclopsImage       monoImage = {0};
//TriclopsColorImage  colorImage = {0};
TriclopsContext     triclops;
TriclopsError       te;
//Name of the calibration file
//char* 		szCalFile 	= "input.cal";
//char* 		szCalFile 	= "next.cal";
char* szCalFile;

//Hi-res BB2
int iMaxCols = 1024;
int iMaxRows = 768;

float	       x, y, z; 
int		       r, g, b;
FILE*	       pointFile;
int		       nPoints = 0;
int		       pixelinc;
int		       i, j, k;
unsigned short*     row;
unsigned short      disparity;

int width = 240;
int height = 320;
int minDisparity = 0, maxDisparity = 130;
int maskSize = 15;



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


void grabColorAndStereo(FlyCaptureImage	&flycaptureImage, TriclopsImage16 &depthImage16, TriclopsImage &monoImage, TriclopsColorImage  &colorImageRight, TriclopsColorImage  &colorImageLeft)
{
	TriclopsColorImage dummyRightImage = {0};


	// Set up some stereo parameters:
   // Set to 320x240 output images
   te = triclopsSetResolution( triclops, width, height );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetResolution()");
   
   // Set disparity range
   te = triclopsSetDisparity( triclops, minDisparity, maxDisparity );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetDisparity()" );   
   
   // Lets turn off all validation except subpixel and surface
   // This works quite well
   te = triclopsSetTextureValidation( triclops, 0 );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetTextureValidation()" );
   te = triclopsSetUniquenessValidation( triclops, 0 );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetUniquenessValidation()" );
   te = triclopsSetBackForthValidation( triclops, 1 );
   te = triclopsSetStereoMask( triclops, maskSize );
   
   // Turn on sub-pixel interpolation
   te = triclopsSetSubpixelInterpolation( triclops, 1 );
   _HANDLE_TRICLOPS_ERROR( te, "triclopsSetSubpixelInterpolation()" );

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
   if (flycaptureImage.iNumImages == 2)
   {
      greenColorRight = redColorRight + ( 4 * imageCols );
	  greenColorLeft = redColorLeft + ( 4 * imageCols );
      blueColorRight = redColorRight + ( 4 * imageCols );
	  blueColorLeft = redColorLeft + ( 4 * imageCols );
   }
   
   if (flycaptureImage.iNumImages == 3)
   {
      greenColorRight = redColorRight + ( 4 * imageCols );
      blueColorRight = redColorRight + ( 2 * 4 * imageCols );
   }

   // Pointers to positions in the mono buffer that correspond to the beginning
   // of the red, green and blue sections
   unsigned char* redMono = NULL;
   unsigned char* greenMono = NULL;
   unsigned char* blueMono = NULL; 

   redMono = rowIntMono;
   if (flycaptureImage.iNumImages == 2)
   {
      greenMono = redMono + imageCols;
      blueMono = redMono + imageCols;
   }
   
   if (flycaptureImage.iNumImages == 3)
   {
      greenMono = redMono + imageCols;
      blueMono = redMono + ( 2 * imageCols );
   }

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

	  int len = sizeof(unsigned char)*dummyRightImage.ncols*dummyRightImage.nrows;
	unsigned char* strBlue = (unsigned char*)malloc(len);
	unsigned char* strGreen = (unsigned char*)malloc(len);
	unsigned char* strRed = (unsigned char*)malloc(len);
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

	  //colorImageLeft = dummyLeftImage;
	  
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
   
}
*/


void expandGrayscale(TriclopsImage image, unsigned char* &expandedData)
{
	expandedData = new unsigned char[3*image.ncols*image.nrows];

	for (int i = 0; i < image.nrows; i++)
		for (int j = 0; j < image.ncols; j++)
			for (int k = 0; k < 3; k++)
			{
				int indexExp = i * image.rowinc * 3 + j * 3 + k;
				int index = i * image.rowinc + j;
				expandedData[indexExp]= image.data[index];
			}
}

void expandColor(TriclopsColorImage &image, unsigned char* &expandedData)
{
	expandedData = new unsigned char[3*image.ncols*image.nrows];
	for (int i = 0; i < image.nrows; i++)
		for (int j = 0; j < image.ncols; j++)
		{
			int indexExp = i * image.rowinc * 3 + j * 3;
			int index = i * image.rowinc + j;
			expandedData[indexExp] = image.red[index];
			expandedData[indexExp + 1] = image.green[index];
			expandedData[indexExp + 2] = image.blue[index];
		}
}

void expandDepth(TriclopsImage16  &image16, unsigned char* &expandedData)
{
	expandedData = new unsigned char[3*image16.ncols*image16.nrows];
	for (int i = 0; i < image16.nrows; i++)
		for (int j = 0; j < image16.ncols; j++)
			for (int k = 0; k < 3; k++)
			{
				int indexExp = i * image16.rowinc/2 * 3 + j * 3 + k;
				int index = i * image16.rowinc/2 + j;
				expandedData[indexExp]= image16.data[index]/256;
			}
}

/*

void initCamera()
{
	//Get the default flycapture context
	std::cout << "Creating context for determining buffer size.\n" << std::endl;
    fe = flycaptureCreateContext( &flycapture );
	_HANDLE_FLYCAPTURE_ERROR( fe, "flycaptureCreateContext()" );

	//Initialize the camera
	std::cout << "Initializing camera " << _CAMERA_INDEX << std::endl;
	fe = flycaptureInitialize( flycapture, _CAMERA_INDEX );
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

   switch (pInfo.CameraModel)
   {
   case FLYCAPTURE_BUMBLEBEE2:
      {
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
      }
      break;
      
   case FLYCAPTURE_BUMBLEBEEXB3:
      iMaxCols = 1280;
      iMaxRows = 960;
      break;
      
   default:
      te = TriclopsErrorInvalidCamera;
      _HANDLE_TRICLOPS_ERROR( te, "triclopsCheckCameraModel()" );
      break;
   }

    // Start transferring images from the camera to the computer
   fe = flycaptureStartCustomImage( 
      flycapture, 3, 0, 0, iMaxCols, iMaxRows, 100, pixelFormat);
   _HANDLE_FLYCAPTURE_ERROR( fe, "flycaptureStart()" );

}

void destroyCamera()
{
// Close the camera
   fe = flycaptureStop( flycapture );
   _HANDLE_FLYCAPTURE_ERROR( fe, "flycaptureStop()" );
   
   fe = flycaptureDestroyContext( flycapture );
   _HANDLE_FLYCAPTURE_ERROR( fe, "flycaptureDestroyContext()" );
   
   // Destroy the Triclops context
   te = triclopsDestroyContext( triclops ) ;
   _HANDLE_TRICLOPS_ERROR( te, "triclopsDestroyContext()" );
}

/*
void grabImage(TriclopsImage &tr_disparityImage, TriclopsImage &tr_refImage, FlyCaptureImage &a_image, FlyCaptureImage &f_colorImage)
{
	//TriclopsImage     tr_disparityImage;
	FlyCaptureImage f_image;
	memset( &f_image, 0x0, sizeof( FlyCaptureImage ) );

	std::cout << "Grabbing single image to get image sizes" << std::endl;
	f_error = flycaptureGrabImage2( f_context, &f_image );
	_HANDLE_FLYCAPTURE_ERROR( f_error, "flycaptureGrabImage2()" ); 

	std::cout << "Number of cols in the image: " << f_image.iCols << std::endl;
	std::cout << "Num of rows in the image: " << f_image.iRows << std::endl;
	std::cout << "Num of images: " << f_image.iNumImages << std::endl;

	FlyCaptureImage imageConverted;
	imageConverted.pData = new unsigned char[f_image.iCols * f_image.iRows * 3];
	imageConverted.pixelFormat = FLYCAPTURE_BGR;

	
	f_error = flycaptureConvertImage( f_context, &f_image, &imageConverted);
	_HANDLE_FLYCAPTURE_ERROR( f_error, "flycaptureConvertImage()" ); 
	

	// Extract information from the FlycaptureImage
	int imageCols = f_image.iCols;
	int imageRows = f_image.iRows;
	int imageRowInc = f_image.iRowInc;
	int iSideBySideImages = f_image.iNumImages;
	unsigned long timeStampSeconds = f_image.timeStamp.ulSeconds;
	unsigned long timeStampMicroSeconds = f_image.timeStamp.ulMicroSeconds;

	// Create buffers for holding the mono images
	unsigned char* rowIntMono = new unsigned char[ imageCols * imageRows * iSideBySideImages ];

	// Create a temporary FlyCaptureImage for preparing the stereo image
	FlyCaptureImage f_tempImage;
	f_tempImage.pData = rowIntMono;

	// Convert the pixel interleaved raw data to row interleaved format
	f_error = flycapturePrepareStereoImage( f_context, f_image, &f_tempImage, NULL);
	_HANDLE_FLYCAPTURE_ERROR( f_error, "flycapturePrepareStereoImage()" );

	// Pointers to positions in the mono buffer that correspond to the beginning
	// of the red, green and blue sections
	unsigned char* redMono = NULL;
	unsigned char* greenMono = NULL;
	unsigned char* blueMono = NULL;

	redMono = rowIntMono;
	if (f_image.iNumImages == 2)
	{
		greenMono = redMono + imageCols;
		blueMono = redMono + imageCols;
	}
	if (f_image.iNumImages == 3)
	{
		greenMono = redMono + imageCols;
		blueMono = redMono + ( 2 * imageCols );
	}
   
	// Use the row interleaved images to build up an RGB TriclopsInput.  
	// An RGB triclops input will contain the 3 raw images (1 from each camera).
	tr_error = triclopsBuildRGBTriclopsInput(
		imageCols, 
		imageRows, 
		imageRowInc,  
		timeStampSeconds, 
		timeStampMicroSeconds, 
		redMono, 
		greenMono, 
		blueMono, 
		&tr_Input);
	_HANDLE_TRICLOPS_ERROR( "triclopsBuildRGBTriclopsInput()", tr_error );

	// Rectify the images
	tr_error = triclopsRectify( tr_context, &tr_Input );
	_HANDLE_TRICLOPS_ERROR( "triclopsRectify()", tr_error );
   
	// Do stereo processing
	tr_error = triclopsStereo( tr_context );
	_HANDLE_TRICLOPS_ERROR( "triclopsStereo()", tr_error );
   
	// Retrieve the disparity image from the triclops context
	tr_error = triclopsGetImage( tr_context, TriImg_DISPARITY, TriCam_REFERENCE, &tr_disparityImage );
	_HANDLE_TRICLOPS_ERROR( "triclopsGetImage()", tr_error );

	// Retrieve the rectified image from the triclops context
	tr_error = triclopsGetImage( tr_context, TriImg_RECTIFIED, TriCam_REFERENCE, &tr_refImage );
	_HANDLE_TRICLOPS_ERROR( "triclopsGetImage()", tr_error );
   
	// Save the disparity and reference images
	tr_error = triclopsSaveImage( &tr_disparityImage, "disparity.pgm" );
	_HANDLE_TRICLOPS_ERROR( "triclopsSaveImage()", tr_error );

	tr_error = triclopsSaveImage( &tr_refImage, "reference.pgm" );
	_HANDLE_TRICLOPS_ERROR( "triclopsSaveImage()", tr_error );

	// Delete the image buffer
	delete [] rowIntMono;
	redMono = NULL;
	greenMono = NULL;
	blueMono = NULL;

	//return tr_disparityImage;
}
*/
