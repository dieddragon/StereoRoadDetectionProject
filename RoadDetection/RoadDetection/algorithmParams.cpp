#include "main.h"

//Algorithm parameters
typeDistribution distribution = HS_HIST;
//histogram equalization stuff
bool isHistEqualizationBeforeHSV = true;
bool isHistEqualizationActive = true;
//Value channel band-pass parameters
bool isBandPassActive = false;
int thresholdSatBandMin = 2;
int thresholdValBandMin = 25;
int thresholdValBandMax = 230;
//size of the images
int width = 640;
int height = 480;
//number of pixels in one dimension of the square patch (n)
int size_patch = 5; 
//Histogram Params
//number of bins
int n_bins = 2; //number of bins in each histogram of image patches for each channel (m)
//int n_bins_hue = 2; //number of bins in each histogram of image patches for hue (m)
//total size of histograms
int size_hist[] = {n_bins, n_bins, n_bins};
int size_hist_hs[] = {n_bins, n_bins};
//value ranges
float range[] = {0, 256};	//range for r,g,b and sat channels
float range_hue[] = {0, 180};	//range for hue channel
//total range arrays
const float* ranges[] = { range, range, range }; //overall range array for rgb color space
const float* ranges_hs[] = { range_hue, range };	//overall range array for only hs channels of hsv color space
//channels to be used
int channels[] = {0, 1, 2};	//channels to be used in rgb histogram
int channels_hs[] = { 0, 1 };	//channels to be used in hs histogram
//Short term memory (memory for training samples gathered)
//number of minimum samples for training (except for the start) (N)
int n_samples_min = 20;
//threshold for variance of Y coordinate in patch
int thresholdVarY = 3;		//variable to hold the threshold value
double thresholdVarYd = 0.00003;		//variable to hold the threshold value
int threshold_var_y_max = 100;	//maximum allowable threshold value
int threshold_var_y_divider = 100000;	//divider to change from int to double
//int threshold_var_y_divider = 10000000;	//divider to change from int to double
//threshold for difference of Y coordinate in patch
int thresholdDiffY = 9;		//variable to hold the threshold value
double thresholdDiffYd = 0.09;		//variable to hold the threshold value
int threshold_diff_y_max = 100;	//maximum allowable threshold value
int threshold_diff_y_divider = 100;	//divider to change from int to double
//hierarchical clustering
bool isHierarchicalClustering = false;
//One-Class SVM
double gamma = 50;
double nu = 0.1;
CvSVMParams paramsSVM = CvSVMParams::CvSVMParams();
//max number of clusters
int n_clusters_max = 5;	//(K)
int n_clusters_initial = 2;	//(k0)
int n_clusters_new = 1;	//(k)
//threshold for max mahalanobis distance to accept as a road (unit is the variance (square of std dev))
float threshold_update_mahalanobis = 1;	//(Tm)
float threshold_classify_mahalanobis = 20;	//(Tc)