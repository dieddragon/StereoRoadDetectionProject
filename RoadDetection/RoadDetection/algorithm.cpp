#include "main.h"

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
Mat detectRoadHistHS(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, Mat &means, vector<Mat> &covs, Mat &weights, double thresholdVarYd, bool isFirstFrame)
{
	Mat hsvMat;
	Mat varPatches;
	vector<vector<Mat>> histsHS;
	Mat trPatchesVar;	//Matrix composed of Y variance of each training patches 
	Mat filteredtrPatchesVar;	//Filtered (biggest blob) matrix composed of Y variance of each training patches
	Mat locationTrPatches;		//Matrix holding the location of the training samples in the (row and column) patch matrix
	Mat classifiedBinaryMat;
	Mat classifiedPatches;
	Mat classifiedMat;

	vector<Mat> covsInv;		//vector of matrices holding the inverse covariance materices of the Gaussian models
	Mat empty;	//An empty matrix

	int ntr = 0;		//number of training patches obtained in current frame

	//check the height variance threshold
	thresholdVarYd = (thresholdVarYd != 0) ? thresholdVarYd : 0.00001;
	if (debug) cout << "Starting road recognition..." << endl;
	
	//////////////////////////////////////////////////////////////////
	///////////////////// PRE-PROCESSING /////////////////////////////
	//////////////////////////////////////////////////////////////////
	
	//Do histogram equalization before hsv
	if (isHistEqualizationActive && isHistEqualizationBeforeHSV)
	{
		vector<Mat> bgr_planes;
		split( imgMat, bgr_planes );
		for (int i = 0; i < bgr_planes.size(); i++)
			equalizeHist(bgr_planes[i], bgr_planes[i]);
		merge( bgr_planes, imgMat);
	}

	cvtColor(imgMat, hsvMat, CV_BGR2HSV);	//Convert the image from bgr to hsv

	//Do histogram equalization after hsv
	if (isHistEqualizationActive && !isHistEqualizationBeforeHSV)
	{
		vector<Mat> hsv_planes;
		split( hsvMat, hsv_planes );
		for (int i = 0; i < (hsv_planes.size() - 1); i++)
			equalizeHist(hsv_planes[i], hsv_planes[i]);
		merge( hsv_planes, hsvMat);
	}

	varPatches = calculateVarianceYforPatches(xyzMat, size_patch);	//Calculate the variance of Y in each patch
	histsHS = calculateHistograms(hsvMat, size_patch, channels_hs, 2, size_hist_hs, ranges_hs, false);	//Calculate image patch histograms
		
	//convert the variance threshold value obtained from the trackbar to double
	//thresholdVarYd = ((double)thresholdVarY)/threshold_var_y_divider;

	if (debug) cout << "Thresholding the variance patches..." << endl;
	trPatchesVar = thresholdPatches1Chi(varPatches, thresholdVarYd);

	//check whether training patches are obtained in that frame
	if (trPatchesVar.rows == 0)
	{
		if (debug) cout << "No training samples are obtained, returning..." << endl;
		return imgMat;
	}

	if (debug) cout << "The biggest area of patches are being filtered..." << endl;
	filteredtrPatchesVar = filterBiggestBlob(trPatchesVar); 

	//insert the training patches to the short-term memory and get the locations 
	if (debug) cout << "From extracted patches, the histograms to be used in the training are being inserted to the training buffer..." << endl;
	insertTrainingPatches(filteredtrPatchesVar, histsHS, trainingBuffer, locationTrPatches);

	//////////////////////////////////////////////////////////////////
	////////////////// MODEL LEARNING AND UPDATING ///////////////////
	//////////////////////////////////////////////////////////////////

	if (isFirstFrame)	//If it is the first frame in this algorithm, you have to learn!
	{
		if (isHierarchicalClustering)
		{
			vector<Mat> clusters;
			Mat dummyTrainingBuffer;
			//Find the optimal number of clusters from the data
			n_clusters_initial = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
			for (int i = 0; i < clusters.size(); i++)
				cout << "Cluster " << i << " : " << clusters[i].size() << endl;
			trainingBuffer = empty;
			//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
			//Else learn it
			for (int i = 0; i < clusters.size(); i++)
				if (clusters[i].rows < n_samples_min)
				{
					dummyTrainingBuffer.push_back(clusters[i]);
					n_clusters_initial--;
				}
				else
					trainingBuffer.push_back(clusters[i]);	
			cout << "N Training Buffer : " << trainingBuffer.rows << endl;
			cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
			if (n_clusters_initial > 0)
			{
				//Find Gaussians using EM and all training samples
				clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
				//cout << means.rows << endl;
				covsInv = invertDiagonalMatrixAll(covs);
			}
			trainingBuffer = dummyTrainingBuffer;	//pass remaining samples to the next frame
		}
		else
		{
				//Find Gaussians using EM and all training samples
				clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
				//cout << means.rows << endl;
				covsInv = invertDiagonalMatrixAll(covs);
				trainingBuffer = empty;		//empty the training buffer
		}
	}
	else
	{
		int nTrainingSamplesOld = trainingBuffer.rows;	//take the number of training samples before the update of models
		covsInv = invertDiagonalMatrixAll(covs);	//invert the covariance matrices
		updateClustersEMMah(trainingBuffer, threshold_update_mahalanobis, means, covs, covsInv, weights);	//update clusters (models) found with EM
		
		//int nTrainingSamplesCurr = trainingBuffer.rows;	//take the number of training samples after the update of models
		if (debug) cout << "Remaining training samples : " << trainingBuffer.rows << endl;
		
		//Parameters of the new models to learn
		Mat meansNew, weightsNew;
		vector<Mat> covsNew;


		if (isHierarchicalClustering)
		{
			vector<Mat> clusters;
			Mat dummyTrainingBuffer;
			//Find the optimal number of clusters from the data
			n_clusters_new = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
			for (int i = 0; i < clusters.size(); i++)
				cout << "Cluster " << i << " : " << clusters[i].size() << endl;
			trainingBuffer = empty;
			//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
			//Else learn it
			for (int i = 0; i < clusters.size(); i++)
				if (clusters[i].rows < n_samples_min)
				{
					dummyTrainingBuffer.push_back(clusters[i]);
					n_clusters_new--;
				}
				else
					trainingBuffer.push_back(clusters[i]);
	
			cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
			cout << "N Training Buffer : " << trainingBuffer.rows << endl;

			if (n_clusters_new > 0)
			{
				int nTrainingSamplesCurr = trainingBuffer.rows;
		
				double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
				clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
				weightsNew = weightsNew * weightNew;		//update the weights
				insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
			}
		
				trainingBuffer = dummyTrainingBuffer;		//pass remaining samples to the next frame
		}
		else
		{
			int nTrainingSamplesCurr = trainingBuffer.rows;

			if (nTrainingSamplesCurr > n_samples_min)	//if the remaining training samples are greater than N
			{
				//Parameters of the new models to learn
				//Mat meansNew, weightsNew;
				//vector<Mat> covsNew;

				double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
				clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
				weightsNew = weightsNew * weightNew;		//update the weights
				insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
				trainingBuffer = empty;		//empty the training sample buffer
			}	
		}
		//cout << means.rows << endl;
		
		//if (nTrainingSamplesCurr > n_samples_min)	//if the remaining training samples are greater than N
		//{
		//	//Parameters of the new models to learn
		//	Mat meansNew, weightsNew;
		//	vector<Mat> covsNew;
//
//			double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
//			clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
//			weightsNew = weightsNew * weightNew;		//update the weights
//			insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
//			trainingBuffer = empty;		//empty the training sample buffer
//		}	
	}
	if (debug) cout << "Number of clusters = " << means.rows << endl;

	//////////////////////////////////////////////////////////////////
	///////////////////// CLASSIFICATION /////////////////////////////
	//////////////////////////////////////////////////////////////////

	//classify
	if (debug) cout << "Classifying the current image..." << endl;
	//classifiedBinaryMat = Mat::zeros(histsHS.size(),histsHS[0].size(),CV_64FC1);
	classifyEMThroughTrainingSamplesMah(means, covsInv, weights, histsHS, filteredtrPatchesVar, locationTrPatches, classifiedPatches, threshold_classify_mahalanobis);	
	classifiedMat = transformPatches2Color3Chi(classifiedPatches, imgMat, size_patch);
	return classifiedMat;
}


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
Mat detectRoadHistBGR(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, Mat &means, vector<Mat> &covs, Mat &weights, double thresholdVarYd, bool isFirstFrame)
{
	Mat varPatches;
	vector<vector<MatND>> hists;
	Mat trPatchesVar;	//Matrix composed of Y variance of each training patches 
	Mat filteredtrPatchesVar;	//Filtered (biggest blob) matrix composed of Y variance of each training patches
	Mat locationTrPatches;		//Matrix holding the location of the training samples in the (row and column) patch matrix
	Mat classifiedBinaryMat;
	Mat classifiedPatches;
	Mat classifiedMat;

	vector<Mat> covsInv;		//vector of matrices holding the inverse covariance materices of the Gaussian models
	Mat empty;	//An empty matrix

	int ntr = 0;		//number of training patches obtained in current frame

	//check the height variance threshold
	thresholdVarYd = (thresholdVarYd != 0) ? thresholdVarYd : 0.00001;
	
	//////////////////////////////////////////////////////////////////
	///////////////////// PRE-PROCESSING /////////////////////////////
	//////////////////////////////////////////////////////////////////
	
	//Do histogram equalization
	if (isHistEqualizationActive)
	{
		vector<Mat> bgr_planes;
		split( imgMat, bgr_planes );
		for (int i = 0; i < bgr_planes.size(); i++)
			equalizeHist(bgr_planes[i], bgr_planes[i]);
		merge( bgr_planes, imgMat);
	}

	varPatches = calculateVarianceYforPatches(xyzMat, size_patch);	//Calculate the variance of Y in each patch
	hists = calculateHistograms(imgMat, size_patch, channels, 3, size_hist, ranges, false);	//Calculate image patch histograms
	//convert the variance threshold value obtained from the trackbar to double
	//thresholdVarYd = ((double)thresholdVarY)/threshold_var_y_divider;

	if (debug) cout << "Thresholding the variance patches..." << endl;
	trPatchesVar = thresholdPatches1Chi(varPatches, thresholdVarYd);

	//check whether training patches are obtained in that frame
	if (trPatchesVar.rows == 0)
	{
		if (debug) cout << "No training samples are obtained, returning..." << endl;
		return imgMat; //bgrtrMatVar;
	}

	if (debug) cout << "The biggest area of patches are being filtered..." << endl;
	filteredtrPatchesVar = filterBiggestBlob(trPatchesVar); 

	//insert the training patches to the short-term memory and get the locations 
	if (debug) cout << "From extracted patches, the histograms to be used in the training are being inserted to the training buffer..." << endl;
	insertTrainingPatches(filteredtrPatchesVar, hists, trainingBuffer, locationTrPatches);

	//////////////////////////////////////////////////////////////////
	////////////////// MODEL LEARNING AND UPDATING ///////////////////
	//////////////////////////////////////////////////////////////////

	if (isFirstFrame)	//If it is the first frame in this algorithm, you have to learn!
	{
		if (isHierarchicalClustering)
		{
			vector<Mat> clusters;
			Mat dummyTrainingBuffer;
			//Find the optimal number of clusters from the data
			n_clusters_initial = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
			for (int i = 0; i < clusters.size(); i++)
				cout << "Cluster " << i << " : " << clusters[i].size() << endl;
			trainingBuffer = empty;
			//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
			//Else learn it
			for (int i = 0; i < clusters.size(); i++)
				if (clusters[i].rows < n_samples_min)
				{
					dummyTrainingBuffer.push_back(clusters[i]);
					n_clusters_initial--;
				}
				else
					trainingBuffer.push_back(clusters[i]);	
			cout << "N Training Buffer : " << trainingBuffer.rows << endl;
			cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
			if (n_clusters_initial > 0)
			{
				//Find Gaussians using EM and all training samples
				clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
				//cout << means.rows << endl;
				covsInv = invertDiagonalMatrixAll(covs);
			}
			trainingBuffer = dummyTrainingBuffer;	//pass remaining samples to the next frame
		}
		else
		{
				//Find Gaussians using EM and all training samples
				clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
				//cout << means.rows << endl;
				covsInv = invertDiagonalMatrixAll(covs);
				trainingBuffer = empty;		//empty the training buffer
		}
	}
	else
	{
		int nTrainingSamplesOld = trainingBuffer.rows;	//take the number of training samples before the update of models
		covsInv = invertDiagonalMatrixAll(covs);	//invert the covariance matrices
		updateClustersEMMah(trainingBuffer, threshold_update_mahalanobis, means, covs, covsInv, weights);	//update clusters (models) found with EM
		
		//int nTrainingSamplesCurr = trainingBuffer.rows;	//take the number of training samples after the update of models
		if (debug) cout << "Remaining training samples : " << trainingBuffer.rows << endl;
		
		//Parameters of the new models to learn
		Mat meansNew, weightsNew;
		vector<Mat> covsNew;


		if (isHierarchicalClustering)
		{
			vector<Mat> clusters;
			Mat dummyTrainingBuffer;
			//Find the optimal number of clusters from the data
			n_clusters_new = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
			for (int i = 0; i < clusters.size(); i++)
				cout << "Cluster " << i << " : " << clusters[i].size() << endl;
			trainingBuffer = empty;
			//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
			//Else learn it
			for (int i = 0; i < clusters.size(); i++)
				if (clusters[i].rows < n_samples_min)
				{
					dummyTrainingBuffer.push_back(clusters[i]);
					n_clusters_new--;
				}
				else
					trainingBuffer.push_back(clusters[i]);
	
			cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
			cout << "N Training Buffer : " << trainingBuffer.rows << endl;

			if (n_clusters_new > 0)
			{
				int nTrainingSamplesCurr = trainingBuffer.rows;
		
				double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
				clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
				weightsNew = weightsNew * weightNew;		//update the weights
				insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
			}
		
				trainingBuffer = dummyTrainingBuffer;		//pass remaining samples to the next frame
		}
		else
		{
			int nTrainingSamplesCurr = trainingBuffer.rows;

			if (nTrainingSamplesCurr > n_samples_min)	//if the remaining training samples are greater than N
			{
				//Parameters of the new models to learn
				//Mat meansNew, weightsNew;
				//vector<Mat> covsNew;

				double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
				clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
				weightsNew = weightsNew * weightNew;		//update the weights
				insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
				trainingBuffer = empty;		//empty the training sample buffer
			}	
		}
		//cout << means.rows << endl;
		
		//if (nTrainingSamplesCurr > n_samples_min)	//if the remaining training samples are greater than N
		//{
		//	//Parameters of the new models to learn
		//	Mat meansNew, weightsNew;
		//	vector<Mat> covsNew;
//
//			double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
//			clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
//			weightsNew = weightsNew * weightNew;		//update the weights
//			insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
//			trainingBuffer = empty;		//empty the training sample buffer
//		}	
	}
	if (debug) cout << "Number of clusters = " << means.rows << endl;

	//////////////////////////////////////////////////////////////////
	///////////////////// CLASSIFICATION /////////////////////////////
	//////////////////////////////////////////////////////////////////

	if (means.rows > 0)
	{
		//classify
		if (debug) cout << "Classifying the current image..." << endl;
		//classifiedBinaryMat = Mat::zeros(hists.size(),hists[0].size(),CV_64FC1);
		classifyEMThroughTrainingSamplesMah(means, covsInv, weights, hists, filteredtrPatchesVar, locationTrPatches, classifiedPatches, threshold_classify_mahalanobis);
	}
	classifiedMat = transformPatches2Color3Chi(classifiedPatches, imgMat, size_patch);
	return classifiedMat;
}

