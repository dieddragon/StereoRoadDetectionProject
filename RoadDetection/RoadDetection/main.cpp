#include "main.h"

//Debug mode
bool debug = false;

//Title of the GUI elements
const string nameWindowMain = "Adaptive Stereo Road Detection";
const string nameWindowHue = "Hue Channel";
const string nameWindowSat = "Saturation Channel";
const string nameWindowThresholdedVar = "Variance of Y thresholded";
const string nameWindowThresholdedDiff = "Max Difference of Y thresholded";
const string nameWindowBlobsVar = "Variance of Y blobbed";
const string nameWindowBlobsDiff = "Max Difference of Y blobbed";
const string nameWindowClassifiedVar = "Classified using Var of Y";
const string nameBtnStart = "Start";
const string nameBtnNext = "Next";
const string nameTrackbarVarYThreshold = "Variance Y Threshold";
const string nameTrackbarDiffYThreshold = "Max Difference Y Threshold";

//Data files
//Index of the file
int countFile = 1;
//File extensions
const string fileExtImg = ".ppm";
const string fileExtPts = ".pts";
//Folders
//const string folderRoot = "C:\\Users\\KBO\\Dropbox\\Academy\\Projects\\Road Detection, Extraction and Tracking\\Stereo Vision\\Stereo Road Detection\\Software\\RoadDetectionv3 - OpenCV - i7\\RoadDetection\\";
const string folderRoot = "C:\\Users\\KBO\\Documents\\GitHub\\";
const string folderPts = "Data\\Pts\\";
const string folderImg = "Data\\Color\\";
//File names
string nameImg = folderRoot + folderImg + "(1).ppm";
string namePtsFile = folderRoot + folderPts + "(1).pts";

//Results
//Change algorithm params to string
string nBinsStr = static_cast<ostringstream*>( &(ostringstream() << n_bins) )->str();
string sizePatchStr = static_cast<ostringstream*>( &(ostringstream() << size_patch) )->str();
string nSamplesMinStr = static_cast<ostringstream*>( &(ostringstream() << n_samples_min) )->str();
string nClustersMaxStr = static_cast<ostringstream*>( &(ostringstream() << n_clusters_max) )->str();
string nClustersInitialStr = static_cast<ostringstream*>( &(ostringstream() << n_clusters_initial) )->str();
string nClustersNewStr = static_cast<ostringstream*>( &(ostringstream() << n_clusters_new) )->str();
string thresholdUpdateStr = static_cast<ostringstream*>( &(ostringstream() << threshold_update_mahalanobis) )->str();
string thresholdClassifyStr = static_cast<ostringstream*>( &(ostringstream() << threshold_classify_mahalanobis) )->str();
//Result folder
string folderWrite = "results\\";
//Result File name prefix
//string prefixResultsFile = "hs_" + nBinsStr + "bins_" + sizePatchStr + "pchs_" + nSamplesMinStr + "nsmpsmin_" + nClustersMaxStr + "_" + nClustersInitialStr + "_" + nClustersNewStr + "_clst_max_init_new_" + thresholdUpdateStr + "thrUpd_" + thresholdClassifyStr + "thrClsfy_";
string prefixResultsFile = "hs_" + nBinsStr + "bins_" + sizePatchStr + "pchs_" + nSamplesMinStr + "nsmpsmin_" + nClustersMaxStr + "_clst_max_" + thresholdUpdateStr + "thrUpd_" + thresholdClassifyStr + "thrClsfy_";
//Result file extension
const string fileExtResult = ".txt";
//Result file names
string nameResultsFile = folderWrite + prefixResultsFile + "(1).txt";
string nameResultsImg = folderWrite + prefixResultsFile + "(1).ppm";

