#ifndef ROADDETECTIONGPU_ALGORITHMPARAMS_H__
#define ROADDETECTIONGPU_ALGORITHMPARAMS_H__

//Algorithm parameters
//public enum class typeDistribution : int { RGB_GAUSS, HS_GAUSS, RGB_HIST, HS_HIST }; 
//enum typeDistribution { RGB_GAUSS, HS_GAUSS, RGB_HIST, HS_HIST };
//extern typeDistribution distribution;
//filtering stuff
//extern bool isHistEqualizationBeforeHSV;
//extern bool isHistEqualizationActive;
//size of the images
//extern int width;
//extern int height;
//number of pixels in one dimension of the square patch (n)
//extern int size_patch; 
//Histogram Params
//number of bins
//extern int n_bins; //number of bins in each histogram of image patches for each channel (m)
//extern int n_bins_hue; //number of bins in each histogram of image patches for hue (m)
//total size of histograms
//extern int size_hist[];
//extern int size_hist_hs[];
//value ranges
//extern float range[];	//range for r,g,b and sat channels
//extern float range_hue[];	//range for hue channel
//total range arrays
//extern const float* ranges[]; //overall range array for rgb color space
//extern const float* ranges_hs[];	//overall range array for only hs channels of hsv color space
//channels to be used
//extern int channels[];	//channels to be used in rgb histogram
//extern int channels_hs[];	//channels to be used in hs histogram
//Short term memory (memory for training samples gathered)
//number of minimum samples for training (except for the start) (N)
//extern int n_samples_min;
//threshold for variance of Y coordinate in patch
//extern int thresholdVarY;		//variable to hold the threshold value
//extern double thresholdVarYd;		//variable to hold the threshold value
//extern int threshold_var_y_max;	//maximum allowable threshold value
//extern int threshold_var_y_divider;	//divider to change from int to double
//threshold for difference of Y coordinate in patch
//extern int thresholdDiffY;		//variable to hold the threshold value
//extern double thresholdDiffYd;		//variable to hold the threshold value
//extern int threshold_diff_y_max;	//maximum allowable threshold value
//extern int threshold_diff_y_divider;	//divider to change from int to double
//hierarchical clustering
//extern bool isHierarchicalClustering;
//One-Class SVM
//extern double gamma;
//extern double nu;
//extern CvSVMParams paramsSVM;
//max number of clusters
//extern int n_clusters_max;	//(K)
//extern int n_clusters_initial;	//(k0)
//extern int n_clusters_new;	//(k)
//threshold for max mahalanobis distance to accept as a road (unit is the variance (square of std dev))
//extern float threshold_update_mahalanobis;	//(Tm)
//extern float threshold_classify_mahalanobis;	//(Tc)
//GPU Params

//for prefix sum scan (kernel_prefix)
#define NUM_BANKS 32		//16 for compute capability < 2, 32 for compute capability >= 2
#define LOG_NUM_BANKS 5		//4 for compute capability < 2, 5 for compute capability >= 2
#ifdef ZERO_BANK_CONFLICTS 
#define CONFLICT_FREE_OFFSET(n) \ 
 ((n) >> NUM_BANKS + (n) >> (2 * LOG_NUM_BANKS)) 
#else 
#define CONFLICT_FREE_OFFSET(n) ((n) >> LOG_NUM_BANKS) 
#endif

//for invCov
const float INF =  10000000000.0f;
const float COV_MIN = 0.000001f;

//image size
const int WIDTH = 640;
const int HEIGHT = 480;

//patch size
const int SIZE_PATCH = 5;

//for in patch histogram calculations (kernel_downsample)
const int SIZE_HIST_IMG = 256;
const int N_BINS = 8;
const int N_BINS_SQR = 64;

//model parameters
//const int N_SAMPLES_MIN = 50;
const int N_CLUSTERS_INITIAL = 1;  //number of models to be learned in the first cycle (k0)
const int N_CLUSTERS_NEW = 1;	//number of models to be learned in each cycle (k)
const int N_CLUSTERS_MAX = 55;  //number of maximum models to be stored (K)

//for patch variance thresholding
//const float THRESHOLD_VAR_Y = 0.00001f;	//threshold for size_patch 8
const float THRESHOLD_VAR_Y = 0.00001f;	//threshold for size_patch 4

//Model related thresholds
const float THRESHOLD_UPDATE_MAH = 1;	// (Tm)
const float THRESHOLD_CLASSIFY_MAH = 25;	// (Tc)

#endif ROADDETECTIONGPU_ALGORITHMPARAMS_H__