//road detection function. Takes original bgr image and xyz point cloud matrices and classify the road regions in the image
//returns the classified image as road and non-road regions overlaid on the original image
//road regions are green, non red regions are reddish
//imgMat is a matrix of 3-channels uchars
//xyzMat is a matrix of 3-channels doubles
//trainingBuffer is a matrix that holds training samples (RGB values)
//means is a matrix of 1-channel double that holds means of the Gaussians
//covs is a vector of 1-channel double matrices that holds diagonal covariance matrices of the Gaussians
//weights is a matrix of 1-channel double that holds weight of the Gaussians
//thresholdVarYd is the height variance threshold to be used in the training sample generation
//isFirstFrame is a bool to indicate whether the system is just started or not
Mat detectRoadHS(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, Mat &means, vector<Mat> &covs, Mat &weights, double thresholdVarYd, bool isFirstFrame)
{
	Mat hsvMat;
	Mat varPatches;
	vector<vector<Mat>> gaussHS;		//For each patch BGR distribution is modeled as a 3D gaussian as 3 means and 3x3 covariances
	Mat trPatchesVar;	//Matrix composed of Y variance of each training patches 
	Mat filteredtrPatchesVar;	//Filtered (biggest blob) matrix composed of Y variance of each training patches
	Mat locationTrPatches;		//Matrix holding the location of the training samples in the (row and column) patch matrix
	Mat classifiedBinaryMat;
	Mat classifiedPatches;
	Mat classifiedMat;

	vector<Mat> covsInv;		//vector of matrices holding the inverse covariance materices of the Gaussian models
	Mat empty;	//An empty matrix

	int ntr = 0;		//number of training patches obtained in current frame

	//check the height variance threshold
	thresholdVarYd = (thresholdVarYd != 0) ? thresholdVarYd : 0.00001;
	
	//////////////////////////////////////////////////////////////////
	///////////////////// PRE-PROCESSING /////////////////////////////
	//////////////////////////////////////////////////////////////////
	
	//Do histogram equalization before hsv
	if (isHistEqualizationActive && isHistEqualizationBeforeHSV)
	{
		vector<Mat> bgr_planes;
		split( imgMat, bgr_planes );
		for (int i = 0; i < bgr_planes.size(); i++)
			equalizeHist(bgr_planes[i], bgr_planes[i]);
		merge( bgr_planes, imgMat);
	}

	cvtColor(imgMat, hsvMat, CV_BGR2HSV);	//Convert the image from bgr to hsv

	//Do histogram equalization after hsv
	if (isHistEqualizationActive && !isHistEqualizationBeforeHSV)
	{
		vector<Mat> hsv_planes;
		split( hsvMat, hsv_planes );
		for (int i = 0; i < (hsv_planes.size() - 1); i++)
			equalizeHist(hsv_planes[i], hsv_planes[i]);
		merge( hsv_planes, hsvMat);
	}

	varPatches = calculateVarianceYforPatches(xyzMat, size_patch);	//Calculate the variance of Y in each patch
	gaussHS = calculateGaussiansHS2D(hsvMat, size_patch);	//Calculate image patch BGR gaussians
		
	if (debug) cout << "Thresholding the variance patches..." << endl;
	trPatchesVar = thresholdPatches1Chi(varPatches, thresholdVarYd);

	//check whether training patches are obtained in that frame
	if (trPatchesVar.rows == 0)
	{
		if (debug) cout << "No training samples are obtained, returning..." << endl;
		return imgMat;
	}

	if (debug) cout << "The biggest area of patches are being filtered..." << endl;
	filteredtrPatchesVar = filterBiggestBlob(trPatchesVar); 

	//insert the training patches to the short-term memory and get the locations 
	if (debug) cout << "From extracted patches, the histograms to be used in the training are being inserted to the training buffer..." << endl;
	insertTrainingPatches(filteredtrPatchesVar, gaussHS, trainingBuffer, locationTrPatches);

	//////////////////////////////////////////////////////////////////
	////////////////// MODEL LEARNING AND UPDATING ///////////////////
	//////////////////////////////////////////////////////////////////

	if (isFirstFrame)	//If it is the first frame in this algorithm, you have to learn!
	{
		if (isHierarchicalClustering)
		{
			vector<Mat> clusters;
			Mat dummyTrainingBuffer;
			//Find the optimal number of clusters from the data
			n_clusters_initial = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
			for (int i = 0; i < clusters.size(); i++)
				cout << "Cluster " << i << " : " << clusters[i].size() << endl;
			trainingBuffer = empty;
			//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
			//Else learn it
			for (int i = 0; i < clusters.size(); i++)
				if (clusters[i].rows < n_samples_min)
				{
					dummyTrainingBuffer.push_back(clusters[i]);
					n_clusters_initial--;
				}
				else
					trainingBuffer.push_back(clusters[i]);	
			cout << "N Training Buffer : " << trainingBuffer.rows << endl;
			cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
			if (n_clusters_initial > 0)
			{
				//Find Gaussians using EM and all training samples
				clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
				//cout << means.rows << endl;
				covsInv = invertDiagonalMatrixAll(covs);
			}
			trainingBuffer = dummyTrainingBuffer;	//pass remaining samples to the next frame
		}
		else
		{
				//Find Gaussians using EM and all training samples
				clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
				//cout << means.rows << endl;
				covsInv = invertDiagonalMatrixAll(covs);
				trainingBuffer = empty;		//empty the training buffer
		}
	}
	else
	{
		int nTrainingSamplesOld = trainingBuffer.rows;	//take the number of training samples before the update of models
		covsInv = invertDiagonalMatrixAll(covs);	//invert the covariance matrices
		updateClustersEMMah(trainingBuffer, threshold_update_mahalanobis, means, covs, covsInv, weights);	//update clusters (models) found with EM
		
		//int nTrainingSamplesCurr = trainingBuffer.rows;	//take the number of training samples after the update of models
		if (debug) cout << "Remaining training samples : " << trainingBuffer.rows << endl;
		
		//Parameters of the new models to learn
		Mat meansNew, weightsNew;
		vector<Mat> covsNew;


		if (isHierarchicalClustering)
		{
			vector<Mat> clusters;
			Mat dummyTrainingBuffer;
			//Find the optimal number of clusters from the data
			n_clusters_new = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
			for (int i = 0; i < clusters.size(); i++)
				cout << "Cluster " << i << " : " << clusters[i].size() << endl;
			trainingBuffer = empty;
			//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
			//Else learn it
			for (int i = 0; i < clusters.size(); i++)
				if (clusters[i].rows < n_samples_min)
				{
					dummyTrainingBuffer.push_back(clusters[i]);
					n_clusters_new--;
				}
				else
					trainingBuffer.push_back(clusters[i]);
	
			cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
			cout << "N Training Buffer : " << trainingBuffer.rows << endl;

			if (n_clusters_new > 0)
			{
				int nTrainingSamplesCurr = trainingBuffer.rows;
		
				double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
				clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
				weightsNew = weightsNew * weightNew;		//update the weights
				insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
			}
		
				trainingBuffer = dummyTrainingBuffer;		//pass remaining samples to the next frame
		}
		else
		{
			int nTrainingSamplesCurr = trainingBuffer.rows;

			if (nTrainingSamplesCurr > n_samples_min)	//if the remaining training samples are greater than N
			{
				//Parameters of the new models to learn
				//Mat meansNew, weightsNew;
				//vector<Mat> covsNew;

				double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
				clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
				weightsNew = weightsNew * weightNew;		//update the weights
				insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
				trainingBuffer = empty;		//empty the training sample buffer
			}	
		}
		//cout << means.rows << endl;
		
		//if (nTrainingSamplesCurr > n_samples_min)	//if the remaining training samples are greater than N
		//{
		//	//Parameters of the new models to learn
		//	Mat meansNew, weightsNew;
		//	vector<Mat> covsNew;
//
//			double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
//			clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
//			weightsNew = weightsNew * weightNew;		//update the weights
//			insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
//			trainingBuffer = empty;		//empty the training sample buffer
//		}	
	}
	if (debug) cout << "Number of clusters = " << means.rows << endl;

	//////////////////////////////////////////////////////////////////
	///////////////////// CLASSIFICATION /////////////////////////////
	//////////////////////////////////////////////////////////////////

	//classify
	if (debug) cout << "Classifying the current image..." << endl;
	//classifiedBinaryMat = Mat::zeros(gaussHS.size(),gaussHS[0].size(),CV_64FC1);
	classifyEMThroughTrainingSamplesMah(means, covsInv, weights, gaussHS, filteredtrPatchesVar, locationTrPatches, classifiedPatches, threshold_classify_mahalanobis);
	classifiedMat = transformPatches2Color3Chi(classifiedPatches, imgMat, size_patch);
	return classifiedMat;
}