//Data
Mat bgrMat, hsvMat;	//main images of bgr and hsv color spaces
Mat varPatches;		//Y coordinate variance of image patches 
Mat diffPatches;	//Y coordinate difference of image patches
vector<vector<MatND>> histsBGR;	//bgr histogram matrix of current image
vector<vector<MatND>> histsHS;	//hs histogram matrix of current image
Mat trainingBuffer; //Memory to hold the training samples of past and current time as histograms. So the info obtained in the past can be used
vector<Mat> trainingBufferLib;	//Memory to hold the training samples of each frame chronogically. This sample library is used in SVM method
//Model parameters
Mat means, weights;	//Means and Weights of the gaussians
vector<Mat> covs;	//Covariance matrixes of the gaussians
CvSVM model; //One-class SVM model

//event handler for the next button to go from one image to another
void callbackBtnNext(int state, void* dataPassed)
{
	//increase the file counter by 1
	countFile++;
	//if (countFile == 203)
	//	cout << "asd" << endl;
	//construct the next file names
	string countFileStr = static_cast<ostringstream*>( &(ostringstream() << countFile) )->str();
	nameImg = folderRoot + folderImg + "(" + countFileStr + ")" + fileExtImg;
	namePtsFile = folderRoot + folderPts + "(" + countFileStr + ")" + fileExtPts;
	nameResultsFile = folderWrite + prefixResultsFile + "(" + countFileStr + ")" + fileExtResult;
	nameResultsImg = folderWrite + prefixResultsFile + "(" + countFileStr + ")" + fileExtImg;

	//Read the image and store in a matrix
	//cout << nameImg << " is loading..." << endl;	
	//bgrMat = imread(nameImg, cv::IMREAD_COLOR);
	//cvtColor(bgrMat, hsvMat, CV_BGR2HSV);

	//Read the pts file and store x,y,z in a matrix
	//cout << namePtsFile << " is loading..." << endl;
	//Mat xyzMat = Mat::zeros(height, width, CV_64FC3);
	//readPtsDataFromFile(namePtsFile, xyzMat);

	//Calculate the variance of Y and the max diff of Y in patches
	//cout << "Patches are constructing..." << endl;
	//cout << "Variance and Difference in Y calulcation..." << endl;
	//varPatches = calculateVarianceYforPatches(xyzMat, size_patch);
	//diffPatches = calculateDifferenceMaxYforPatches(xyzMat, size_patch);

	//Calculate image patch histograms
	//cout << "Image patch histograms are building..." << endl;
	//histsBGR = calculateHistograms(bgrMat, SIZE_PATCH, CHANNELS, 3, SIZE_HIST, RANGES);
	//histsHS = calculateHistograms(hsvMat, size_patch, channels_hs, 2, size_hist_hs, ranges_hs, false);
	
	//show the original image
	//imshow(nameWindowMain, bgrMat);


	//Tracbar stuff
	//create slider for training sample threshold
	//createTrackbar( nameTrackbarVarYThreshold, nameWindowMain, &thresholdVarY, threshold_var_y_max, callbackTrackbarVarYThreshold );
	//createTrackbar( nameTrackbarDiffYThreshold, nameWindowMain, &thresholdDiffY, THRESHOLD_DIFF_Y_MAX, callbackTrackbarDiffYThreshold );
	//create next button to jump to the next image
	//createButton(nameBtnNext, callbackBtnNext, NULL);
	// Show some stuff
	//callbackTrackbarVarYThreshold( thresholdVarY, 0 );
	//callbackTrackbarDiffYThreshold( thresholdDiffY, 0 );

	//Read the image and store in a matrix
	cout << nameImg << " is loading..." << endl;	
	bgrMat = imread(nameImg, cv::IMREAD_COLOR);

	//Read the pts file and store x,y,z in a matrix
	cout << namePtsFile << " is loading..." << endl;
	Mat xyzMat = Mat::zeros(height, width, CV_64FC3);
	readPtsDataFromFile(namePtsFile, xyzMat);

	//convert the variance threshold value obtained from the trackbar to double
	thresholdVarYd = ((double)thresholdVarY)/threshold_var_y_divider;

	//Get ticks before algorithm
	double t = (double)getTickCount();

	//Mat classifiedMat = detectRoadSingleSVM(bgrMat, xyzMat, trainingBuffer, trainingBufferLib, model, weights, thresholdVarYd, false, distribution);
	Mat classifiedMat = detectRoad(bgrMat, xyzMat, trainingBuffer, means, covs, weights, thresholdVarYd, false, distribution);
	//Mat classifiedMat = detectRoadDebug(bgrMat, xyzMat, trainingBuffer, means, covs, weights, thresholdVarYd, false, distribution);
	//Mat classifiedMat = detectRoadBanded(bgrMat, xyzMat, trainingBuffer, means, covs, weights, thresholdVarYd, false, distribution);

	//cout << "Number of Training Sets : " << trainingBufferLib.size() << endl;
	//for (int i = 0; i < trainingBufferLib.size(); i++)
	//	cout << "Set " << i << " : " << trainingBufferLib[i].rows << endl;
	//for (int i = 0; i < trainingBufferLib.size(); i++)
	//	cout << "Set " << i << " : " << trainingBufferLib[i] << endl;

	//Get tocks after algorithm, find the difference and write in sec form
	t = ((double)getTickCount() - t)/getTickFrequency();
	cout << "Times passed in seconds: " << t << endl;

	//Show the classification result for the Y variance
	namedWindow(nameWindowClassifiedVar, CV_WINDOW_AUTOSIZE);
	imshow(nameWindowClassifiedVar, classifiedMat);

	//////////////////////////////////////////////////////////////////
	///////////////////// WRITING THE RESULTS ////////////////////////
	//////////////////////////////////////////////////////////////////

	//writing the classification results as image and/or text
	cout << "Writing to Image : " << nameResultsImg << endl;
	vector<int> output_params;
	output_params.push_back(CV_IMWRITE_PXM_BINARY);
	output_params.push_back(1);
	imwrite(nameResultsImg, classifiedMat, output_params);
	cout << "Writing to File: " << nameResultsFile << endl;
	writeResultDataToFile(nameResultsFile, classifiedMat);
	cout << "Done..." << endl;

}

