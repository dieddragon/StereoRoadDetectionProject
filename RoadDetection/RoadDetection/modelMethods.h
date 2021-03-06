#ifndef ROADDETECTION_MODELMETHODS_H__
#define ROADDETECTION_MODELMETHODS_H__

//////////////////////////////////////////////////////////////////////
////////////////////////// MODEL OPERATIONS /////////////////////////
//////////////////////////////////////////////////////////////////////

//Given the weights of the models, returns the index of the model with minimum weight.
int findIndexMinWeight(Mat weights);

//////////////////////////////////////////////////////////////////////
////////////////////////// MATRIX OPERATIONS /////////////////////////
//////////////////////////////////////////////////////////////////////

//Inverts a diagonal matrix. 
//origMat is a matrix of one channel double.
Mat invertDiagonalMatrix(Mat origMat);

//Inverts a vector of diagonal matrices.
//Each matrix is a matrix type of one channel double.
vector<Mat> invertDiagonalMatrixAll(vector<Mat> origMat);

//////////////////////////////////////////////////////////////////////
////////////////////////// MEMORY OPERATIONS /////////////////////////
//////////////////////////////////////////////////////////////////////

//Inserts the training patches filtered to the short term buffer. 
//trPatches should be a matrix of doubles
//hists are the histograms calculated from the patches
//trainingBuffer is the short term training example buffer holds the selected histograms of the training patches
//void insertTrainingPatches(Mat trPatches, vector<vector<MatND>> hists, vector<MatND> trainingBuffer);

//Inserts the training patches filtered of RGB or HSV or HS histograms or gaussians to the short term buffer and insert the location of these pathces into a matrix
//trPatches should be a matrix of doubles
//features are the features calculated from the patches (histograms or gaussians)
//trainingBuffer is the short term training example buffer holds the selected features of the training patches
//location patches should be a matrix of int
void insertTrainingPatches(Mat trPatches, vector<vector<Mat>> features, Mat &buffer, Mat &locationPatches);

//////////////////////////////////////////////////////////////////////
////////// AGGLOMERATIVE CLUSTERING OF SAMPLES OPERATIONS  ///////////
//////////////////////////////////////////////////////////////////////

//Initialize the clusters from the given training samples one cluster for each row (histogram) in the training buffer.
void initializeClusters(vector<Mat> &clusters, Mat trainingBuffer);

//merges clusters i and j into one and put back into the original clusters array.
//clusters array is an array of histograms
void mergeClusters(vector<Mat> &clusters, int i, int j);

//given current clusters, calculate the pseudo-F statistic of the cluster group
float calculatePseudoFScore(vector<Mat> clusters, Mat trainingBuffer);

////////////////////// AVERAGE LINKAGE METHODS ///////////////////////

//Finds the average Euclidian distance between two clusters of samples.
//Each row of cluster matrix holds one sample 
float findAverageDistance(Mat cluster1, Mat cluster2);

//Finds the closest cluster to the cluster k according to the euclidian average distance and returns the found distance
float findClosestClusterAverageLinkage(vector<Mat> clusters, int k, int &l);

//Given a vector of clusters, finds the index of the closest two clusters according to the average linkage criterion with Euclidian distance
void findClosestClusterPairAverageLinkage(vector<Mat> clusters, int &i, int &j);

//hierarchical clustering algorithm using Average linkage distance criterion.
//starts with the clusters equals to the training samples and unifies the clusters until pseudo-F test gives a maximum or there
//remains only one cluster.
int clusterHierarchicalAverageLinkage(vector<Mat> &clusters, Mat trainingBuffer);

////////////////////// COMPLETE LINKAGE METHODS ///////////////////////

//Finds the farthest Euclidian distance between two clusters of samples.
//Each row of cluster matrix holds one sample 
float findCompleteDistance(Mat cluster1, Mat cluster2);

//Finds the closest cluster to the cluster k according to the farthest neighbour and returns the found distance
float findClosestClusterCompleteLinkage(vector<Mat> clusters, int k, int &l);

//Given a vector of clusters, finds the index of the closest two clusters according to the farthest neighbour criterion
//returns the maximum Euclidian distance between the closest cluster pair
float findClosestClusterPairCompleteLinkage(vector<Mat> clusters, int &i, int &j);

//hierarchical clustering algorithm.
//starts with the clusters equals to the training samples and unifies the clusters until the terminal distance criterion
//is met or there left only one cluster. The unifying is done according to the farthest neighbour criterion
int clusterHierarchicalCompleteLinkage(vector<Mat> &clusters, Mat trainingBuffer);

////////////////////// SINGLE LINKAGE METHODS ///////////////////////

//Finds the closest Euclidian distance between two clusters of samples.
//Each row of cluster matrix holds one sample 
float findSingleDistance(Mat cluster1, Mat cluster2);

//Finds the closest cluster to the cluster k according to the closest neighbour and returns the found distance
float findClosestClusterSingleLinkage(vector<Mat> clusters, int k, int &l);

//Given a vector of clusters, finds the index of the closest two clusters according to the closest neighbour criterion
//returns the minimum Euclidian distance between the closest cluster pair
float findClosestClusterPairSingleLinkage(vector<Mat> clusters, int &i, int &j);

//hierarchical clustering algorithm using Single linkage distance criterion.
//starts with the clusters equals to the training samples and unifies the clusters until pseudo-F test gives a maximum or there
//remains only one cluster.
int clusterHierarchicalSingleLinkage(vector<Mat> &clusters, Mat trainingBuffer);