//road detection function. Takes original bgr image and xyz point cloud matrices and classify the road regions in the image
//returns the classified image as road and non-road regions overlaid on the original image
//road regions are green, non red regions are reddish
//imgMat is a matrix of 3-channels uchars
//xyzMat is a matrix of 3-channels doubles
//trainingBuffer is a matrix that holds training samples (RGB values)
//means is a matrix of 1-channel double that holds means of the Gaussians
//covs is a vector of 1-channel double matrices that holds diagonal covariance matrices of the Gaussians
//weights is a matrix of 1-channel double that holds weight of the Gaussians
//thresholdVarYd is the height variance threshold to be used in the training sample generation
//isFirstFrame is a bool to indicate whether the system is just started or not
Mat detectRoadBGR(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, Mat &means, vector<Mat> &covs, Mat &weights, double thresholdVarYd, bool isFirstFrame)
{
	Mat varPatches;
	vector<vector<Mat>> gaussBGR;		//For each patch BGR distribution is modeled as a 3D gaussian as 3 means and 3x3 covariances
	Mat trPatchesVar;	//Matrix composed of Y variance of each training patches 
	Mat filteredtrPatchesVar;	//Filtered (biggest blob) matrix composed of Y variance of each training patches
	Mat locationTrPatches;		//Matrix holding the location of the training samples in the (row and column) patch matrix
	Mat classifiedBinaryMat;
	Mat classifiedPatches;
	Mat classifiedMat;

	vector<Mat> covsInv;		//vector of matrices holding the inverse covariance materices of the Gaussian models
	Mat empty;	//An empty matrix

	//check the height variance threshold
	thresholdVarYd = (thresholdVarYd != 0) ? thresholdVarYd : 0.00001;
	
	//////////////////////////////////////////////////////////////////
	///////////////////// PRE-PROCESSING /////////////////////////////
	//////////////////////////////////////////////////////////////////
	
	//Do histogram equalization
	if (isHistEqualizationActive)
	{
		vector<Mat> bgr_planes;
		split( imgMat, bgr_planes );
		for (int i = 0; i < bgr_planes.size(); i++)
			equalizeHist(bgr_planes[i], bgr_planes[i]);
		merge( bgr_planes, imgMat);
	}

	varPatches = calculateVarianceYforPatches(xyzMat, size_patch);	//Calculate the variance of Y in each patch
	gaussBGR = calculateGaussiansBGR3D(imgMat, size_patch);	//Calculate image patch BGR gaussians
		
	if (debug) cout << "Thresholding the variance patches..." << endl;
	trPatchesVar = thresholdPatches1Chi(varPatches, thresholdVarYd);

	//check whether training patches are obtained in that frame
	if (trPatchesVar.rows == 0)
	{
		if (debug) cout << "No training samples are obtained, returning..." << endl;
		return imgMat;
	}

	if (debug) cout << "The biggest area of patches are being filtered..." << endl;
	filteredtrPatchesVar = filterBiggestBlob(trPatchesVar); 

	//insert the training patches to the short-term memory and get the locations 
	if (debug) cout << "From extracted patches, the histograms to be used in the training are being inserted to the training buffer..." << endl;
	insertTrainingPatches(filteredtrPatchesVar, gaussBGR, trainingBuffer, locationTrPatches);

	//////////////////////////////////////////////////////////////////
	////////////////// MODEL LEARNING AND UPDATING ///////////////////
	//////////////////////////////////////////////////////////////////

	if (isFirstFrame)	//If it is the first frame in this algorithm, you have to learn!
	{
		if (isHierarchicalClustering)
		{
			vector<Mat> clusters;
			Mat dummyTrainingBuffer;
			//Find the optimal number of clusters from the data
			n_clusters_initial = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
			for (int i = 0; i < clusters.size(); i++)
				cout << "Cluster " << i << " : " << clusters[i].size() << endl;
			trainingBuffer = empty;
			//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
			//Else learn it
			for (int i = 0; i < clusters.size(); i++)
				if (clusters[i].rows < n_samples_min)
				{
					dummyTrainingBuffer.push_back(clusters[i]);
					n_clusters_initial--;
				}
				else
					trainingBuffer.push_back(clusters[i]);	
			cout << "N Training Buffer : " << trainingBuffer.rows << endl;
			cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
			if (n_clusters_initial > 0)
			{
				//Find Gaussians using EM and all training samples
				clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
				//cout << means.rows << endl;
				covsInv = invertDiagonalMatrixAll(covs);
			}
			trainingBuffer = dummyTrainingBuffer;	//pass remaining samples to the next frame
		}
		else
		{
				//Find Gaussians using EM and all training samples
				clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
				//cout << means.rows << endl;
				covsInv = invertDiagonalMatrixAll(covs);
				trainingBuffer = empty;		//empty the training buffer
		}
	}
	else
	{
		int nTrainingSamplesOld = trainingBuffer.rows;	//take the number of training samples before the update of models
		covsInv = invertDiagonalMatrixAll(covs);	//invert the covariance matrices
		updateClustersEMMah(trainingBuffer, threshold_update_mahalanobis, means, covs, covsInv, weights);	//update clusters (models) found with EM
		
		//int nTrainingSamplesCurr = trainingBuffer.rows;	//take the number of training samples after the update of models
		if (debug) cout << "Remaining training samples : " << trainingBuffer.rows << endl;
		
		//Parameters of the new models to learn
		Mat meansNew, weightsNew;
		vector<Mat> covsNew;


		if (isHierarchicalClustering)
		{
			vector<Mat> clusters;
			Mat dummyTrainingBuffer;
			//Find the optimal number of clusters from the data
			n_clusters_new = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
			for (int i = 0; i < clusters.size(); i++)
				cout << "Cluster " << i << " : " << clusters[i].size() << endl;
			trainingBuffer = empty;
			//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
			//Else learn it
			for (int i = 0; i < clusters.size(); i++)
				if (clusters[i].rows < n_samples_min)
				{
					dummyTrainingBuffer.push_back(clusters[i]);
					n_clusters_new--;
				}
				else
					trainingBuffer.push_back(clusters[i]);
	
			cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
			cout << "N Training Buffer : " << trainingBuffer.rows << endl;

			if (n_clusters_new > 0)
			{
				int nTrainingSamplesCurr = trainingBuffer.rows;
		
				double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
				clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
				weightsNew = weightsNew * weightNew;		//update the weights
				insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
			}
		
				trainingBuffer = dummyTrainingBuffer;		//pass remaining samples to the next frame
		}
		else
		{
			int nTrainingSamplesCurr = trainingBuffer.rows;

			if (nTrainingSamplesCurr > n_samples_min)	//if the remaining training samples are greater than N
			{
				//Parameters of the new models to learn
				//Mat meansNew, weightsNew;
				//vector<Mat> covsNew;

				double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
				clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
				weightsNew = weightsNew * weightNew;		//update the weights
				insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
				trainingBuffer = empty;		//empty the training sample buffer
			}	
		}
		//cout << means.rows << endl;
		
		//if (nTrainingSamplesCurr > n_samples_min)	//if the remaining training samples are greater than N
		//{
		//	//Parameters of the new models to learn
		//	Mat meansNew, weightsNew;
		//	vector<Mat> covsNew;
//
//			double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
//			clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
//			weightsNew = weightsNew * weightNew;		//update the weights
//			insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
//			trainingBuffer = empty;		//empty the training sample buffer
//		}	
	}
	if (debug) cout << "Number of clusters = " << means.rows << endl;

	//////////////////////////////////////////////////////////////////
	///////////////////// CLASSIFICATION /////////////////////////////
	//////////////////////////////////////////////////////////////////

	//classify
	if (debug) cout << "Classifying the current image..." << endl;
	//classifiedBinaryMat = Mat::zeros(gaussBGR.size(),gaussBGR[0].size(),CV_64FC1);
	classifyEMThroughTrainingSamplesMah(means, covsInv, weights, gaussBGR, filteredtrPatchesVar, locationTrPatches, classifiedPatches, threshold_classify_mahalanobis);
	classifiedMat = transformPatches2Color3Chi(classifiedPatches, imgMat, size_patch);
	return classifiedMat;
}

