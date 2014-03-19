#ifndef ROADDETECTION_ALGORITHM_H__
#define ROADDETECTION_ALGORITHM_H__

Mat detectRoadHistHS(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, Mat &means, vector<Mat> &covs, Mat &weights, double thresholdVarYd, bool isFirstFrame);

Mat detectRoadHistBGR(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, Mat &means, vector<Mat> &covs, Mat &weights, double thresholdVarYd, bool isFirstFrame);

Mat detectRoadHS(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, Mat &means, vector<Mat> &covs, Mat &weights, double thresholdVarYd, bool isFirstFrame);

Mat detectRoadBGR(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, Mat &means, vector<Mat> &covs, Mat &weights, double thresholdVarYd, bool isFirstFrame);

//road detection function with band-pass on value channel of hsv color space. Takes original bgr image and xyz point cloud matrices and classify the road regions in the image
//returns the classified image as road and non-road regions overlaid on the original image
//road regions are green, non red regions are reddish
//imgMat is a matrix of 3-channels uchars
//xyzMat is a matrix of 3-channels doubles
//trainingBuffer is a matrix that holds training samples (histograms)
//means is a matrix of 1-channel double that holds means of the Gaussians
//covs is a vector of 1-channel double matrices that holds diagonal covariance matrices of the Gaussians
//weights is a matrix of 1-channel double that holds weight of the Gaussians
//thresholdVarYd is the height variance threshold to be used in the training sample generation
//isFirstFrame is a bool to indicate whether the system is just started or not
//distributionType is one of the RGB_GAUSS, HS_GAUSS, RGB_HIST and HS_HIST
Mat detectRoadBanded(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, Mat &means, vector<Mat> &covs, Mat &weights, double thresholdVarYd, bool isFirstFrame, typeDistribution distributionType);

//road detection function. Takes original bgr image and xyz point cloud matrices and classify the road regions in the image
//returns the classified image as road and non-road regions overlaid on the original image
//road regions are green, non red regions are reddish
//imgMat is a matrix of 3-channels uchars
//xyzMat is a matrix of 3-channels doubles
//trainingBuffer is a matrix that holds training samples (histograms)
//means is a matrix of 1-channel double that holds means of the Gaussians
//covs is a vector of 1-channel double matrices that holds diagonal covariance matrices of the Gaussians
//weights is a matrix of 1-channel double that holds weight of the Gaussians
//thresholdVarYd is the height variance threshold to be used in the training sample generation
//isFirstFrame is a bool to indicate whether the system is just started or not
//distributionType is one of the RGB_GAUSS, HS_GAUSS, RGB_HIST and HS_HIST
Mat detectRoad(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, Mat &means, vector<Mat> &covs, Mat &weights, double thresholdVarYd, bool isFirstFrame, typeDistribution distributionType);

/////////////////////////// MULTI MODEL //////////////////////////////

//////////////////////////////////////////////////////////////////////
///// NOT WORKING WITH OPENCV 2.4.3 SINCE CvSVM COPY CONSTRUCTOR /////
/////////////////// DOES NOT WORKING PROPERLY ////////////////////////
///////////////// WILL IMPLEMENT MY OWN SVN LIB //////////////////////

/*
//road detection function. Takes original bgr image and xyz point cloud matrices and classify the road regions in the image
//returns the classified image as road and non-road regions overlaid on the original image
//road regions are green, non red regions are reddish
//imgMat is a matrix of 3-channels uchars
//xyzMat is a matrix of 3-channels doubles
//trainingBuffer is a matrix that holds training samples (histograms)
//means is a matrix of 1-channel double that holds means of the Gaussians
//covs is a vector of 1-channel double matrices that holds diagonal covariance matrices of the Gaussians
//weights is a matrix of 1-channel double that holds weight of the Gaussians
//thresholdVarYd is the height variance threshold to be used in the training sample generation
//isFirstFrame is a bool to indicate whether the system is just started or not
//distributionType is one of the RGB_GAUSS, HS_GAUSS, RGB_HIST and HS_HIST
Mat detectRoadSVM(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, vector<CvSVM> &models, Mat &weights, double thresholdVarYd, bool isFirstFrame, typeDistribution distributionType);
*/

/////////////////////////// SINGLE MODEL //////////////////////////////

//road detection function. Takes original bgr image and xyz point cloud matrices and classify the road regions in the image
//returns the classified image as road and non-road regions overlaid on the original image
//road regions are green, non red regions are reddish
//imgMat is a matrix of 3-channels uchars
//xyzMat is a matrix of 3-channels doubles
//trainingBuffer is a matrix that holds training samples
//trainingBufferLib is the vector of matrices that holds the samples gathered in the history
//model is a CvSVM model of one-class svm
//thresholdVarYd is the height variance threshold to be used in the training sample generation
//isFirstFrame is a bool to indicate whether the system is just started or not
//distributionType is one of the RGB_GAUSS, HS_GAUSS, RGB_HIST and HS_HIST
Mat detectRoadSingleSVM(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, vector<Mat> &trainingBufferLib, CvSVM &model, Mat &weights, double thresholdVarYd, bool isFirstFrame, typeDistribution distributionType);

///////////////////////////////////////////////////////////////////////
///////////// SAMPLE BASED ALGORITHM (NO MODEL LEARNING) //////////////
///////////////////////////////////////////////////////////////////////

//road detection function. Takes original bgr image and xyz point cloud matrices and classify the road regions in the image
//returns the classified image as road and non-road regions overlaid on the original image
//road regions are green, non red regions are reddish
//imgMat is a matrix of 3-channels uchars
//xyzMat is a matrix of 3-channels doubles
//trainingBuffer is a matrix that holds training samples
//trainingBufferLib is the vector of matrices that holds the samples gathered in the history
//thresholdVarYd is the height variance threshold to be used in the training sample generation
//isFirstFrame is a bool to indicate whether the system is just started or not
//distributionType is one of the RGB_GAUSS, HS_GAUSS, RGB_HIST and HS_HIST
Mat detectRoadSampleBased(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, vector<Mat> &trainingBufferLib, double thresholdVarYd, bool isFirstFrame, typeDistribution distributionType);

/////////////////////////////////////// DEBUG ALGORITHM ///////////////////////////////////
//road detection function. Takes original bgr image and xyz point cloud matrices and classify the road regions in the image
//returns the classified image as road and non-road regions overlaid on the original image
//road regions are green, non red regions are reddish
//imgMat is a matrix of 3-channels uchars
//xyzMat is a matrix of 3-channels doubles
//trainingBuffer is a matrix that holds training samples (histograms)
//means is a matrix of 1-channel double that holds means of the Gaussians
//covs is a vector of 1-channel double matrices that holds diagonal covariance matrices of the Gaussians
//weights is a matrix of 1-channel double that holds weight of the Gaussians
//thresholdVarYd is the height variance threshold to be used in the training sample generation
//isFirstFrame is a bool to indicate whether the system is just started or not
//distributionType is one of the RGB_GAUSS, HS_GAUSS, RGB_HIST and HS_HIST
Mat detectRoadDebug(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, Mat &means, vector<Mat> &covs, Mat &weights, double thresholdVarYd, bool isFirstFrame, typeDistribution distributionType);


#endif ROADDETECTION_ALGORITHM_H__