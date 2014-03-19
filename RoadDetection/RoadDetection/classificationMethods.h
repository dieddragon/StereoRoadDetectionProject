#ifndef ROADDETECTION_CLASSIFICATIONMETHODS_H__
#define ROADDETECTION_CLASSIFICATIONMETHODS_H__

/*
//////////////////////////////////////////////////////////////////////
/////////// AGGLOMERATIVE CLUSTERING OF SAMPLES OPERATIONS ///////////
//////////////////////////////////////////////////////////////////////

////////////////////// UNCLASSIFIED OTHER METHODS ///////////////////////

//finds the minimum Euclidian distance to a cluster of samples from a sample point (sample histogram)
float findMinDistanceToCluster(Mat cluster, MatND hist);

//finds the Euclidian distance to the closest sample of the closest cluster of samples among an array of clusters
float findClosestDistanceToClusters(vector<Mat> clusters, MatND hist);

//Classify using cluster of samples (sample based classification)
void classify(vector<Mat> clusters, vector<vector<MatND>> hists, Mat &resultantMat, int sizePatch, float threshold);
*/

//////////////////////////////////////////////////////////////////////
///////////////////// SAMPLE BASED CLUSTERING ////////////////////////
//////////////////////////////////////////////////////////////////////

//////////////////// VIA MAHALANOBIS DISTANCE ////////////////////////

//Given a training sample library find the closest sample to the sample (feature vector) according to mahalanobis distance
float findClosestMahDistanceToSamples(vector<Mat> trainingSamplesLib, Mat covInv, Mat feature);

void classifyThroughTrainingSamplesMah(vector<Mat> trainingBufferLib, Mat covInv, vector<vector<Mat>> featureVec, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold);

//////////////////// VIA EUCLIDIAN DISTANCE //////////////////////////

//Given a training sample library find the closest sample to the sample (feature vector) according to euclidian distance
float findClosestEucDistanceToSamples(vector<Mat> trainingSamplesLib, Mat feature);

void classifyThroughTrainingSamplesEuc(vector<Mat> trainingBufferLib, vector<vector<Mat>> featureVec, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold);


//////////////////////////////////////////////////////////////////////
/////////////////// K-MEANS CLUSTERING OPERATIONS ////////////////////
//////////////////////////////////////////////////////////////////////

//finds the closest Euclidian distance to the closest cluster among an array of clusters
float findClosestDistanceToClusters(Mat clusterCenters, MatND hist);

//classify using kmeans clusters via Euclidian distance
void classifyKMeans(Mat centers, vector<vector<MatND>> hists, Mat &resultantMat, int sizePatch, float threshold);

//////////////////////////////////////////////////////////////////////
///////////////////// EM CLUSTERING OPERATIONS ///////////////////////
//////////////////////////////////////////////////////////////////////

//////////////////// VIA MAHALANOBIS DISTANCE ////////////////////////

//Given means and inverse of covariances find the closest cluster to the sample (feature vector) according to mahalanobis distance
float findClosestMahDistanceToClusters(Mat means, vector<Mat> covsInv, Mat feature);

//resultantMat is of type double and 1 channel matrix, it is the patch matrix not an image
void classifyEMBinaryMah(Mat means, vector<Mat> covsInv, Mat weights, vector<vector<Mat>> features, Mat &resultantMat, float threshold);

//Classification using EM found Gaussians via region growing from the training samples
//classifiedPatches is the patch matrix not an image
void classifyEMThroughTrainingSamplesMah(Mat means, vector<Mat> covsInv, Mat weights, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold);

//find a cluster closer than threshold according to mahalanobis distance
int findClusterMah(Mat means, vector<Mat> covsInv, Mat histMat, float threshold);

//find the cluster closest to histMat according to mahalanobis distance
//if cannot find, returns -1
int findClosestClusterMah(Mat means, vector<Mat> covsInv, Mat histMat, float threshold);

//////////////////// VIA EUCLIDIAN DISTANCE ////////////////////////

//Given means find the closest cluster to the sample (feature vector) according to Euclidian distance
float findClosestEucDistanceToClusters(Mat means, Mat feature);

//resultantMat is of type double and 1 channel matrix, it is the patch matrix not an image
void classifyEMBinaryEuc(Mat means, vector<vector<Mat>> features, Mat &resultantMat, float threshold);

//Classification using EM found Gaussians via region growing from the training samples
//classifiedPatches is the patch matrix not an image
void classifyEMThroughTrainingSamplesEuc(Mat means, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold);

//find a cluster closer than threshold according to Euclidian distance
//if cannot find, returns -1
int findClusterEuc(Mat means, Mat feature, float threshold);

//find the cluster closest to feature (vector) according to Euclidian distance
//if cannot find, returns -1
int findClosestClusterEuc(Mat means, Mat feature, float threshold);

//////////////////// VIA MANHATTAN DISTANCE ////////////////////////

//Given means find the closest cluster to the sample (feature vector) according to Manhattan distance
float findClosestManDistanceToClusters(Mat means, Mat feature);

//resultantMat is of type double and 1 channel matrix, it is the patch matrix not an image
void classifyEMBinaryMan(Mat means, vector<vector<Mat>> features, Mat &resultantMat, float threshold);

//Classification using EM found Gaussians via region growing from the training samples
//classifiedPatches is the patch matrix not an image
void classifyEMThroughTrainingSamplesMan(Mat means, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold);