*/

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
Mat detectRoadBanded(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, Mat &means, vector<Mat> &covs, Mat &weights, double thresholdVarYd, bool isFirstFrame, typeDistribution distributionType)
{	
	if ( distributionType > -1 && distributionType < 4)
	{
		Mat hsvMat;
		Mat varPatches;
		
		//Distribution vector
		vector<vector<Mat>> featureVec;

		Mat trPatchesVar;	//Matrix composed of Y variance of each training patches 
		Mat filteredtrPatchesVar;	//Filtered (biggest blob) matrix composed of Y variance of each training patches
		Mat locationTrPatches;		//Matrix holding the location of the training samples in the (row and column) patch matrix
		Mat classifiedBinaryMat;
		Mat classifiedPatches;
		Mat classifiedMat;

		int ntr = 0;		//number of training patches obtained in current frame

		vector<Mat> covsInv;		//vector of matrices holding the inverse covariance materices of the Gaussian models
		Mat empty;	//An empty matrix

		vector<int> output_params;
		output_params.push_back(CV_IMWRITE_PXM_BINARY);
		output_params.push_back(1);

		//check the height variance threshold
		thresholdVarYd = (thresholdVarYd != 0) ? thresholdVarYd : 0.00001;
	
		//////////////////////////////////////////////////////////////////
		///////////////////// PRE-PROCESSING /////////////////////////////
		//////////////////////////////////////////////////////////////////
	
		//Do histogram equalization
		if (isHistEqualizationActive)
		{
			if (distributionType == RGB_GAUSS || distributionType == RGB_HIST || 
			   ((distributionType == HS_GAUSS || distributionType == HS_HIST) && isHistEqualizationBeforeHSV))
			{
				vector<Mat> bgr_planes;
				split( imgMat, bgr_planes );
				for (int i = 0; i < bgr_planes.size(); i++)
					equalizeHist(bgr_planes[i], bgr_planes[i]);
				merge( bgr_planes, imgMat);
				//imwrite("histeq.ppm", imgMat, output_params);
			}
			if ((distributionType == HS_GAUSS || distributionType == HS_HIST))
				cvtColor(imgMat, hsvMat, CV_BGR2HSV);	//Convert the image from bgr to hsv
			if ((distributionType == HS_GAUSS || distributionType == HS_HIST) && !isHistEqualizationBeforeHSV)
			{
				vector<Mat> hsv_planes;
				split( hsvMat, hsv_planes );
				for (int i = 0; i < (hsv_planes.size() - 1); i++)
					equalizeHist(hsv_planes[i], hsv_planes[i]);
				merge( hsv_planes, hsvMat);
			}
		}
		if (!isHistEqualizationActive && (distributionType == HS_GAUSS || distributionType == HS_HIST))
		{
			cvtColor(imgMat, hsvMat, CV_BGR2HSV);	//Convert the image from bgr to hsv
			if (isBandPassActive)
			{
				//filterValueChannel(xyzMat, hsvMat, thresholdValBandMin, thresholdValBandMax);
				filterSatChannel(xyzMat, hsvMat, thresholdSatBandMin);
			}
		}

		varPatches = calculateVarianceYforPatches(xyzMat, size_patch);	//Calculate the variance of Y in each patch

		//Calculate distributions within the patches to be used as feature vectors
		if (distributionType == HS_HIST)	
			featureVec = calculateHistograms(hsvMat, size_patch, channels_hs, 2, size_hist_hs, ranges_hs, false);	//Calculate image patch histograms
		if (distributionType == RGB_HIST)	
			featureVec = calculateHistograms(imgMat, size_patch, channels, 3, size_hist, ranges, false);	//Calculate image patch histograms
		if (distributionType == HS_GAUSS)	
			featureVec = calculateGaussiansHS2D(hsvMat, size_patch);	//Calculate image patch BGR gaussians
		if (distributionType == RGB_GAUSS)	
			featureVec = calculateGaussiansBGR3D(imgMat, size_patch);	//Calculate image patch BGR gaussians

		if (debug) cout << "Thresholding the variance patches..." << endl;
		trPatchesVar = thresholdPatches1Chi(varPatches, thresholdVarYd, &ntr);

		//check whether training patches are obtained in that frame
		if (ntr < 5)
		{
			if (debug) cout << "No training samples are obtained, returning..." << endl;
			classifiedMat = transformPatches2Color3Chi(Mat::zeros(height/size_patch, width/size_patch, CV_64FC1) , imgMat, size_patch);
			return classifiedMat;
		}

		if (debug) imwrite("trPatchesVar.pgm", trPatchesVar, output_params);

		if (debug) cout << "The biggest area of patches are being filtered..." << endl;
		filteredtrPatchesVar = filterBiggestBlob(trPatchesVar, &ntr); 

		//check whether training patches are obtained in that frame
		if (ntr < 5)
		{
			if (debug) cout << "No training samples are obtained, returning..." << endl;
			classifiedMat = transformPatches2Color3Chi(Mat::zeros(height/size_patch, width/size_patch, CV_64FC1) , imgMat, size_patch);
			return classifiedMat;
		}

		//insert the training patches to the short-term memory and get the locations 
		if (debug) cout << "From extracted patches, the histograms to be used in the training are being inserted to the training buffer..." << endl;
		insertTrainingPatches(filteredtrPatchesVar, featureVec, trainingBuffer, locationTrPatches);
		
		//////////////////////////////////////////////////////////////////
		////////////////// MODEL LEARNING AND UPDATING ///////////////////
		//////////////////////////////////////////////////////////////////

		if (isFirstFrame)	//If it is the first frame in this algorithm, you have to learn!
		{
			if (isHierarchicalClustering)
			{
				vector<Mat> clusters;
				Mat dummyTrainingBuffer;
				//Find the optimal number of clusters from the data
				n_clusters_initial = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
				if (debug)
				{
					for (int i = 0; i < clusters.size(); i++)
						cout << "Cluster " << i << " : " << clusters[i].size() << endl;
				}
				trainingBuffer = empty;
				//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
				//Else learn it
				for (int i = 0; i < clusters.size(); i++)
					if (clusters[i].rows < n_samples_min)
					{
						dummyTrainingBuffer.push_back(clusters[i]);
						n_clusters_initial--;
					}	
					else
						trainingBuffer.push_back(clusters[i]);	
				if (debug) cout << "N Training Buffer : " << trainingBuffer.rows << endl;
				if (debug) cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
				if (n_clusters_initial > 0)
				{
					//Find Gaussians using EM and all training samples
					clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
					//cout << means.rows << endl;
					covsInv = invertDiagonalMatrixAll(covs);
				}
				trainingBuffer = dummyTrainingBuffer;	//pass remaining samples to the next frame
			}
			else
			{
					//Find Gaussians using EM and all training samples
					clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
					//cout << means.rows << endl;
					covsInv = invertDiagonalMatrixAll(covs);
					trainingBuffer = empty;		//empty the training buffer
			}
		}
		else
		{
			int nTrainingSamplesOld = trainingBuffer.rows;	//take the number of training samples before the update of models
			covsInv = invertDiagonalMatrixAll(covs);	//invert the covariance matrices
			updateClustersEMMah(trainingBuffer, threshold_update_mahalanobis, means, covs, covsInv, weights);	//update clusters (models) found with EM
		
			//int nTrainingSamplesCurr = trainingBuffer.rows;	//take the number of training samples after the update of models
			if (debug) cout << "Remaining training samples : " << trainingBuffer.rows << endl;
		
			//Parameters of the new models to learn
			Mat meansNew, weightsNew;
			vector<Mat> covsNew;

			if (isHierarchicalClustering)
			{
				vector<Mat> clusters;
				Mat dummyTrainingBuffer;
				//Find the optimal number of clusters from the data
				n_clusters_new = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
				if (debug)
				{
					for (int i = 0; i < clusters.size(); i++)
						cout << "Cluster " << i << " : " << clusters[i].size() << endl;
				}
				trainingBuffer = empty;
				//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
				//Else learn it
				for (int i = 0; i < clusters.size(); i++)
					if (clusters[i].rows < n_samples_min)
					{
						dummyTrainingBuffer.push_back(clusters[i]);
						n_clusters_new--;
					}
					else
						trainingBuffer.push_back(clusters[i]);
	
				if (debug) cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
				if (debug) cout << "N Training Buffer : " << trainingBuffer.rows << endl;

				if (n_clusters_new > 0)
				{
					int nTrainingSamplesCurr = trainingBuffer.rows;
		
					double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
					clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
					weightsNew = weightsNew * weightNew;		//update the weights
					insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
				}
		
				trainingBuffer = dummyTrainingBuffer;		//pass remaining samples to the next frame
			}
			else
			{
				int nTrainingSamplesCurr = trainingBuffer.rows;

				if (nTrainingSamplesCurr > n_samples_min)	//if the remaining training samples are greater than N
				{
					//Parameters of the new models to learn
					//Mat meansNew, weightsNew;
					//vector<Mat> covsNew;

					double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
					clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
					weightsNew = weightsNew * weightNew;		//update the weights
					insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
					trainingBuffer = empty;		//empty the training sample buffer
				}	
			}
		}
		if (debug) cout << "Number of clusters = " << means.rows << endl;

		//////////////////////////////////////////////////////////////////
		///////////////////// CLASSIFICATION /////////////////////////////
		//////////////////////////////////////////////////////////////////

		if (means.rows > 0)
		{
			//classify
			if (debug) cout << "Classifying the current image..." << endl;
			classifyEMThroughTrainingSamplesMah(means, covsInv, weights, featureVec, filteredtrPatchesVar, locationTrPatches, classifiedPatches, threshold_classify_mahalanobis);
			classifiedMat = transformPatches2Color3Chi(classifiedPatches, imgMat, size_patch);
		}
		else
			classifiedMat = transformPatches2Color3Chi(Mat::zeros(height/size_patch, width/size_patch, CV_64FC1) , imgMat, size_patch);
		return classifiedMat;
	}
	else
		return imgMat;
}



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
Mat detectRoad(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, Mat &means, vector<Mat> &covs, Mat &weights, double thresholdVarYd, bool isFirstFrame, typeDistribution distributionType)
{	
	if ( distributionType > -1 && distributionType < 4)
	{
		Mat hsvMat;
		Mat varPatches;
		
		//Distribution vector
		vector<vector<Mat>> featureVec;

		Mat trPatchesVar;	//Matrix composed of Y variance of each training patches 
		Mat filteredtrPatchesVar;	//Filtered (biggest blob) matrix composed of Y variance of each training patches
		Mat locationTrPatches;		//Matrix holding the location of the training samples in the (row and column) patch matrix
		Mat classifiedBinaryMat;
		Mat classifiedPatches;
		Mat classifiedMat;

		int ntr = 0;		//number of training patches obtained in current frame

		vector<Mat> covsInv;		//vector of matrices holding the inverse covariance materices of the Gaussian models
		Mat empty;	//An empty matrix

		//check the height variance threshold
		thresholdVarYd = (thresholdVarYd != 0) ? thresholdVarYd : 0.00001;
	
		//////////////////////////////////////////////////////////////////
		///////////////////// PRE-PROCESSING /////////////////////////////
		//////////////////////////////////////////////////////////////////
	
		//Do histogram equalization
		if (isHistEqualizationActive)
		{
			if (distributionType == RGB_GAUSS || distributionType == RGB_HIST || 
			   ((distributionType == HS_GAUSS || distributionType == HS_HIST) && isHistEqualizationBeforeHSV))
			{
				vector<Mat> bgr_planes;
				split( imgMat, bgr_planes );
				for (int i = 0; i < bgr_planes.size(); i++)
					equalizeHist(bgr_planes[i], bgr_planes[i]);
				merge( bgr_planes, imgMat);
			}
			if ((distributionType == HS_GAUSS || distributionType == HS_HIST))
				cvtColor(imgMat, hsvMat, CV_BGR2HSV);	//Convert the image from bgr to hsv
			if ((distributionType == HS_GAUSS || distributionType == HS_HIST) && !isHistEqualizationBeforeHSV)
			{
				vector<Mat> hsv_planes;
				split( hsvMat, hsv_planes );
				for (int i = 0; i < (hsv_planes.size() - 1); i++)
					equalizeHist(hsv_planes[i], hsv_planes[i]);
				merge( hsv_planes, hsvMat);
			}
		}
		if (!isHistEqualizationActive && (distributionType == HS_GAUSS || distributionType == HS_HIST))
		{
			cvtColor(imgMat, hsvMat, CV_BGR2HSV);	//Convert the image from bgr to hsv
		}

		varPatches = calculateVarianceYforPatches(xyzMat, size_patch);	//Calculate the variance of Y in each patch

		//Calculate distributions within the patches to be used as feature vectors
		if (distributionType == HS_HIST)	
			featureVec = calculateHistograms(hsvMat, size_patch, channels_hs, 2, size_hist_hs, ranges_hs, false);	//Calculate image patch histograms
		if (distributionType == RGB_HIST)	
			featureVec = calculateHistograms(imgMat, size_patch, channels, 3, size_hist, ranges, false);	//Calculate image patch histograms
		if (distributionType == HS_GAUSS)	
			featureVec = calculateGaussiansHS2D(hsvMat, size_patch);	//Calculate image patch BGR gaussians
		if (distributionType == RGB_GAUSS)	
			featureVec = calculateGaussiansBGR3D(imgMat, size_patch);	//Calculate image patch BGR gaussians

		if (debug) cout << "Thresholding the variance patches..." << endl;
		trPatchesVar = thresholdPatches1Chi(varPatches, thresholdVarYd, &ntr);

		//check whether training patches are obtained in that frame
		if (ntr < 5)
		{
			if (debug) cout << "No training samples are obtained, returning..." << endl;
			return imgMat; //bgrtrMatVar;
		}

		if (debug) cout << "The biggest area of patches are being filtered..." << endl;
		filteredtrPatchesVar = filterBiggestBlob(trPatchesVar, &ntr); 

		//check whether training patches are obtained in that frame
		if (ntr < 5)
		{
			if (debug) cout << "No training samples are obtained, returning..." << endl;
			return imgMat; //bgrtrMatVar;
		}

		//insert the training patches to the short-term memory and get the locations 
		if (debug) cout << "From extracted patches, the histograms to be used in the training are being inserted to the training buffer..." << endl;
		insertTrainingPatches(filteredtrPatchesVar, featureVec, trainingBuffer, locationTrPatches);
		
		//////////////////////////////////////////////////////////////////
		////////////////// MODEL LEARNING AND UPDATING ///////////////////
		//////////////////////////////////////////////////////////////////

		if (isFirstFrame)	//If it is the first frame in this algorithm, you have to learn!
		{
			if (isHierarchicalClustering)
			{
				vector<Mat> clusters;
				Mat dummyTrainingBuffer;
				//Find the optimal number of clusters from the data
				n_clusters_initial = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
				for (int i = 0; i < clusters.size(); i++)
					cout << "Cluster " << i << " : " << clusters[i].size() << endl;
				trainingBuffer = empty;
				//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
				//Else learn it
				for (int i = 0; i < clusters.size(); i++)
					if (clusters[i].rows < n_samples_min)
					{
						dummyTrainingBuffer.push_back(clusters[i]);
						n_clusters_initial--;
					}	
					else
						trainingBuffer.push_back(clusters[i]);	
				cout << "N Training Buffer : " << trainingBuffer.rows << endl;
				cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
				if (n_clusters_initial > 0)
				{
					//Find Gaussians using EM and all training samples
					clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
					//cout << means.rows << endl;
					covsInv = invertDiagonalMatrixAll(covs);
				}
				trainingBuffer = dummyTrainingBuffer;	//pass remaining samples to the next frame
			}
			else
			{
					//Find Gaussians using EM and all training samples
					clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
					//cout << means.rows << endl;
					covsInv = invertDiagonalMatrixAll(covs);
					trainingBuffer = empty;		//empty the training buffer
			}
		}
		else
		{
			int nTrainingSamplesOld = trainingBuffer.rows;	//take the number of training samples before the update of models
			covsInv = invertDiagonalMatrixAll(covs);	//invert the covariance matrices
			//updateClustersEMMah(trainingBuffer, threshold_update_mahalanobis, means, covs, covsInv, weights);	//update clusters (models) found with EM
			updateClustersEMMahCorrected(trainingBuffer, threshold_update_mahalanobis, means, covs, covsInv, weights);	//update clusters (models) found with EM
		
			//int nTrainingSamplesCurr = trainingBuffer.rows;	//take the number of training samples after the update of models
			if (debug) cout << "Remaining training samples : " << trainingBuffer.rows << endl;
		
			//Parameters of the new models to learn
			Mat meansNew, weightsNew;
			vector<Mat> covsNew;

			if (isHierarchicalClustering)
			{
				vector<Mat> clusters;
				Mat dummyTrainingBuffer;
				//Find the optimal number of clusters from the data
				n_clusters_new = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
				for (int i = 0; i < clusters.size(); i++)
					cout << "Cluster " << i << " : " << clusters[i].size() << endl;
				trainingBuffer = empty;
				//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
				//Else learn it
				for (int i = 0; i < clusters.size(); i++)
					if (clusters[i].rows < n_samples_min)
					{
						dummyTrainingBuffer.push_back(clusters[i]);
						n_clusters_new--;
					}
					else
						trainingBuffer.push_back(clusters[i]);
	
				cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
				cout << "N Training Buffer : " << trainingBuffer.rows << endl;

				if (n_clusters_new > 0)
				{
					int nTrainingSamplesCurr = trainingBuffer.rows;
		
					double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
					clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
					//weightsNew = weightsNew * weightNew;		//update the weights
					weightsNew = weightsNew * weightNew / 2;	//update the weights (new implementation with corrected update)
					//insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
					insertClusters2LibEMCorrected(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
				}
		
				trainingBuffer = dummyTrainingBuffer;		//pass remaining samples to the next frame
			}
			else
			{
				int nTrainingSamplesCurr = trainingBuffer.rows;

				if (nTrainingSamplesCurr > n_samples_min)	//if the remaining training samples are greater than N
				{
					//Parameters of the new models to learn
					//Mat meansNew, weightsNew;
					//vector<Mat> covsNew;

					double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
					clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
					//weightsNew = weightsNew * weightNew;		//update the weights (old implementation)
					weightsNew = weightsNew * weightNew / 2;	//update the weights (new implementation with corrected update)
					//insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
					insertClusters2LibEMCorrected(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
					trainingBuffer = empty;		//empty the training sample buffer
				}	
			}
		}
		if (debug) cout << "Number of clusters = " << means.rows << endl;

		//////////////////////////////////////////////////////////////////
		///////////////////// CLASSIFICATION /////////////////////////////
		//////////////////////////////////////////////////////////////////

		if (means.rows > 0)
		{
			//classify
			if (debug) cout << "Classifying the current image..." << endl;
			classifyEMThroughTrainingSamplesMah(means, covsInv, weights, featureVec, filteredtrPatchesVar, locationTrPatches, classifiedPatches, threshold_classify_mahalanobis);
			classifiedMat = transformPatches2Color3Chi(classifiedPatches, imgMat, size_patch);
		}
		else
			classifiedMat = transformPatches2Color3Chi(Mat::zeros(height/size_patch, width/size_patch, CV_64FC1) , imgMat, size_patch);
		return classifiedMat;
	}
	else
		return imgMat;
}






//////////////////////////////////////////////////////////////////////
///// NOT WORKING WITH OPENCV 2.4.3 SINCE CvSVM COPY CONSTRUCTOR /////
/////////////////// DOES NOT WORKING PROPERLY ////////////////////////
///////////////// WILL IMPLEMENT MY OWN SVN LIB //////////////////////

/////////////////////////// MULTI MODEL //////////////////////////////

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
Mat detectRoadSVM(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, vector<CvSVM> &models, Mat &weights, double thresholdVarYd, bool isFirstFrame, typeDistribution distributionType)
{	
	if ( distributionType > -1 && distributionType < 4)
	{
		Mat hsvMat;
		Mat varPatches;
		
		//Distribution vector
		vector<vector<Mat>> featureVec;

		Mat trPatchesVar;	//Matrix composed of Y variance of each training patches 
		Mat filteredtrPatchesVar;	//Filtered (biggest blob) matrix composed of Y variance of each training patches
		Mat locationTrPatches;		//Matrix holding the location of the training samples in the (row and column) patch matrix
		Mat classifiedBinaryMat;
		Mat classifiedPatches;
		Mat classifiedMat;

		vector<Mat> covsInv;		//vector of matrices holding the inverse covariance materices of the Gaussian models
		Mat empty;	//An empty matrix

		paramsSVM.svm_type = CvSVM::ONE_CLASS;
		paramsSVM.kernel_type = CvSVM::RBF;
		paramsSVM.gamma = gamma;
		paramsSVM.nu = nu;
		paramsSVM.term_crit = cvTermCriteria( CV_TERMCRIT_ITER+CV_TERMCRIT_EPS, 100, 0.1 );


		//check the height variance threshold
		thresholdVarYd = (thresholdVarYd != 0) ? thresholdVarYd : 0.00001;
	
		//////////////////////////////////////////////////////////////////
		///////////////////// PRE-PROCESSING /////////////////////////////
		//////////////////////////////////////////////////////////////////
	
		//Do histogram equalization
		if (isHistEqualizationActive)
		{
			if (distributionType == RGB_GAUSS || distributionType == RGB_HIST || 
			   ((distributionType == HS_GAUSS || distributionType == HS_HIST) && isHistEqualizationBeforeHSV))
			{
				vector<Mat> bgr_planes;
				split( imgMat, bgr_planes );
				for (int i = 0; i < bgr_planes.size(); i++)
					equalizeHist(bgr_planes[i], bgr_planes[i]);
				merge( bgr_planes, imgMat);
			}
			if ((distributionType == HS_GAUSS || distributionType == HS_HIST))
				cvtColor(imgMat, hsvMat, CV_BGR2HSV);	//Convert the image from bgr to hsv
			if ((distributionType == HS_GAUSS || distributionType == HS_HIST) && !isHistEqualizationBeforeHSV)
			{
				vector<Mat> hsv_planes;
				split( hsvMat, hsv_planes );
				for (int i = 0; i < (hsv_planes.size() - 1); i++)
					equalizeHist(hsv_planes[i], hsv_planes[i]);
				merge( hsv_planes, hsvMat);
			}
		}
		if (!isHistEqualizationActive && (distributionType == HS_GAUSS || distributionType == HS_HIST))
		{
			cvtColor(imgMat, hsvMat, CV_BGR2HSV);	//Convert the image from bgr to hsv
		}

		varPatches = calculateVarianceYforPatches(xyzMat, size_patch);	//Calculate the variance of Y in each patch

		//Calculate distributions within the patches to be used as feature vectors
		if (distributionType == HS_HIST)	
			featureVec = calculateHistograms(hsvMat, size_patch, channels_hs, 2, size_hist_hs, ranges_hs, false);	//Calculate image patch histograms
		if (distributionType == RGB_HIST)	
			featureVec = calculateHistograms(imgMat, size_patch, channels, 3, size_hist, ranges, false);	//Calculate image patch histograms
		if (distributionType == HS_GAUSS)	
			featureVec = calculateGaussiansHS2D(hsvMat, size_patch);	//Calculate image patch BGR gaussians
		if (distributionType == RGB_GAUSS)	
			featureVec = calculateGaussiansBGR3D(imgMat, size_patch);	//Calculate image patch BGR gaussians

		if (debug) cout << "Thresholding the variance patches..." << endl;
		trPatchesVar = thresholdPatches1Chi(varPatches, thresholdVarYd);

		//check whether training patches are obtained in that frame
		if (trPatchesVar.rows == 0)
		{
			if (debug) cout << "No training samples are obtained, returning..." << endl;
			return imgMat; //bgrtrMatVar;
		}

		if (debug) cout << "The biggest area of patches are being filtered..." << endl;
		filteredtrPatchesVar = filterBiggestBlob(trPatchesVar); 

		//insert the training patches to the short-term memory and get the locations 
		if (debug) cout << "From extracted patches, the histograms to be used in the training are being inserted to the training buffer..." << endl;
		insertTrainingPatches(filteredtrPatchesVar, featureVec, trainingBuffer, locationTrPatches);
		
		//////////////////////////////////////////////////////////////////
		////////////////// MODEL LEARNING AND UPDATING ///////////////////
		//////////////////////////////////////////////////////////////////

		if (isFirstFrame)	//If it is the first frame in this algorithm, you have to learn!
		{
			if (isHierarchicalClustering)
			{
				vector<Mat> clusters, trainingClusters;
				Mat dummyTrainingBuffer;
				//Find the optimal number of clusters from the data
				n_clusters_initial = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
				for (int i = 0; i < clusters.size(); i++)
					cout << "Cluster " << i << " : " << clusters[i].size() << endl;
				trainingBuffer = empty;
				//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
				//Else learn it
				for (int i = 0; i < clusters.size(); i++)
					if (clusters[i].rows < n_samples_min)
					{
						dummyTrainingBuffer.push_back(clusters[i]);
						n_clusters_initial--;
					}	
					else
						trainingClusters.push_back(clusters[i]);	
				cout << "N Training Buffer : " << trainingClusters.size() << endl;
				cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
				if (n_clusters_initial > 0)
				{
					//Find Gaussians using EM and all training samples
					//clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
					clusterSVM(trainingClusters, models, weights, paramsSVM);
					//cout << means.rows << endl;
					//covsInv = invertDiagonalMatrixAll(covs);
				}
				trainingBuffer = dummyTrainingBuffer;	//pass remaining samples to the next frame
			}
			else
			{
					//Find Gaussians using EM and all training samples
					//clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
					vector<Mat> tr;
					tr.push_back(trainingBuffer);
					clusterSVM(tr, models, weights, paramsSVM);
					//cout << means.rows << endl;
					//covsInv = invertDiagonalMatrixAll(covs);
					trainingBuffer = empty;		//empty the training buffer
			}
		}
		else
		{
			int nTrainingSamplesOld = trainingBuffer.rows;	//take the number of training samples before the update of models
			//covsInv = invertDiagonalMatrixAll(covs);	//invert the covariance matrices
			updateClustersSVM(trainingBuffer, models, weights);	//update clusters (models) found with One-Class SVM
		
			//int nTrainingSamplesCurr = trainingBuffer.rows;	//take the number of training samples after the update of models
			if (debug) cout << "Remaining training samples : " << trainingBuffer.rows << endl;
		
			//Parameters of the new models to learn
			Mat weightsNew;
			vector<CvSVM> modelsNew;

			if (isHierarchicalClustering)
			{
				vector<Mat> clusters, trainingClusters;
				Mat dummyTrainingBuffer;
				//Find the optimal number of clusters from the data
				n_clusters_new = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
				for (int i = 0; i < clusters.size(); i++)
					cout << "Cluster " << i << " : " << clusters[i].size() << endl;
				trainingBuffer = empty;
				//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
				//Else learn it
				int nTrainingSamplesCurr = 0;
				for (int i = 0; i < clusters.size(); i++)
					if (clusters[i].rows < n_samples_min)
					{
						dummyTrainingBuffer.push_back(clusters[i]);
						n_clusters_new--;
					}
					else
					{
						trainingClusters.push_back(clusters[i]);
						nTrainingSamplesCurr += clusters[i].rows;
					}
	
				cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
				cout << "N Training Buffer : " << nTrainingSamplesCurr << endl;

				if (n_clusters_new > 0)
				{
					//int nTrainingSamplesCurr = trainingBuffer.rows;
		
					double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
					clusterSVM(trainingClusters, modelsNew, weightsNew, paramsSVM);	//learn new models
					weightsNew = weightsNew * weightNew;		//update the weights
					insertClusters2LibSVM(models, weights, modelsNew, weightsNew);	//insert the newly learned models to the model library
				}
		
				trainingBuffer = dummyTrainingBuffer;		//pass remaining samples to the next frame
			}
			else
			{
				int nTrainingSamplesCurr = trainingBuffer.rows;

				if (nTrainingSamplesCurr > n_samples_min)	//if the remaining training samples are greater than N
				{
					//Parameters of the new models to learn
					//Mat meansNew, weightsNew;
					//vector<Mat> covsNew;

					double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
					vector<Mat> tr;
					tr.push_back(trainingBuffer);
					clusterSVM(tr, modelsNew, weightsNew, paramsSVM);	//learn new models
					weightsNew = weightsNew * weightNew;		//update the weights
					insertClusters2LibSVM(models, weights, modelsNew, weightsNew);	//insert the newly learned models to the model library
					trainingBuffer = empty;		//empty the training sample buffer
				}	
			}
		}
		//if (debug) cout << "Number of clusters = " << means.rows << endl;

		//////////////////////////////////////////////////////////////////
		///////////////////// CLASSIFICATION /////////////////////////////
		//////////////////////////////////////////////////////////////////

		if (models.size() > 0)
		{
			//classify
			if (debug) cout << "Classifying the current image..." << endl;
			classifySVMThroughTrainingSamples(models, weights, featureVec, filteredtrPatchesVar, locationTrPatches, classifiedPatches);
		}
		classifiedMat = transformPatches2Color3Chi(classifiedPatches, imgMat, size_patch);
		return classifiedMat;
	}
	else
		return imgMat;
}
*/

/////////////////////////// SINGLE MODEL //////////////////////////////

//road detection function. Takes original bgr image and xyz point cloud matrices and classify the road regions in the image
//returns the classified image as road and non-road regions overlaid on the original image
//road regions are green, non red regions are reddish
//imgMat is a matrix of 3-channels uchars
//xyzMat is a matrix of 3-channels doubles
//trainingBuffer is a matrix that holds training samples (histograms)
//model is a CvSVM model of one-class svm
//thresholdVarYd is the height variance threshold to be used in the training sample generation
//isFirstFrame is a bool to indicate whether the system is just started or not
//distributionType is one of the RGB_GAUSS, HS_GAUSS, RGB_HIST and HS_HIST
Mat detectRoadSingleSVM(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, vector<Mat> &trainingBufferLib, CvSVM &model, Mat &weights, double thresholdVarYd, bool isFirstFrame, typeDistribution distributionType)
{	
	if ( distributionType > -1 && distributionType < 4)
	{
		Mat hsvMat;
		Mat varPatches;
		
		//Distribution vector
		vector<vector<Mat>> featureVec;

		Mat trPatchesVar;	//Matrix composed of Y variance of each training patches 
		Mat filteredtrPatchesVar;	//Filtered (biggest blob) matrix composed of Y variance of each training patches
		Mat locationTrPatches;		//Matrix holding the location of the training samples in the (row and column) patch matrix
		Mat classifiedBinaryMat;
		Mat classifiedPatches;
		Mat classifiedMat;

		int ntr = 0;		//number of training patches obtained in current frame

		Mat empty;	//An empty matrix

		paramsSVM.svm_type = CvSVM::ONE_CLASS;
		paramsSVM.kernel_type = CvSVM::RBF;
		paramsSVM.gamma = gamma;
		paramsSVM.nu = nu;
		paramsSVM.term_crit = cvTermCriteria( CV_TERMCRIT_ITER+CV_TERMCRIT_EPS, 100, 0.1 );


		//check the height variance threshold
		thresholdVarYd = (thresholdVarYd != 0) ? thresholdVarYd : 0.00001;
	
		//////////////////////////////////////////////////////////////////
		///////////////////// PRE-PROCESSING /////////////////////////////
		//////////////////////////////////////////////////////////////////
	
		//Do histogram equalization
		if (isHistEqualizationActive)
		{
			if (distributionType == RGB_GAUSS || distributionType == RGB_HIST || 
			   ((distributionType == HS_GAUSS || distributionType == HS_HIST) && isHistEqualizationBeforeHSV))
			{
				vector<Mat> bgr_planes;
				split( imgMat, bgr_planes );
				for (int i = 0; i < bgr_planes.size(); i++)
					equalizeHist(bgr_planes[i], bgr_planes[i]);
				merge( bgr_planes, imgMat);
			}
			if ((distributionType == HS_GAUSS || distributionType == HS_HIST))
				cvtColor(imgMat, hsvMat, CV_BGR2HSV);	//Convert the image from bgr to hsv
			if ((distributionType == HS_GAUSS || distributionType == HS_HIST) && !isHistEqualizationBeforeHSV)
			{
				vector<Mat> hsv_planes;
				split( hsvMat, hsv_planes );
				for (int i = 0; i < (hsv_planes.size() - 1); i++)
					equalizeHist(hsv_planes[i], hsv_planes[i]);
				merge( hsv_planes, hsvMat);
			}
		}
		if (!isHistEqualizationActive && (distributionType == HS_GAUSS || distributionType == HS_HIST))
		{
			cvtColor(imgMat, hsvMat, CV_BGR2HSV);	//Convert the image from bgr to hsv
		}

		varPatches = calculateVarianceYforPatches(xyzMat, size_patch);	//Calculate the variance of Y in each patch

		//Calculate distributions within the patches to be used as feature vectors
		if (distributionType == HS_HIST)	
			featureVec = calculateHistograms(hsvMat, size_patch, channels_hs, 2, size_hist_hs, ranges_hs, false);	//Calculate image patch histograms
		else if (distributionType == RGB_HIST)	
			featureVec = calculateHistograms(imgMat, size_patch, channels, 3, size_hist, ranges, false);	//Calculate image patch histograms
		else if (distributionType == HS_GAUSS)	
			featureVec = calculateGaussiansHS2D(hsvMat, size_patch);	//Calculate image patch BGR gaussians
		else if (distributionType == RGB_GAUSS)	
			featureVec = calculateGaussiansBGR3D(imgMat, size_patch);	//Calculate image patch BGR gaussians

		if (debug) cout << "Thresholding the variance patches..." << endl;
		trPatchesVar = thresholdPatches1Chi(varPatches, thresholdVarYd, &ntr);

		//check whether training patches are obtained in that frame
		if (ntr < 5)
		{
			if (debug) cout << "No training samples are obtained, returning..." << endl;
			return imgMat; //bgrtrMatVar;
		}

		if (debug) cout << "The biggest area of patches are being filtered..." << endl;
		filteredtrPatchesVar = filterBiggestBlob(trPatchesVar, &ntr); 

		//check whether training patches are obtained in that frame
		if (ntr < 5)
		{
			if (debug) cout << "No training samples are obtained, returning..." << endl;
			return imgMat; //bgrtrMatVar;
		}

		//insert the training patches to the short-term memory and get the locations 
		if (debug) cout << "From extracted patches, the histograms to be used in the training are being inserted to the training buffer..." << endl;
		insertTrainingPatches(filteredtrPatchesVar, featureVec, trainingBuffer, locationTrPatches);
		
		//////////////////////////////////////////////////////////////////
		////////////////// MODEL LEARNING AND UPDATING ///////////////////
		//////////////////////////////////////////////////////////////////

		if (isFirstFrame)	//If it is the first frame in this algorithm, you have to learn!
		{
			if (isHierarchicalClustering)
			{
				vector<Mat> clusters, trainingClusters;
				Mat dummyTrainingBuffer;
				//Find the optimal number of clusters from the data
				n_clusters_initial = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
				for (int i = 0; i < clusters.size(); i++)
					cout << "Cluster " << i << " : " << clusters[i].size() << endl;
				trainingBuffer = empty;
				//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
				//Else learn it
				for (int i = 0; i < clusters.size(); i++)
					if (clusters[i].rows < n_samples_min)
					{
						dummyTrainingBuffer.push_back(clusters[i]);
						n_clusters_initial--;
					}	
					else
						trainingBuffer.push_back(clusters[i]);	

				cout << "N Training Buffer : " << trainingBuffer.rows << endl;
				cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
				if (n_clusters_initial > 0)
				{
					//pass samples gathered and have sufficient size to the sample library and cluster
					trainingBufferLib.push_back(trainingBuffer);
					clusterSVMSingle(trainingBufferLib, model, paramsSVM);
				}
				trainingBuffer = dummyTrainingBuffer;	//pass remaining samples to the next frame
			}
			else
			{
				//pass samples gathered and have sufficient size to the sample library and cluster
				trainingBufferLib.push_back(trainingBuffer);
				clusterSVMSingle(trainingBufferLib, model, paramsSVM);
				trainingBuffer = empty;		//empty the training buffer
			}
		}
		else
		{
			if (isHierarchicalClustering)
			{
				vector<Mat> clusters, trainingClusters;
				Mat dummyTrainingBuffer;
				//Find the optimal number of clusters from the data
				n_clusters_new = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
				for (int i = 0; i < clusters.size(); i++)
					cout << "Cluster " << i << " : " << clusters[i].size() << endl;
				trainingBuffer = empty;
				//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
				//Else learn it
				int nTrainingSamplesCurr = 0;
				for (int i = 0; i < clusters.size(); i++)
					if (clusters[i].rows < n_samples_min)
					{
						dummyTrainingBuffer.push_back(clusters[i]);
						n_clusters_new--;
					}
					else
						trainingBuffer.push_back(clusters[i]);
	
				cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
				cout << "N Training Buffer : " << trainingBuffer.rows << endl;

				if (n_clusters_new > 0)
				{
					trainingBufferLib.push_back(trainingBuffer);
					clusterSVMSingle(trainingBufferLib, model, paramsSVM);	//learn new models
					removeOldSamplesFromLibSVMSingle(trainingBufferLib);	//removes the samples that are really old
				}
		
				trainingBuffer = dummyTrainingBuffer;		//pass remaining unclustered samples to the next frame
			}
			else
			{
				int nTrainingSamplesCurr = trainingBuffer.rows;

				if (nTrainingSamplesCurr > n_samples_min)	//if the remaining training samples are greater than N
				{
					trainingBufferLib.push_back(trainingBuffer);
					clusterSVMSingle(trainingBufferLib, model, paramsSVM);	//learn new models
					removeOldSamplesFromLibSVMSingle(trainingBufferLib);
					trainingBuffer = empty;		//empty the training sample buffer
				}	
			}
		}
		//if (debug) cout << "Number of clusters = " << means.rows << endl;

		//////////////////////////////////////////////////////////////////
		///////////////////// CLASSIFICATION /////////////////////////////
		//////////////////////////////////////////////////////////////////

		if (model.get_support_vector_count() != 0)
		{
			//classify
			if (debug) cout << "Classifying the current image..." << endl;
			classifySingleSVMThroughTrainingSamples(model, featureVec, filteredtrPatchesVar, locationTrPatches, classifiedPatches);
		}
		classifiedMat = transformPatches2Color3Chi(classifiedPatches, imgMat, size_patch);
		return classifiedMat;
	}
	else
		return imgMat;
}


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
Mat detectRoadSampleBased(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, vector<Mat> &trainingBufferLib, double thresholdVarYd, bool isFirstFrame, typeDistribution distributionType)
{	
	if ( distributionType > -1 && distributionType < 4)
	{
		Mat hsvMat;
		Mat varPatches;
		
		//Distribution vector
		vector<vector<Mat>> featureVec;

		Mat trPatchesVar;	//Matrix composed of Y variance of each training patches 
		Mat filteredtrPatchesVar;	//Filtered (biggest blob) matrix composed of Y variance of each training patches
		Mat locationTrPatches;		//Matrix holding the location of the training samples in the (row and column) patch matrix
		Mat classifiedBinaryMat;
		Mat classifiedPatches;
		Mat classifiedMat;

		int ntr = 0;		//number of training patches obtained in current frame

		Mat empty;	//An empty matrix

		//check the height variance threshold
		thresholdVarYd = (thresholdVarYd != 0) ? thresholdVarYd : 0.00001;
	
		//////////////////////////////////////////////////////////////////
		///////////////////// PRE-PROCESSING /////////////////////////////
		//////////////////////////////////////////////////////////////////
	
		//Do histogram equalization
		if (isHistEqualizationActive)
		{
			if (distributionType == RGB_GAUSS || distributionType == RGB_HIST || 
			   ((distributionType == HS_GAUSS || distributionType == HS_HIST) && isHistEqualizationBeforeHSV))
			{
				vector<Mat> bgr_planes;
				split( imgMat, bgr_planes );
				for (int i = 0; i < bgr_planes.size(); i++)
					equalizeHist(bgr_planes[i], bgr_planes[i]);
				merge( bgr_planes, imgMat);
			}
			if ((distributionType == HS_GAUSS || distributionType == HS_HIST))
				cvtColor(imgMat, hsvMat, CV_BGR2HSV);	//Convert the image from bgr to hsv
			if ((distributionType == HS_GAUSS || distributionType == HS_HIST) && !isHistEqualizationBeforeHSV)
			{
				vector<Mat> hsv_planes;
				split( hsvMat, hsv_planes );
				for (int i = 0; i < (hsv_planes.size() - 1); i++)
					equalizeHist(hsv_planes[i], hsv_planes[i]);
				merge( hsv_planes, hsvMat);
			}
		}
		if (!isHistEqualizationActive && (distributionType == HS_GAUSS || distributionType == HS_HIST))
		{
			cvtColor(imgMat, hsvMat, CV_BGR2HSV);	//Convert the image from bgr to hsv
		}

		varPatches = calculateVarianceYforPatches(xyzMat, size_patch);	//Calculate the variance of Y in each patch

		//Calculate distributions within the patches to be used as feature vectors
		if (distributionType == HS_HIST)	
			featureVec = calculateHistograms(hsvMat, size_patch, channels_hs, 2, size_hist_hs, ranges_hs, false);	//Calculate image patch histograms
		else if (distributionType == RGB_HIST)	
			featureVec = calculateHistograms(imgMat, size_patch, channels, 3, size_hist, ranges, false);	//Calculate image patch histograms
		else if (distributionType == HS_GAUSS)	
			featureVec = calculateGaussiansHS2D(hsvMat, size_patch);	//Calculate image patch BGR gaussians
		else if (distributionType == RGB_GAUSS)	
			featureVec = calculateGaussiansBGR3D(imgMat, size_patch);	//Calculate image patch BGR gaussians

		if (debug) cout << "Thresholding the variance patches..." << endl;
		trPatchesVar = thresholdPatches1Chi(varPatches, thresholdVarYd, &ntr);

		//check whether training patches are obtained in that frame
		if (ntr < 5)
		{
			if (debug) cout << "No training samples are obtained, returning..." << endl;
			return imgMat; //bgrtrMatVar;
		}

		if (debug) cout << "The biggest area of patches are being filtered..." << endl;
		filteredtrPatchesVar = filterBiggestBlob(trPatchesVar, &ntr);

		//check whether training patches are obtained in that frame
		if (ntr < 5)
		{
			if (debug) cout << "No training samples are obtained, returning..." << endl;
			return imgMat; //bgrtrMatVar;
		}

		//insert the training patches to the short-term memory and get the locations 
		if (debug) cout << "From extracted patches, the histograms to be used in the training are being inserted to the training buffer..." << endl;
		insertTrainingPatches(filteredtrPatchesVar, featureVec, trainingBuffer, locationTrPatches);
		
		//////////////////////////////////////////////////////////////////
		////////////////// MODEL LEARNING AND UPDATING ///////////////////
		//////////////////////////////////////////////////////////////////

		if (isFirstFrame)	//If it is the first frame in this algorithm, you have to learn!
		{
			if (isHierarchicalClustering)
			{
				vector<Mat> clusters, trainingClusters;
				Mat dummyTrainingBuffer;
				//Find the optimal number of clusters from the data
				n_clusters_initial = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
				for (int i = 0; i < clusters.size(); i++)
					cout << "Cluster " << i << " : " << clusters[i].size() << endl;
				trainingBuffer = empty;
				//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
				//Else learn it
				for (int i = 0; i < clusters.size(); i++)
					if (clusters[i].rows < n_samples_min)
					{
						dummyTrainingBuffer.push_back(clusters[i]);
						n_clusters_initial--;
					}	
					else
						trainingBuffer.push_back(clusters[i]);	

				cout << "N Training Buffer : " << trainingBuffer.rows << endl;
				cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
				if (n_clusters_initial > 0)
					trainingBufferLib.push_back(trainingBuffer);
				trainingBuffer = dummyTrainingBuffer;	//pass remaining samples to the next frame
			}
			else
			{
					trainingBufferLib.push_back(trainingBuffer);
					trainingBuffer = empty;		//empty the training buffer
			}
		}
		else
		{
			if (isHierarchicalClustering)
			{
				vector<Mat> clusters, trainingClusters;
				Mat dummyTrainingBuffer;
				//Find the optimal number of clusters from the data
				n_clusters_new = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
				for (int i = 0; i < clusters.size(); i++)
					cout << "Cluster " << i << " : " << clusters[i].size() << endl;
				trainingBuffer = empty;
				//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
				//Else learn it
				for (int i = 0; i < clusters.size(); i++)
					if (clusters[i].rows < n_samples_min)
					{
						dummyTrainingBuffer.push_back(clusters[i]);
						n_clusters_new--;
					}
					else
					{
						trainingBuffer.push_back(clusters[i]);
					}
	
				cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
				cout << "N Training Buffer : " << trainingBuffer.rows << endl;

				if (n_clusters_new > 0)
				{
					trainingBufferLib.push_back(trainingBuffer);
					removeOldSamplesFromLibSVMSingle(trainingBufferLib);
				}
				trainingBuffer = dummyTrainingBuffer;		//pass remaining samples to the next frame
			}
			else
			{
				int nTrainingSamplesCurr = trainingBuffer.rows;

				if (nTrainingSamplesCurr > n_samples_min)	//if the remaining training samples are greater than N
				{
					trainingBufferLib.push_back(trainingBuffer);
					removeOldSamplesFromLibSVMSingle(trainingBufferLib);
					trainingBuffer = empty;		//empty the training sample buffer
				}	
			}
		}
		//if (debug) cout << "Number of clusters = " << means.rows << endl;

		//////////////////////////////////////////////////////////////////
		///////////////////// CLASSIFICATION /////////////////////////////
		//////////////////////////////////////////////////////////////////

		if (trainingBufferLib.size() != 0)
		{
			//classify
			if (debug) cout << "Classifying the current image..." << endl;
			classifyThroughTrainingSamplesEuc(trainingBufferLib, featureVec, filteredtrPatchesVar, locationTrPatches, classifiedPatches, threshold_classify_mahalanobis);
		}
		classifiedMat = transformPatches2Color3Chi(classifiedPatches, imgMat, size_patch);
		return classifiedMat;
	}
	else
		return imgMat;
}









///////////////////////////////////// DEBUG ALGORITHM /////////////////////////////////////

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
Mat detectRoadDebug(Mat imgMat, Mat xyzMat, Mat &trainingBuffer, Mat &means, vector<Mat> &covs, Mat &weights, double thresholdVarYd, bool isFirstFrame, typeDistribution distributionType)
{	
	if ( distributionType > -1 && distributionType < 4)
	{
		Mat hsvMat;
		Mat varPatches;
		
		//Distribution vector
		vector<vector<Mat>> featureVec;

		Mat trPatchesVar;	//Matrix composed of Y variance of each training patches 
		Mat filteredtrPatchesVar;	//Filtered (biggest blob) matrix composed of Y variance of each training patches
		Mat locationTrPatches;		//Matrix holding the location of the training samples in the (row and column) patch matrix
		Mat classifiedBinaryMat;
		Mat classifiedPatches;
		Mat classifiedMat;

		int ntr = 0;		//number of training patches obtained in current frame

		vector<Mat> covsInv;		//vector of matrices holding the inverse covariance materices of the Gaussian models
		Mat empty;	//An empty matrix

		vector<int> output_params;
		output_params.push_back(CV_IMWRITE_PXM_BINARY);
		output_params.push_back(1);

		//check the height variance threshold
		thresholdVarYd = (thresholdVarYd != 0) ? thresholdVarYd : 0.00001;
	
		//////////////////////////////////////////////////////////////////
		///////////////////// PRE-PROCESSING /////////////////////////////
		//////////////////////////////////////////////////////////////////
	
		//Do histogram equalization
		if (isHistEqualizationActive)
		{
			if (distributionType == RGB_GAUSS || distributionType == RGB_HIST || 
			   ((distributionType == HS_GAUSS || distributionType == HS_HIST) && isHistEqualizationBeforeHSV))
			{
				vector<Mat> bgr_planes;
				split( imgMat, bgr_planes );
				for (int i = 0; i < bgr_planes.size(); i++)
					equalizeHist(bgr_planes[i], bgr_planes[i]);
				merge( bgr_planes, imgMat);
				imwrite("histeq.ppm", imgMat, output_params);
			}
			if ((distributionType == HS_GAUSS || distributionType == HS_HIST))
				cvtColor(imgMat, hsvMat, CV_BGR2HSV);	//Convert the image from bgr to hsv
			if ((distributionType == HS_GAUSS || distributionType == HS_HIST) && !isHistEqualizationBeforeHSV)
			{
				vector<Mat> hsv_planes;
				split( hsvMat, hsv_planes );
				for (int i = 0; i < (hsv_planes.size() - 1); i++)
					equalizeHist(hsv_planes[i], hsv_planes[i]);
				merge( hsv_planes, hsvMat);
			}
		}
		if (!isHistEqualizationActive && (distributionType == HS_GAUSS || distributionType == HS_HIST))
		{
			cvtColor(imgMat, hsvMat, CV_BGR2HSV);	//Convert the image from bgr to hsv
		}

		varPatches = calculateVarianceYforPatches(xyzMat, size_patch);	//Calculate the variance of Y in each patch

		//Calculate distributions within the patches to be used as feature vectors
		if (distributionType == HS_HIST)	
			featureVec = calculateHistograms(hsvMat, size_patch, channels_hs, 2, size_hist_hs, ranges_hs, false);	//Calculate image patch histograms
		if (distributionType == RGB_HIST)	
			featureVec = calculateHistograms(imgMat, size_patch, channels, 3, size_hist, ranges, false);	//Calculate image patch histograms
		if (distributionType == HS_GAUSS)	
			featureVec = calculateGaussiansHS2D(hsvMat, size_patch);	//Calculate image patch BGR gaussians
		if (distributionType == RGB_GAUSS)	
			featureVec = calculateGaussiansBGR3D(imgMat, size_patch);	//Calculate image patch BGR gaussians

		if (debug) cout << "Thresholding the variance patches..." << endl;
		trPatchesVar = thresholdPatches1Chi(varPatches, thresholdVarYd, &ntr);

		//check whether training patches are obtained in that frame
		if (ntr < 5)
		{
			if (debug) cout << "No training samples are obtained, returning..." << endl;
			classifiedMat = transformPatches2Color3Chi(Mat::zeros(height/size_patch, width/size_patch, CV_64FC1) , imgMat, size_patch);
			return classifiedMat; //bgrtrMatVar;
		}

		imwrite("trPatchesVar.pgm", trPatchesVar, output_params);

		if (debug) cout << "The biggest area of patches are being filtered..." << endl;
		filteredtrPatchesVar = filterBiggestBlob(trPatchesVar, &ntr);

		//check whether training patches are obtained in that frame
		if (ntr < 5)
		{
			if (debug) cout << "No training samples are obtained, returning..." << endl;
			classifiedMat = transformPatches2Color3Chi(Mat::zeros(height/size_patch, width/size_patch, CV_64FC1) , imgMat, size_patch);
			return classifiedMat; //bgrtrMatVar;
		}

		//insert the training patches to the short-term memory and get the locations 
		if (debug) cout << "From extracted patches, the histograms to be used in the training are being inserted to the training buffer..." << endl;
		insertTrainingPatches(filteredtrPatchesVar, featureVec, trainingBuffer, locationTrPatches);
		
		//////////////////////////////////////////////////////////////////
		////////////////// MODEL LEARNING AND UPDATING ///////////////////
		//////////////////////////////////////////////////////////////////

		if (isFirstFrame)	//If it is the first frame in this algorithm, you have to learn!
		{
			if (isHierarchicalClustering)
			{
				vector<Mat> clusters;
				Mat dummyTrainingBuffer;
				//Find the optimal number of clusters from the data
				n_clusters_initial = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
				if (debug)
				{
					for (int i = 0; i < clusters.size(); i++)
						cout << "Cluster " << i << " : " << clusters[i].size() << endl;
				}
				trainingBuffer = empty;
				//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
				//Else learn it
				for (int i = 0; i < clusters.size(); i++)
					if (clusters[i].rows < n_samples_min)
					{
						dummyTrainingBuffer.push_back(clusters[i]);
						n_clusters_initial--;
					}	
					else
						trainingBuffer.push_back(clusters[i]);	
				cout << "N Training Buffer : " << trainingBuffer.rows << endl;
				cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
				if (n_clusters_initial > 0)
				{
					//Find Gaussians using EM and all training samples
					clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
					//cout << means.rows << endl;
					covsInv = invertDiagonalMatrixAll(covs);
				}
				trainingBuffer = dummyTrainingBuffer;	//pass remaining samples to the next frame
			}
			else
			{
					//Find Gaussians using EM and all training samples
					clusterEM(trainingBuffer, n_clusters_initial, means, covs, weights);
					//cout << means.rows << endl;
					covsInv = invertDiagonalMatrixAll(covs);
					trainingBuffer = empty;		//empty the training buffer
			}
		}
		else
		{
			int nTrainingSamplesOld = trainingBuffer.rows;	//take the number of training samples before the update of models
			covsInv = invertDiagonalMatrixAll(covs);	//invert the covariance matrices
			updateClustersEMMah(trainingBuffer, threshold_update_mahalanobis, means, covs, covsInv, weights);	//update clusters (models) found with EM
		
			//int nTrainingSamplesCurr = trainingBuffer.rows;	//take the number of training samples after the update of models
			if (debug) cout << "Remaining training samples : " << trainingBuffer.rows << endl;
		
			//Parameters of the new models to learn
			Mat meansNew, weightsNew;
			vector<Mat> covsNew;

			if (isHierarchicalClustering)
			{
				vector<Mat> clusters;
				Mat dummyTrainingBuffer;
				//Find the optimal number of clusters from the data
				n_clusters_new = clusterHierarchicalAverageLinkage(clusters, trainingBuffer);
				if (debug)
				{
					for (int i = 0; i < clusters.size(); i++)
						cout << "Cluster " << i << " : " << clusters[i].size() << endl;
				}
				trainingBuffer = empty;
				//If the samples in a cluster smaller then n_samples_min, pass it to the next frame
				//Else learn it
				for (int i = 0; i < clusters.size(); i++)
					if (clusters[i].rows < n_samples_min)
					{
						dummyTrainingBuffer.push_back(clusters[i]);
						n_clusters_new--;
					}
					else
						trainingBuffer.push_back(clusters[i]);
	
				cout << "N Dummy Training Buffer : " << dummyTrainingBuffer.rows << endl;
				cout << "N Training Buffer : " << trainingBuffer.rows << endl;

				if (n_clusters_new > 0)
				{
					int nTrainingSamplesCurr = trainingBuffer.rows;
		
					double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
					clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
					weightsNew = weightsNew * weightNew;		//update the weights
					insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
				}
		
				trainingBuffer = dummyTrainingBuffer;		//pass remaining samples to the next frame
			}
			else
			{
				int nTrainingSamplesCurr = trainingBuffer.rows;

				if (nTrainingSamplesCurr > n_samples_min)	//if the remaining training samples are greater than N
				{
					//Parameters of the new models to learn
					//Mat meansNew, weightsNew;
					//vector<Mat> covsNew;

					double weightNew = (double)nTrainingSamplesCurr/(double)nTrainingSamplesOld;	//the weight of the remaining samples to the samples at the start
					clusterEM(trainingBuffer, n_clusters_new, meansNew, covsNew, weightsNew);	//learn new models
					weightsNew = weightsNew * weightNew;		//update the weights
					insertClusters2LibEM(means, covs, covsInv, weights, meansNew, covsNew, weightsNew);	//insert the newly learned models to the model library
					trainingBuffer = empty;		//empty the training sample buffer
				}	
			}
		}
		if (debug) cout << "Number of clusters = " << means.rows << endl;

		//////////////////////////////////////////////////////////////////
		///////////////////// CLASSIFICATION /////////////////////////////
		//////////////////////////////////////////////////////////////////

		if (means.rows > 0)
		{
			//classify
			if (debug) cout << "Classifying the current image..." << endl;
			classifyEMThroughTrainingSamplesMah(means, covsInv, weights, featureVec, filteredtrPatchesVar, locationTrPatches, classifiedPatches, threshold_classify_mahalanobis);
			classifiedMat = transformPatches2Color3Chi(classifiedPatches, imgMat, size_patch);
		}
		else
			classifiedMat = transformPatches2Color3Chi(Mat::zeros(height/size_patch, width/size_patch, CV_64FC1) , imgMat, size_patch);
		return classifiedMat;
	}
	else
		return imgMat;
}
