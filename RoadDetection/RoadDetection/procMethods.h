#ifndef ROADDETECTION_PROCMETHODS_H__
#define ROADDETECTION_PROCMETHODS_H__

//////////////////////////////////////////////////////////////////////
/////////////////////// POINT CLOUD PROCESSING ///////////////////////
//////////////////////////////////////////////////////////////////////

//from a patch of xyz values, calculates the unbiased variance of y 
//xyzPatch should be a 3-channel matrix of doubles
double calculateVarianceYforPatch(Mat xyzPatch);

//from the matrix of xyz values (double), calculates the unbiased variance of y for all patches 
//xyzMat should be a 3-channel matrix of doubles.
//sizePatch is the number of pixels in one side of the patch.
Mat calculateVarianceYforPatches(Mat xyzMat, int sizePatch);

//calculates the maximum difference in Y coord within the patch
//returns the absolute value of it. xyzPatch should be a 3-channel matrix of doubles.
double calculateDifferenceMaxYforPatch(Mat xyzPatch);

//calculates the maximum difference in Y coord within the patch
//returns the absolute value of it. xyzMat should be a 3-channel matrix of doubles.
//sizePatch is the number of pixels in one side of the patch.
Mat calculateDifferenceMaxYforPatches(Mat xyzMat, int sizePatch);

//////////////////////////////////////////////////////////////////////
////////////////////////// COLOR PROCESSING //////////////////////////
//////////////////////////////////////////////////////////////////////

//Filters the hsv image and point cloud data according to the saturation channel of hsv image
//Pixels which have a saturation channel value greater than "thresholdSatBandMin" passes
//while others grounded to zero
void filterSatChannel(Mat xyzMat, Mat hsvMat, int thresholdSatBandMin);

//Filters the hsv image and point cloud data according to the value channel of hsv image
//Pixels which have a value channel value between "thresholdValBandMin" and "thresholdValBandMax" passes
//while others grounded to zero
void filterValueChannel(Mat xyzMat, Mat hsvMat, int thresholdValBandMin, int thresholdValBandMax);

//takes a 3 channel (HSV) matrix of uchars and means of each channel, and calculates the 4 elements of covariance matrix
//and store them into the last 4 elements of means vector
//means is a matrix of 1 channels of doubles
void calculateCovs2D(Mat m, Mat &means);

//takes a 3 channel (HSV) matrix of uchars and calculates means of H and S channels and store them into the first two elements of means
//means is a matrix of 1 channels of doubles
void calculateMeans2D(Mat m, Mat &means);

//calculates the 2D HS Gaussians of image patches and return them as a matrix of 6 dims vector (2 means + 4 covs)  
vector<vector<Mat>> calculateGaussiansHS2D(Mat m, int sizePatch);

//takes a 3 channel matrix of uchars and means of each channel, and calculates the 9 elements of covariance matrix
//and store them into the last 9 elements of means vector
//means is a matrix of 1 channels of doubles
void calculateCovs3D(Mat m, Mat &means);

//takes a 3 channel matrix of uchars and calculates means of each channel and store them into the first three elements of means
//means is a matrix of 1 channels of doubles
void calculateMeans3D(Mat m, Mat &means);

//calculates the 3D BGR Gaussians of image patches and return them as a matrix of 12 dims vector (3 means + 9 covs)  
vector<vector<Mat>> calculateGaussiansBGR3D(Mat m, int sizePatch);

//calculates the joint histograms of image patches and return them as a matrix of histograms
vector<vector<Mat>> calculateHistograms(Mat m, int sizePatch, const int* channels, int dims, const int* histSize, const float** ranges, bool isNormalized);

//given a matrix of y variance or y diff patches, original image and the patch size, the road and non-road areas are
//overlaid on the original image. Green shows road, reddish shows non-road.
//Patches should be a matrix of doubles and img should be a 3-channel matrix of uchar
Mat transformPatches2Color3Chi(Mat patches, Mat img, int sizePatch);

//thresholds the source patches and make their values to 255 if greater than threshold
//make them 0 else. Source matrix should be 1 channel and be of type double
Mat thresholdPatches1Chi(Mat source, double threshold, int *ntr);

//////////////////////////////////////////////////////////////////////
////////////////////////// BLOB FINDING //////////////////////////////
//////////////////////////////////////////////////////////////////////

//blob structure to hold the blob data
struct blob 
{
	vector<Point2i> patchCoords;	//row, col coords of the image patch belonging to the blob
	int count;	//number of patches in a blob
};

//finds the blobs in the given image patch. In this case a matrix of doubles which only has either 255 or 0 are used.
void findBlobs(Mat imgPatch, vector<blob> &blobs);

//debug mode
Mat filterBiggestBlob(Mat bgrMat, Mat patchesVarThresholded, Mat &resultantMat);

//filters out the biggest blob in the given matrix of y variance or y difference patches (matrix of doubles)
//and returns the image with the biggest found road area
//patchesVarThresholded is a 1 channel matrix of doubles
//resultantPatches is a 3-channel matrix of uchars
//returns the training samples in the biggest blob (a matrix of doubles)
Mat filterBiggestBlob(Mat patchesVarThresholded, int *ntr);

#endif ROADDETECTION_PROCMETHODS_H__