void callbackBtnStart(int state,void* dataPassed)
{
	
	//Read the image and store in a matrix
	//cout << nameImg << " is loading..." << endl;	
	//bgrMat = imread(nameImg, cv::IMREAD_COLOR);
	//cvtColor(bgrMat, hsvMat, CV_BGR2HSV);

	//Read the pts file and store x,y,z in a matrix
	//cout << namePtsFile << " is loading..." << endl;
	//Mat xyzMat = Mat::zeros(height, width, CV_64FC3);
	//readPtsDataFromFile(namePtsFile, xyzMat);

	//Calculate the variance of Y and the max diff of Y in patches
	//cout << "Patches are constructing..." << endl;
	//cout << "Variance and Difference in Y calulcation..." << endl;
	//varPatches = calculateVarianceYforPatches(xyzMat, size_patch);
	//diffPatches = calculateDifferenceMaxYforPatches(xyzMat, size_patch);

	//Calculate image patch histograms
	//cout << "Image patch histograms are building..." << endl;
	//histsBGR = calculateHistograms(bgrMat, SIZE_PATCH, CHANNELS, 3, SIZE_HIST, RANGES);
	//histsHS = calculateHistograms(hsvMat, size_patch, channels_hs, 2, size_hist_hs, ranges_hs, false);
	
	//imshow(nameWindowMain, bgrMat);

	//Tracbar stuff
	//create slider for training sample threshold
	//createTrackbar( nameTrackbarVarYThreshold, nameWindowMain, &thresholdVarY, threshold_var_y_max, callbackTrackbarVarYThreshold );
	//createTrackbar( nameTrackbarDiffYThreshold, nameWindowMain, &thresholdDiffY, THRESHOLD_DIFF_Y_MAX, callbackTrackbarDiffYThreshold );
	//create next button to jump to the next image
	//createButton(nameBtnNext, callbackBtnNext, NULL);
	// Show some stuff
	//callbackTrackbarVarYThreshold( thresholdVarY, 0 );
	//callbackTrackbarDiffYThreshold( thresholdDiffY, 0 );
	//for (int i = 0; i < 49; i++)
		//callbackBtnNext(0, 0);

	//Mat asd = (Mat_<double>(1,3) << 1, 2, 3);
	//cout << asd.mul(asd) << endl;
	//cout << asd.mul(asd) + asd.mul(asd) << endl;
	//cout << (asd.mul(asd) + asd.mul(asd))/2 << endl;

	//Read the image and store in a matrix
	cout << nameImg << " is loading..." << endl;	
	bgrMat = imread(nameImg, cv::IMREAD_COLOR);

	//Read the pts file and store x,y,z in a matrix
	cout << namePtsFile << " is loading..." << endl;
	Mat xyzMat = Mat::zeros(height, width, CV_64FC3);
	readPtsDataFromFile(namePtsFile, xyzMat);

	//convert the variance threshold value obtained from the trackbar to double
	thresholdVarYd = ((double)thresholdVarY)/threshold_var_y_divider;

	//Get ticks before algorithm
	double t = (double)getTickCount();

	//Mat classifiedMat = detectRoadSingleSVM(bgrMat, xyzMat, trainingBuffer, trainingBufferLib, model, weights, thresholdVarYd, true, distribution);
	Mat classifiedMat = detectRoad(bgrMat, xyzMat, trainingBuffer, means, covs, weights, thresholdVarYd, true, distribution);
	//Mat classifiedMat = detectRoadDebug(bgrMat, xyzMat, trainingBuffer, means, covs, weights, thresholdVarYd, true, distribution);
	//Mat classifiedMat = detectRoadBanded(bgrMat, xyzMat, trainingBuffer, means, covs, weights, thresholdVarYd, true, distribution);
	


	//cout << "Number of Training Sets : " << trainingBufferLib.size() << endl;
	//for (int i = 0; i < trainingBufferLib.size(); i++)
	//	cout << "Set " << i << " : " << trainingBufferLib[i].rows << endl;
	//for (int i = 0; i < trainingBufferLib.size(); i++)
	//	cout << "Set " << i << " : " << trainingBufferLib[i] << endl;
	//Get tocks after algorithm, find the difference and write in sec form
	t = ((double)getTickCount() - t)/getTickFrequency();
	cout << "Times passed in seconds: " << t << endl;

	//Show the classification result for the Y variance
	namedWindow(nameWindowClassifiedVar, CV_WINDOW_AUTOSIZE);
	imshow(nameWindowClassifiedVar, classifiedMat);
		
	//create next button to jump to the next image
	createButton(nameBtnNext, callbackBtnNext, NULL);

	//////////////////////////////////////////////////////////////////
	///////////////////// WRITING THE RESULTS ////////////////////////
	//////////////////////////////////////////////////////////////////

	//writing the classification results as image and/or text
	cout << "Writing to Image : " << nameResultsImg << endl;
	vector<int> output_params;
	output_params.push_back(CV_IMWRITE_PXM_BINARY);
	output_params.push_back(1);
	imwrite(nameResultsImg, classifiedMat, output_params);
	cout << "Writing to File: " << nameResultsFile << endl;
	writeResultDataToFile(nameResultsFile, classifiedMat);
	cout << "Done..." << endl;

	for (int i = 0; i < 358; i++)
		callbackBtnNext(0, 0);
}

int main(int argc, char* argv[])
{
	cout << "%%%% Road Detection Program %%%%" << endl;

	//create main window
	namedWindow(nameWindowMain,CV_WINDOW_AUTOSIZE);

	//create start button (push button)
	createButton(nameBtnStart,callbackBtnStart,NULL);

	while(cvWaitKey(33) != 27)
    {

    }

	//destroy the contents
	destroyAllWindows();

	return 0;
}