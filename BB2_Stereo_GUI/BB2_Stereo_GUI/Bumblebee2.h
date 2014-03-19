#ifndef BUMBLEBEE2_H__
#define BUMBLEBEE2_H__

#include "triclops.h"
#include "PGRFlyCapture.h"
#include "PGRFlyCapturePlus.h"
#include "PGRFlyCaptureStereo.h"

class Bumblebee2
{

public:
	//Constructor
	Bumblebee2();
	//Destructor
	virtual ~Bumblebee2();

	//Member Functions
	//getters
	int getWidthImage();
	int getHeightImage();
	int getMinDisparity();
	int getMaxDisparity();
	int getMaskSizeStereo();
	int getIndexCamera();
	int getVideoMode();
	int getFrameRate();
	int getSurfaceValidationSize();

	bool getIsTextureValidation();
	bool getIsSurfaceValidation();
	bool getIsBackForthValidation();
	bool getIsSubpixelInterpolation();

	float getTextureValidationThreshold();
	float getSurfaceValidationDifference();

	FlyCaptureImage getFlyCaptureImage();
	TriclopsImage16     getDepthImage16();
	TriclopsImage       getMonoImage();
	TriclopsColorImage  getColorImageLeft();
	TriclopsColorImage  getColorImageRight();


	//setters
	void setWidthImage(int width);
	void setHeightImage(int height);
	void setMinDisparity(int minDisp);
	void setMaxDisparity(int maxDisp);
	void setMaskSizeStereo(int maskSize);
	void setIndexCamera(int index);
	void setVideoMode(int videoMode);
	void setFrameRate(int frameRate);
	void setSurfaceValidationSize(int size);

	void setIsTextureValidation(bool b);
	void setIsSurfaceValidation(bool b);
	void setIsBackForthValidation(bool b);
	void setIsSubpixelInterpolation(bool b);

	void setTextureValidationThreshold(float threshold);
	void setSurfaceValidationDifference(float difference);

	//methods
	void grabColorAndStereo(FlyCaptureImage	&flycaptureImage, TriclopsImage16 &depthImage16, TriclopsImage &monoImage, TriclopsColorImage  &colorImageRight, TriclopsColorImage  &colorImageLeft);
	void grabColorAndStereo();
	void initCamera();
	void destroyCamera();
	void releaseBuffers();
	void allocBuffers();
	void setStereoParams();
	void saveData(std::string fileName);
	void RCDToXYZ(int row, int col, float &x, float &y, float &z); 

protected:
	//Signal Handlers

	//Members
	int widthImage;
	int heightImage;
	int minDisparity;
	int maxDisparity;
	int maskSizeStereo;
	int indexCamera;
	int videoMode;
	int frameRate;
	int surfaceValidationSize;

	bool isTextureValidation;
	bool isSurfaceValidation;
	bool isBackForthValidation;
	bool isSubpixelInterpolation;

	float textureValidationThreshold;
	float surfaceValidationDifference;


private:
	//Global variables
	//Flycapture related
	FlyCaptureContext	   flycapture;
	FlyCaptureImage	   flycaptureImage;
	FlyCaptureInfoEx	   pInfo;
	FlyCapturePixelFormat   pixelFormat;
	FlyCaptureError	   fe;
	//Triclops related
	TriclopsInput       stereoData;
	TriclopsInput       colorDataRight;
	TriclopsInput		colorDataLeft;
	TriclopsImage16     depthImage16;
	TriclopsImage       monoImage;
	TriclopsColorImage  colorImageLeft;
	TriclopsColorImage  colorImageRight;
	TriclopsContext     triclops;
	TriclopsError       te;

	int iMaxCols;
	int iMaxRows;
	char* szCalFile;
	char* szSavePtsFile;

	//FILE* pointFile;

	//Some buffer to take right camera image
	unsigned char* strBlue;
	unsigned char* strGreen;
	unsigned char* strRed;

	//Private Member functions
	

	
};

#endif BUMBLEBEE2_H