//////////////////////////////////////////////////////////////////////
/////////////////// K-MEANS CLUSTERING OPERATIONS ////////////////////
//////////////////////////////////////////////////////////////////////

//Clusters using kMeans clustering algorithm of OpenCV
void clusterKMeans(Mat trainingBuffer, int nClusters, Mat &labels, Mat &centers);

//////////////////////////////////////////////////////////////////////
///////////////////// EM CLUSTERING OPERATIONS ///////////////////////
//////////////////////////////////////////////////////////////////////

//Clustering using Expectation Maximization algorithm
//Takes training buffer of histograms 1xd matrices
//Takes number of Gaussians to be found
//means, covs and weights should not be initialized
bool clusterEM(Mat trainingBuffer, int nGaussians, Mat &means, vector<Mat> &covs, Mat &weights);

//removes the k clusters (or gaussians) with the smallest weights from the model library but does not update the weights, only remove
void removekClusterswSmallestWeightCorrected(Mat &means, vector<Mat> &covs, vector<Mat> &covsInv, Mat &weights, int k);

//removes the k clusters (or gaussians) with the smallest weights from the model library
void removekClusterswSmallestWeight(Mat &means, vector<Mat> &covs, vector<Mat> &covsInv, Mat &weights, int k);

//inserts newly learned clusters to the model library
void insertClusters2LibEMCorrected(Mat &means, vector<Mat> &covs, vector<Mat> &covsInv, Mat &weights, Mat meansNew, vector<Mat> covsNew, Mat weightNew);

//inserts newly learned clusters to the model library
void insertClusters2LibEM(Mat &means, vector<Mat> &covs, vector<Mat> &covsInv, Mat &weights, Mat meansNew, vector<Mat> covsNew, Mat weightNew);

////////////////// VIA MAHALANOBIS DISTANCE ////////////////////

//updates the EM clusters according to the threshold given and returns remaining training samples.
//distance is calculated via Mahalanobis distance
void updateClustersEMMahCorrected(Mat &trainingBuffer, float threshold, Mat &means, vector<Mat> &covs, vector<Mat> &covsInv, Mat &weights);

//updates the EM clusters according to the threshold given and returns remaining training samples.
void updateClustersEMMah(Mat &trainingBuffer, float threshold, Mat &means, vector<Mat> &covs, vector<Mat> &covsInv, Mat &weights);

////////////////// VIA EUCLIDIAN DISTANCE ////////////////////

//updates the EM clusters according to the threshold given and returns remaining training samples.
//distance is calculated via Euclidian distance
void updateClustersEMEuc(Mat &trainingBuffer, float threshold, Mat &means, Mat &weights);

////////////////// VIA MANHATTAN DISTANCE ////////////////////

//updates the EM clusters according to the threshold given and returns remaining training samples.
//distance is calculated via Manhattan distance
void updateClustersEMMan(Mat &trainingBuffer, float threshold, Mat &means, Mat &weights);

////////////////// VIA CHEBYSHEV DISTANCE ////////////////////

//updates the EM clusters according to the threshold given and returns remaining training samples.
//distance is calculated via Chebyshev distance
void updateClustersEMCheb(Mat &trainingBuffer, float threshold, Mat &means, Mat &weights);

////////////////// VIA HELLINGER DISTANCE ////////////////////

//updates the EM clusters according to the threshold given and returns remaining training samples.
//distance is calculated via Hellinger distance
void updateClustersEMHell(Mat &trainingBuffer, float threshold, Mat &means, Mat &weights);

////////////////// VIA CHI-SQUARE DISTANCE ////////////////////

////////////////// FOR HISTOGRAMS ////////////////////////////

//updates the EM clusters according to the threshold given and returns remaining training samples.
//distance is calculated via Chi-square distance
void updateClustersEMChi(Mat &trainingBuffer, float threshold, Mat &means, Mat &weights);


//////////////////////////////////////////////////////////////////////
//////////////// ONE-CLASS SVM CLUSTERING OPERATIONS /////////////////
//////////////////////////////////////////////////////////////////////

/////////////////////////// MULTI MODEL //////////////////////////////

//////////////////////////////////////////////////////////////////////
///// NOT WORKING WITH OPENCV 2.4.3 SINCE CvSVM COPY CONSTRUCTOR /////
/////////////////// DOES NOT WORKING PROPERLY ////////////////////////
///////////////// WILL IMPLEMENT MY OWN SVN LIB //////////////////////

/*
//Clustering using One-Class SVMs
//Takes training buffer
//means and weights should not be initialized
bool clusterSVM(vector<Mat> trainingClusters, vector<CvSVM> &models, Mat &weights, CvSVMParams params);


void updateClustersSVM(Mat &trainingBuffer, vector<CvSVM> &models, Mat &weights);

void insertClusters2LibSVM(vector<CvSVM> &models, Mat &weights, vector<CvSVM> modelsNew, Mat weightsNew);

//removes the k clusters with the smallest weights from the model library
void removekClusterswSmallestWeightSVM(vector<CvSVM> &models, Mat &weights, int k);
*/

/////////////////////////// SINGLE MODEL //////////////////////////////

//Clustering using One-Class SVM
//Takes training samples library 
bool clusterSVMSingle(vector<Mat> trainingBufferLib, CvSVM &model, CvSVMParams params);

void removeOldSamplesFromLibSVMSingle(vector<Mat> &trainingBufferLib);

#endif ROADDETECTION_MODELMETHODS_H__