//find a cluster closer than threshold according to Manhattan distance
//if cannot find, returns -1
int findClusterMan(Mat means, Mat feature, float threshold);

//find the cluster closest to feature (vector) according to Manhattan distance
//if cannot find, returns -1
int findClosestClusterMan(Mat means, Mat feature, float threshold);

//////////////////// VIA CHEBYSHEV DISTANCE ////////////////////////

//Given means find the closest cluster to the sample (feature vector) according to Chebyshev distance
float findClosestChebDistanceToClusters(Mat means, Mat feature);

//resultantMat is of type double and 1 channel matrix, it is the patch matrix not an image
void classifyEMBinaryCheb(Mat means, vector<vector<Mat>> features, Mat &resultantMat, float threshold);

//Classification using EM found Gaussians via region growing from the training samples
//classifiedPatches is the patch matrix not an image
void classifyEMThroughTrainingSamplesCheb(Mat means, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold);

//find a cluster closer than threshold according to Chebyshev distance
//if cannot find, returns -1
int findClusterCheb(Mat means, Mat feature, float threshold);

//find the cluster closest to feature (vector) according to Chebyshev distance
//if cannot find, returns -1
int findClosestClusterCheb(Mat means, Mat feature, float threshold);

//////////////////// VIA HELLINGER DISTANCE ////////////////////////

////////////////// FOR HISTOGRAMS ////////////////////////////

//Given means of the models find the closest cluster to the sample (feature vector) according to hellinger distance
double findClosestHellDistanceToClustersHist(Mat means, Mat feature);

//resultantMat is of type double and 1 channel matrix, it is the patch matrix not an image
void classifyEMBinaryHellHist(Mat means, vector<vector<Mat>> features, Mat &resultantMat, float threshold);

//Classification using EM found Gaussians via region growing from the training samples
//classifiedPatches is the patch matrix not an image
void classifyEMThroughTrainingSamplesHellHist(Mat means, Mat weights, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold);

//find a cluster closer than threshold according to hellinger distance
int findClusterHellHist(Mat means, Mat histMat, float threshold);

//find the cluster closest to histMat according to hellinger distance
//if cannot find, returns -1
int findClosestClusterHellHist(Mat means, Mat histMat, float threshold);

////////////////// FOR GAUSSIANS ////////////////////////////

//Finds the Hellinger distance between two gaussians
//Gaussians are in the form of vectors
//If 2d: mean1 mean2 var11 var22 var12 var21
//If 3d: mean1 mean2 mean3 var11 var22 var33 var12 var13 var23 var21 var31 var32
double findHellDistanceGauss(Mat gaussian1, Mat gaussian2);

//Given means of the models find the closest cluster to the sample (feature vector) according to hellinger distance
double findClosestHellDistanceToClustersGauss(Mat means, Mat feature);

//resultantMat is of type double and 1 channel matrix, it is the patch matrix not an image
void classifyEMBinaryHellGauss(Mat means, vector<vector<Mat>> features, Mat &resultantMat, float threshold);

//Classification using EM found Gaussians via region growing from the training samples
//classifiedPatches is the patch matrix not an image
void classifyEMThroughTrainingSamplesHellGauss(Mat means, Mat weights, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold);

//find a cluster closer than threshold according to hellinger distance
int findClusterHellGauss(Mat means, Mat histMat, float threshold);

//find the cluster closest to histMat according to hellinger distance
//if cannot find, returns -1
int findClosestClusterHellGauss(Mat means, Mat histMat, float threshold);


//////////////////// VIA CHI-SQUARE DISTANCE ////////////////////////

////////////////// FOR HISTOGRAMS ////////////////////////////

//Given means of the models find the closest cluster to the sample (feature vector) according to chi-square distance
double findClosestHellDistanceToChiHist(Mat means, Mat feature);

//resultantMat is of type double and 1 channel matrix, it is the patch matrix not an image
void classifyEMBinaryChiHist(Mat means, vector<vector<Mat>> features, Mat &resultantMat, float threshold);

//Classification using EM found Gaussians via region growing from the training samples
//classifiedPatches is the patch matrix not an image
void classifyEMThroughTrainingSamplesChiHist(Mat means, Mat weights, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold);

//find a cluster closer than threshold according to chi-square distance
int findClusterChiHist(Mat means, Mat histMat, float threshold);

//find the cluster closest to histMat according to chi-square distance
//if cannot find, returns -1
int findClosestClusterChiHist(Mat means, Mat histMat, float threshold);

//////////////////////////////////////////////////////////////////////
///////////////////// SVM CLUSTERING OPERATIONS ///////////////////////
//////////////////////////////////////////////////////////////////////

/////////////////////////// MULTI MODEL //////////////////////////////

//////////////////////////////////////////////////////////////////////
///// NOT WORKING WITH OPENCV 2.4.3 SINCE CvSVM COPY CONSTRUCTOR /////
/////////////////// DOES NOT WORKING PROPERLY ////////////////////////
///////////////// WILL IMPLEMENT MY OWN SVN LIB //////////////////////

/*
void classifySVMThroughTrainingSamples(vector<CvSVM> &models, Mat weights, vector<vector<Mat>> featureVec, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches);
*/

/////////////////////////// SINGLE MODEL //////////////////////////////

void classifySingleSVMThroughTrainingSamples(CvSVM &model, vector<vector<Mat>> featureVec, Mat filteredtrPatchesVar, Mat locationTrPatches, Mat &classifiedPatches);

#endif ROADDETECTION_CLASSIFICATIONMETHODS_H__