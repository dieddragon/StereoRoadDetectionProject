#include "main.h"

/*

//////////////////////////////////////////////////////////////////////
/////////// AGGLOMERATIVE CLUSTERING OF SAMPLES OPERATIONS ///////////
//////////////////////////////////////////////////////////////////////

////////////////////// UNCLASSIFIED OTHER METHODS LINKAGE METHODS ///////////////////////

//finds the minimum Euclidian distance to a cluster of samples from a sample point (sample histogram)
float findMinDistanceToCluster(Mat cluster, MatND hist)
{
	float minDist = INF;
	for (int i = 0; i < cluster.rows; i++)
	{
		float currDist = 0;
		for (int j = 0; j < cluster.cols; j++)
			currDist += (cluster.at<float>(i,j)-hist.at<float>(j/hist.cols,j%hist.cols))*(cluster.at<float>(i,j)-hist.at<float>(j/hist.cols,j%hist.cols));
		//currDist = sqrt(currDist);
		minDist = (currDist < minDist) ? currDist : minDist;
	}
	return minDist;
}

//finds the closest Euclidian distance to the closest cluster of samples among an array of clusters
float findClosestDistanceToClusters(vector<Mat> clusters, MatND hist)
{
	float minDist = INF;
	for (int i = 0; i < clusters.size(); i++)
	{
		float currDist = findMinDistanceToCluster(clusters[i], hist);	
		minDist = (currDist < minDist) ? currDist : minDist;
	}
	return minDist;
}

//Classify using cluster of samples
void classify(vector<Mat> clusters, vector<vector<MatND>> hists, Mat &resultantMat, int sizePatch, float threshold)
{
	for (int i = 0; i < hists.size(); i++)
		for (int j = 0; j < hists[i].size(); j++)
		{
			float dist = findClosestDistanceToClusters(clusters, hists[i][j]);
			if (dist < threshold) 
			{
				for (int k = i * sizePatch; k < (i+1) * sizePatch; k++)
					for (int l = j * sizePatch; l < (j+1) * sizePatch; l++)
						resultantMat.at<Vec<uchar,3>>(k,l)[1] = 255;
			}
			else
			{
				for (int k = i * sizePatch; k < (i+1) * sizePatch; k++)
					for (int l = j * sizePatch; l < (j+1) * sizePatch; l++)
						resultantMat.at<Vec<uchar,3>>(k,l)[1] = 0;
			}
		}
}

//////////////////////////////////////////////////////////////////////
/////////////////// K-MEANS CLUSTERING OPERATIONS ////////////////////
//////////////////////////////////////////////////////////////////////

//finds the closest Euclidian distance to the closest cluster among an array of clusters
float findClosestDistanceToClusters(Mat clusterCenters, MatND hist)
{
	float minDist = INF;
	for (int i = 0; i < clusterCenters.rows; i++)
	{
		float currDist = 0;
		float term = 0;
		for (int j = 0; j < hist.rows; j++)
			for (int k = 0; k < hist.cols; k++)
			{
				term = (hist.at<float>(j,k) - clusterCenters.at<float>(i,j*hist.cols+k));
				currDist += (term*term);
			}
		//currDist = sqrt(currDist);
		//float currDist = findMinDistanceToCluster(clusters[i], hist);	
		minDist = (currDist < minDist) ? currDist : minDist;
	}
	return minDist;
}

//classify using kmeans clusters
void classifyKMeans(Mat centers, vector<vector<MatND>> hists, Mat &resultantMat, int sizePatch, float threshold)
{
	for (int i = 0; i < hists.size(); i++)
		for (int j = 0; j < hists[i].size(); j++)
		{
			float dist = findClosestDistanceToClusters(centers, hists[i][j]);
			if (dist < threshold) 
			{
				for (int k = i * sizePatch; k < (i+1) * sizePatch; k++)
					for (int l = j * sizePatch; l < (j+1) * sizePatch; l++)
						resultantMat.at<Vec<uchar,3>>(k,l)[1] = 255;
			}
			else
			{
				for (int k = i * sizePatch; k < (i+1) * sizePatch; k++)
					for (int l = j * sizePatch; l < (j+1) * sizePatch; l++)
						resultantMat.at<Vec<uchar,3>>(k,l)[1] = 0;
			}
		}
}

*/

//////////////////////////////////////////////////////////////////////
///////////////////// SAMPLE BASED CLUSTERING ////////////////////////
//////////////////////////////////////////////////////////////////////

//////////////////// VIA MAHALANOBIS DISTANCE ////////////////////////


//Given a training sample library find the closest sample to the sample (feature vector) according to mahalanobis distance
float findClosestMahDistanceToSamples(vector<Mat> trainingBufferLib, Mat covInv, Mat feature)
{
	float minDist = INF;

	//find the closest sample according to euclidian distance
	for (int i = 0; i < trainingBufferLib.size(); i++)
		for (int j = 0; j < trainingBufferLib[i].rows; j++)
		{
			Mat roiLib = trainingBufferLib[i](Range(j,(j+1)),Range(0,trainingBufferLib[i].cols));
			Mat term = (feature - roiLib);
			Mat term2 = term*covInv*term.t();
			float currDist = term2.at<double>(0,0);
			minDist = (currDist < minDist) ? currDist : minDist;
		}
	return minDist;
}

void classifyThroughTrainingSamplesMah(vector<Mat> trainingBufferLib, Mat covInv, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold)
{
	classifiedPatches = filteredbgrtrPatchesVar;//.clone();
	MatIterator_<int> it = locationPatches.begin<int>();
	
	for (int i = 0; i < locationPatches.rows; i++, ++it)
	{
		vector<int> queueRow, queueCol;
		queueRow.push_back(*it);
		it++;
		queueCol.push_back(*it);
		while (!(queueRow.empty()))
		{
			int currRow = queueRow[queueRow.size() - 1];
			int currCol = queueCol[queueCol.size() - 1];
			queueRow.pop_back();
			queueCol.pop_back();
			
			//extract neighbours
			vector<int> neighborRow, neighborCol;
			//left
			if (currRow != 0)
			{
				neighborRow.push_back(currRow - 1);
				neighborCol.push_back(currCol);
			}
			//right
			if (currRow != classifiedPatches.rows - 1)
			{
				neighborRow.push_back(currRow + 1);
				neighborCol.push_back(currCol);
			}
			//top
			if (currCol != 0)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol - 1);
			}
			//down
			if (currCol != classifiedPatches.cols - 1)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol + 1);
			}

			for (int j = 0; j < neighborRow.size(); j++)
			{
				if (classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) == 0)
				{
					//TODO: Instead of finding the closest distance, only find a cluster that is smaller than a threshold.
					//Thus not calculate the distance for all samples but find only one cluster that is closer than the threshold.
					float dist = findClosestMahDistanceToSamples(trainingBufferLib, covInv, features[neighborRow[j]][neighborCol[j]]);
					if (dist < threshold) 
					{
						classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) = 255;
						queueRow.push_back(neighborRow[j]);
						queueCol.push_back(neighborCol[j]);
						
					}
				}
			}
		}
	}
}

//////////////////// VIA EUCLIDIAN DISTANCE ////////////////////////


//Given a training sample library find the closest sample to the sample (feature vector) according to euclidian distance
float findClosestEucDistanceToSamples(vector<Mat> trainingBufferLib, Mat feature)
{
	float minDist = INF;

	//find the closest sample according to euclidian distance
	for (int i = 0; i < trainingBufferLib.size(); i++)
		for (int j = 0; j < trainingBufferLib[i].rows; j++)
		{
			Mat roiLib = trainingBufferLib[i](Range(j,(j+1)),Range(0,trainingBufferLib[i].cols));
			//Mat roiCovsInv = covsInv[i];
			Mat term = (feature - roiLib);
			Mat term2 = term*term.t();
			float currDist = term2.at<double>(0,0);
			minDist = (currDist < minDist) ? currDist : minDist;
		}
	return minDist;
}

void classifyThroughTrainingSamplesEuc(vector<Mat> trainingBufferLib, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold)
{
	classifiedPatches = filteredbgrtrPatchesVar;//.clone();
	MatIterator_<int> it = locationPatches.begin<int>();
	
	for (int i = 0; i < locationPatches.rows; i++, ++it)
	{
		vector<int> queueRow, queueCol;
		queueRow.push_back(*it);
		it++;
		queueCol.push_back(*it);
		while (!(queueRow.empty()))
		{
			int currRow = queueRow[queueRow.size() - 1];
			int currCol = queueCol[queueCol.size() - 1];
			queueRow.pop_back();
			queueCol.pop_back();
			
			//extract neighbours
			vector<int> neighborRow, neighborCol;
			//left
			if (currRow != 0)
			{
				neighborRow.push_back(currRow - 1);
				neighborCol.push_back(currCol);
			}
			//right
			if (currRow != classifiedPatches.rows - 1)
			{
				neighborRow.push_back(currRow + 1);
				neighborCol.push_back(currCol);
			}
			//top
			if (currCol != 0)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol - 1);
			}
			//down
			if (currCol != classifiedPatches.cols - 1)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol + 1);
			}

			for (int j = 0; j < neighborRow.size(); j++)
			{
				if (classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) == 0)
				{
					//TODO: Instead of finding the closest distance, only find a cluster that is smaller than a threshold.
					//Thus not calculate the distance for all samples but find only one cluster that is closer than the threshold.
					float dist = findClosestEucDistanceToSamples(trainingBufferLib, features[neighborRow[j]][neighborCol[j]]);
					if (dist < threshold) 
					{
						classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) = 255;
						queueRow.push_back(neighborRow[j]);
						queueCol.push_back(neighborCol[j]);
						
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////
///////////////////// EM CLUSTERING OPERATIONS ///////////////////////
//////////////////////////////////////////////////////////////////////

//////////////////// VIA MAHALANOBIS DISTANCE ////////////////////////

//Given means and inverse of covariances find the closest cluster to the sample (feature vector) according to mahalanobis distance
float findClosestMahDistanceToClusters(Mat means, vector<Mat> covsInv, Mat feature)
{
	float minDist = INF;

	//find the closest cluster according to mahalanobis distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		Mat roiCovsInv = covsInv[i];
		Mat term = (feature - roiMeans);
		Mat term2 = term*roiCovsInv*term.t();
		float currDist = term2.at<double>(0,0);
		minDist = (currDist < minDist) ? currDist : minDist;
	}
	return minDist;
}

//resultantMat is of type double and 1 channel matrix, it is the patch matrix not an image
void classifyEMBinaryMah(Mat means, vector<Mat> covsInv, Mat weights, vector<vector<Mat>> features, Mat &resultantMat, float threshold)
{
	for (int i = 0; i < features.size(); i++)
		for (int j = 0; j < features[i].size(); j++)
		{
			float dist = findClosestMahDistanceToClusters(means, covsInv, features[i][j]);
			if (dist < threshold) 
			{
					resultantMat.at<double>(i,j) = 255;
			}
		}
}


//Classification using EM found Gaussians via region growing from the training samples
//classifiedPatches is the patch matrix not an image
void classifyEMThroughTrainingSamplesMah(Mat means, vector<Mat> covsInv, Mat weights, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold)
{
	classifiedPatches = filteredbgrtrPatchesVar;//.clone();
	MatIterator_<int> it = locationPatches.begin<int>();
	
	for (int i = 0; i < locationPatches.rows; i++, ++it)
	{
		vector<int> queueRow, queueCol;
		queueRow.push_back(*it);
		it++;
		queueCol.push_back(*it);
		while (!(queueRow.empty()))
		{
			int currRow = queueRow[queueRow.size() - 1];
			int currCol = queueCol[queueCol.size() - 1];
			queueRow.pop_back();
			queueCol.pop_back();
			
			//extract neighbours
			vector<int> neighborRow, neighborCol;
			//left
			if (currRow != 0)
			{
				neighborRow.push_back(currRow - 1);
				neighborCol.push_back(currCol);
			}
			//right
			if (currRow != classifiedPatches.rows - 1)
			{
				neighborRow.push_back(currRow + 1);
				neighborCol.push_back(currCol);
			}
			//top
			if (currCol != 0)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol - 1);
			}
			//down
			if (currCol != classifiedPatches.cols - 1)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol + 1);
			}

			for (int j = 0; j < neighborRow.size(); j++)
			{
				if (classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) == 0)
				{
					//TODO: Instead of finding the closest distance, only find a cluster that is smaller than a threshold.
					//Thus not calculate the distance for all clusters but find only one cluster that is closer than the threshold.
					float dist = findClosestMahDistanceToClusters(means, covsInv, features[neighborRow[j]][neighborCol[j]]);
					if (dist < threshold) 
					{
						classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) = 255;
						queueRow.push_back(neighborRow[j]);
						queueCol.push_back(neighborCol[j]);
						
					}
				}
			}
		}
	}
}


//find a cluster closer than threshold according to mahalanobis distance
//if cannot find, returns -1
int findClusterMah(Mat means, vector<Mat> covsInv, Mat histMat, float threshold)
{
	int indClosestCluster = -1;
	//find a cluster according to mahalanobis distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		Mat roiCovsInv = covsInv[i];
		Mat term = (histMat - roiMeans);
		Mat term2 = term*roiCovsInv*term.t();
		float currDist = *term2.begin<double>();	
		indClosestCluster = (currDist < threshold) ? i : indClosestCluster;
		if (indClosestCluster != -1) return indClosestCluster;
	}
	return indClosestCluster;
}

//find the cluster closest to histMat according to mahalanobis distance
//if cannot find, returns -1
int findClosestClusterMah(Mat means, vector<Mat> covsInv, Mat histMat, float threshold)
{
	int indClosestCluster = -1;
	float minDist = INF;
	//find a cluster according to mahalanobis distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		Mat roiCovsInv = covsInv[i];
		Mat term = (histMat - roiMeans);
		Mat term2 = term*roiCovsInv*term.t();
		float currDist = *term2.begin<double>();	
		indClosestCluster = (currDist < threshold && currDist < minDist) ? i : indClosestCluster;
		minDist = (currDist < minDist) ? currDist : minDist;
		//if (indClosestCluster != -1) return indClosestCluster;
	}
	return indClosestCluster;
}

//////////////////// VIA EUCLIDIAN DISTANCE ////////////////////////

//Given means find the closest cluster to the sample (feature vector) according to Euclidian distance
float findClosestEucDistanceToClusters(Mat means, Mat feature)
{
	float minDist = INF;

	//find the closest cluster according to Euclidian distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		Mat term = (feature - roiMeans);
		Mat term2 = term*term.t();
		float currDist = term2.at<double>(0,0);
		minDist = (currDist < minDist) ? currDist : minDist;
	}
	return minDist;
}

//resultantMat is of type double and 1 channel matrix, it is the patch matrix not an image
void classifyEMBinaryEuc(Mat means, vector<vector<Mat>> features, Mat &resultantMat, float threshold)
{
	for (int i = 0; i < features.size(); i++)
		for (int j = 0; j < features[i].size(); j++)
		{
			float dist = findClosestEucDistanceToClusters(means, features[i][j]);
			if (dist < threshold) 
			{
					resultantMat.at<double>(i,j) = 255;
			}
		}
}


//Classification using EM found Gaussians via region growing from the training samples
//classifiedPatches is the patch matrix not an image
void classifyEMThroughTrainingSamplesEuc(Mat means, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold)
{
	classifiedPatches = filteredbgrtrPatchesVar;//.clone();
	MatIterator_<int> it = locationPatches.begin<int>();
	
	for (int i = 0; i < locationPatches.rows; i++, ++it)
	{
		vector<int> queueRow, queueCol;
		queueRow.push_back(*it);
		it++;
		queueCol.push_back(*it);
		while (!(queueRow.empty()))
		{
			int currRow = queueRow[queueRow.size() - 1];
			int currCol = queueCol[queueCol.size() - 1];
			queueRow.pop_back();
			queueCol.pop_back();
			
			//extract neighbours
			vector<int> neighborRow, neighborCol;
			//left
			if (currRow != 0)
			{
				neighborRow.push_back(currRow - 1);
				neighborCol.push_back(currCol);
			}
			//right
			if (currRow != classifiedPatches.rows - 1)
			{
				neighborRow.push_back(currRow + 1);
				neighborCol.push_back(currCol);
			}
			//top
			if (currCol != 0)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol - 1);
			}
			//down
			if (currCol != classifiedPatches.cols - 1)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol + 1);
			}

			for (int j = 0; j < neighborRow.size(); j++)
			{
				if (classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) == 0)
				{
					//TODO: Instead of finding the closest distance, only find a cluster that is smaller than a threshold.
					//Thus not calculate the distance for all clusters but find only one cluster that is closer than the threshold.
					float dist = findClosestEucDistanceToClusters(means, features[neighborRow[j]][neighborCol[j]]);
					if (dist < threshold) 
					{
						classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) = 255;
						queueRow.push_back(neighborRow[j]);
						queueCol.push_back(neighborCol[j]);
						
					}
				}
			}
		}
	}
}

//find a cluster closer than threshold according to Euclidian distance
//if cannot find, returns -1
int findClusterEuc(Mat means, Mat feature, float threshold)
{
	int indClosestCluster = -1;
	//find a cluster according to Euclidian distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		Mat term = (feature - roiMeans);
		Mat term2 = term*term.t();
		float currDist = *term2.begin<double>();	
		indClosestCluster = (currDist < threshold) ? i : indClosestCluster;
		if (indClosestCluster != -1) return indClosestCluster;
	}
	return indClosestCluster;
}

//find the cluster closest to feature (vector) according to Euclidian distance
//if cannot find, returns -1
int findClosestClusterEuc(Mat means, Mat feature, float threshold)
{
	int indClosestCluster = -1;
	float minDist = INF;
	//find a cluster according to Euclidian distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		Mat term = (feature - roiMeans);
		Mat term2 = term*term.t();
		float currDist = *term2.begin<double>();	
		indClosestCluster = (currDist < threshold && currDist < minDist) ? i : indClosestCluster;
		minDist = (currDist < minDist) ? currDist : minDist;
		//if (indClosestCluster != -1) return indClosestCluster;
	}
	return indClosestCluster;
}

//////////////////// VIA MANHATTAN DISTANCE ////////////////////////

//Given means find the closest cluster to the sample (feature vector) according to Manhattan distance
float findClosestManDistanceToClusters(Mat means, Mat feature)
{
	float minDist = INF;

	//find the closest cluster according to Manhattan distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		float currDist = norm(feature, roiMeans, NORM_L1);
		minDist = (currDist < minDist) ? currDist : minDist;
	}
	return minDist;
}

//resultantMat is of type double and 1 channel matrix, it is the patch matrix not an image
void classifyEMBinaryMan(Mat means, vector<vector<Mat>> features, Mat &resultantMat, float threshold)
{
	for (int i = 0; i < features.size(); i++)
		for (int j = 0; j < features[i].size(); j++)
		{
			float dist = findClosestManDistanceToClusters(means, features[i][j]);
			if (dist < threshold) 
			{
					resultantMat.at<double>(i,j) = 255;
			}
		}
}


//Classification using EM found Gaussians via region growing from the training samples
//classifiedPatches is the patch matrix not an image
void classifyEMThroughTrainingSamplesMan(Mat means, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold)
{
	classifiedPatches = filteredbgrtrPatchesVar;//.clone();
	MatIterator_<int> it = locationPatches.begin<int>();
	
	for (int i = 0; i < locationPatches.rows; i++, ++it)
	{
		vector<int> queueRow, queueCol;
		queueRow.push_back(*it);
		it++;
		queueCol.push_back(*it);
		while (!(queueRow.empty()))
		{
			int currRow = queueRow[queueRow.size() - 1];
			int currCol = queueCol[queueCol.size() - 1];
			queueRow.pop_back();
			queueCol.pop_back();
			
			//extract neighbours
			vector<int> neighborRow, neighborCol;
			//left
			if (currRow != 0)
			{
				neighborRow.push_back(currRow - 1);
				neighborCol.push_back(currCol);
			}
			//right
			if (currRow != classifiedPatches.rows - 1)
			{
				neighborRow.push_back(currRow + 1);
				neighborCol.push_back(currCol);
			}
			//top
			if (currCol != 0)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol - 1);
			}
			//down
			if (currCol != classifiedPatches.cols - 1)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol + 1);
			}

			for (int j = 0; j < neighborRow.size(); j++)
			{
				if (classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) == 0)
				{
					//TODO: Instead of finding the closest distance, only find a cluster that is smaller than a threshold.
					//Thus not calculate the distance for all clusters but find only one cluster that is closer than the threshold.
					float dist = findClosestManDistanceToClusters(means, features[neighborRow[j]][neighborCol[j]]);
					if (dist < threshold) 
					{
						classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) = 255;
						queueRow.push_back(neighborRow[j]);
						queueCol.push_back(neighborCol[j]);
						
					}
				}
			}
		}
	}
}

//find a cluster closer than threshold according to Manhattan distance
//if cannot find, returns -1
int findClusterMan(Mat means, Mat feature, float threshold)
{
	int indClosestCluster = -1;
	//find a cluster according to Manhattan distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		float currDist = norm(feature, roiMeans, NORM_L1);	
		indClosestCluster = (currDist < threshold) ? i : indClosestCluster;
		if (indClosestCluster != -1) return indClosestCluster;
	}
	return indClosestCluster;
}

//find the cluster closest to feature (vector) according to Manhattan distance
//if cannot find, returns -1
int findClosestClusterMan(Mat means, Mat feature, float threshold)
{
	int indClosestCluster = -1;
	float minDist = INF;
	//find a cluster according to Manhattan distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		float currDist = norm(feature, roiMeans, NORM_L1);
		indClosestCluster = (currDist < threshold && currDist < minDist) ? i : indClosestCluster;
		minDist = (currDist < minDist) ? currDist : minDist;
		//if (indClosestCluster != -1) return indClosestCluster;
	}
	return indClosestCluster;
}

//////////////////// VIA CHEBYSHEV DISTANCE ////////////////////////

//Given means find the closest cluster to the sample (feature vector) according to Chebyshev distance
float findClosestChebDistanceToClusters(Mat means, Mat feature)
{
	float minDist = INF;

	//find the closest cluster according to Chebyshev distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		float currDist = norm(feature, roiMeans, NORM_INF);
		minDist = (currDist < minDist) ? currDist : minDist;
	}
	return minDist;
}

//resultantMat is of type double and 1 channel matrix, it is the patch matrix not an image
void classifyEMBinaryCheb(Mat means, vector<vector<Mat>> features, Mat &resultantMat, float threshold)
{
	for (int i = 0; i < features.size(); i++)
		for (int j = 0; j < features[i].size(); j++)
		{
			float dist = findClosestChebDistanceToClusters(means, features[i][j]);
			if (dist < threshold) 
			{
					resultantMat.at<double>(i,j) = 255;
			}
		}
}


//Classification using EM found Gaussians via region growing from the training samples
//classifiedPatches is the patch matrix not an image
void classifyEMThroughTrainingSamplesCheb(Mat means, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold)
{
	classifiedPatches = filteredbgrtrPatchesVar;//.clone();
	MatIterator_<int> it = locationPatches.begin<int>();
	
	for (int i = 0; i < locationPatches.rows; i++, ++it)
	{
		vector<int> queueRow, queueCol;
		queueRow.push_back(*it);
		it++;
		queueCol.push_back(*it);
		while (!(queueRow.empty()))
		{
			int currRow = queueRow[queueRow.size() - 1];
			int currCol = queueCol[queueCol.size() - 1];
			queueRow.pop_back();
			queueCol.pop_back();
			
			//extract neighbours
			vector<int> neighborRow, neighborCol;
			//left
			if (currRow != 0)
			{
				neighborRow.push_back(currRow - 1);
				neighborCol.push_back(currCol);
			}
			//right
			if (currRow != classifiedPatches.rows - 1)
			{
				neighborRow.push_back(currRow + 1);
				neighborCol.push_back(currCol);
			}
			//top
			if (currCol != 0)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol - 1);
			}
			//down
			if (currCol != classifiedPatches.cols - 1)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol + 1);
			}

			for (int j = 0; j < neighborRow.size(); j++)
			{
				if (classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) == 0)
				{
					//TODO: Instead of finding the closest distance, only find a cluster that is smaller than a threshold.
					//Thus not calculate the distance for all clusters but find only one cluster that is closer than the threshold.
					float dist = findClosestChebDistanceToClusters(means, features[neighborRow[j]][neighborCol[j]]);
					if (dist < threshold) 
					{
						classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) = 255;
						queueRow.push_back(neighborRow[j]);
						queueCol.push_back(neighborCol[j]);
						
					}
				}
			}
		}
	}
}

//find a cluster closer than threshold according to Chebyshev distance
//if cannot find, returns -1
int findClusterCheb(Mat means, Mat feature, float threshold)
{
	int indClosestCluster = -1;
	//find a cluster according to Manhattan distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		float currDist = norm(feature, roiMeans, NORM_INF);	
		indClosestCluster = (currDist < threshold) ? i : indClosestCluster;
		if (indClosestCluster != -1) return indClosestCluster;
	}
	return indClosestCluster;
}

//find the cluster closest to feature (vector) according to Chebyshev distance
//if cannot find, returns -1
int findClosestClusterCheb(Mat means, Mat feature, float threshold)
{
	int indClosestCluster = -1;
	float minDist = INF;
	//find a cluster according to Chebyshev distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		float currDist = norm(feature, roiMeans, NORM_INF);
		indClosestCluster = (currDist < threshold && currDist < minDist) ? i : indClosestCluster;
		minDist = (currDist < minDist) ? currDist : minDist;
		//if (indClosestCluster != -1) return indClosestCluster;
	}
	return indClosestCluster;
}

//////////////////// VIA HELLINGER DISTANCE ////////////////////////

////////////////// FOR HISTOGRAMS ////////////////////////////

//Given means of the models find the closest cluster to the sample (feature vector) according to hellinger distance
double findClosestHellDistanceToClustersHist(Mat means, Mat feature)
{
	double minDist = INF;

	//find the closest cluster according to hellinger distance
	for (int i = 0; i < means.rows; i++)
	{
		//model stuff
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		//feature stuff
		double currDist = compareHist(feature, roiMeans, CV_COMP_HELLINGER);
		minDist = (currDist < minDist) ? currDist : minDist;
	}
	return minDist;
}

//resultantMat is of type double and 1 channel matrix, it is the patch matrix not an image
void classifyEMBinaryHellHist(Mat means, vector<vector<Mat>> features, Mat &resultantMat, float threshold)
{
		for (int i = 0; i < features.size(); i++)
			for (int j = 0; j < features[i].size(); j++)
			{
				double dist = findClosestHellDistanceToClustersHist(means, features[i][j]);
				if (dist < threshold) 
				{
					resultantMat.at<double>(i,j) = 255;
				}
			}
}

//Classification using EM found Gaussians via region growing from the training samples
//classifiedPatches is the patch matrix not an image
void classifyEMThroughTrainingSamplesHellHist(Mat means, Mat weights, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold)
{
	classifiedPatches = filteredbgrtrPatchesVar;//.clone();
	MatIterator_<int> it = locationPatches.begin<int>();
	
	for (int i = 0; i < locationPatches.rows; i++, ++it)
	{
		vector<int> queueRow, queueCol;
		queueRow.push_back(*it);
		it++;
		queueCol.push_back(*it);
		while (!(queueRow.empty()))
		{
			int currRow = queueRow[queueRow.size() - 1];
			int currCol = queueCol[queueCol.size() - 1];
			queueRow.pop_back();
			queueCol.pop_back();
			
			//extract neighbours
			vector<int> neighborRow, neighborCol;
			//left
			if (currRow != 0)
			{
				neighborRow.push_back(currRow - 1);
				neighborCol.push_back(currCol);
			}
			//right
			if (currRow != classifiedPatches.rows - 1)
			{
				neighborRow.push_back(currRow + 1);
				neighborCol.push_back(currCol);
			}
			//top
			if (currCol != 0)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol - 1);
			}
			//down
			if (currCol != classifiedPatches.cols - 1)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol + 1);
			}

			for (int j = 0; j < neighborRow.size(); j++)
			{
				if (classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) == 0)
				{
					double dist = findClosestHellDistanceToClustersHist(means, features[neighborRow[j]][neighborCol[j]]);
					if (dist < threshold) 
					{
						classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) = 255;
						queueRow.push_back(neighborRow[j]);
						queueCol.push_back(neighborCol[j]);
						
					}
				}
			}
		}
	}
}

//find a cluster closer than threshold according to hellinger distance
int findClusterHellHist(Mat means, Mat histMat, float threshold)
{
	int indClosestCluster = -1;
	//find a cluster according to mahalanobis distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		double currDist = compareHist(histMat, roiMeans, CV_COMP_HELLINGER);	
		indClosestCluster = (currDist < threshold) ? i : indClosestCluster;
		if (indClosestCluster != -1) return indClosestCluster;
	}
	return indClosestCluster;
}

//find the cluster closest to histMat according to hellinger distance
//if cannot find, returns -1
int findClosestClusterHellHist(Mat means, Mat histMat, float threshold)
{
	int indClosestCluster = -1;
	double minDist = INF;
	//find a cluster according to mahalanobis distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		double currDist = compareHist(histMat, roiMeans, CV_COMP_HELLINGER);
		indClosestCluster = (currDist < threshold && currDist < minDist) ? i : indClosestCluster;
		minDist = (currDist < minDist) ? currDist : minDist;
		//if (indClosestCluster != -1) return indClosestCluster;
	}
	return indClosestCluster;
}

////////////////// FOR GAUSSIANS ////////////////////////////

//Finds the Hellinger distance between two gaussians
//Gaussians are in the form of vectors
//If 2d: mean1 mean2 var11 var22 var12 var21
//If 3d: mean1 mean2 mean3 var11 var22 var33 var12 var13 var23 var21 var31 var32
double findHellDistanceGauss(Mat gaussian1, Mat gaussian2)
{
	Mat cov1, cov2, mean1, mean2;
	if (gaussian1.cols == 6)
	{
		cov1 = Mat(2, 2, CV_64FC1);
		cov2 = Mat(2, 2, CV_64FC1);
		mean1 = Mat(2, 1, CV_64FC1);
		mean2 = Mat(2, 1, CV_64FC1);
		mean1.at<double>(0,0) = gaussian1.at<double>(0,0);
		mean2.at<double>(0,0) = gaussian2.at<double>(0,0);
		mean1.at<double>(1,0) = gaussian1.at<double>(0,1);
		mean2.at<double>(1,0) = gaussian2.at<double>(0,1);
		cov1.at<double>(0,0) = gaussian1.at<double>(0,2);
		cov2.at<double>(0,0) = gaussian2.at<double>(0,2);
		cov1.at<double>(1,1) = gaussian1.at<double>(0,3);
		cov2.at<double>(1,1) = gaussian2.at<double>(0,3);
		cov1.at<double>(0,1) = gaussian1.at<double>(0,4);
		cov2.at<double>(0,1) = gaussian2.at<double>(0,4);
		cov1.at<double>(1,0) = gaussian1.at<double>(0,4);
		cov2.at<double>(1,0) = gaussian2.at<double>(0,4);
	}
	if (gaussian1.cols == 12)
	{
		cov1 = Mat(3, 3, CV_64FC1);
		cov2 = Mat(3, 3, CV_64FC1);
		mean1 = Mat(3, 1, CV_64FC1);
		mean2 = Mat(3, 1, CV_64FC1);
		mean1.at<double>(0,0) = gaussian1.at<double>(0,0);
		mean2.at<double>(0,0) = gaussian2.at<double>(0,0);
		mean1.at<double>(1,0) = gaussian1.at<double>(0,1);
		mean2.at<double>(1,0) = gaussian2.at<double>(0,1);
		mean1.at<double>(2,0) = gaussian1.at<double>(0,2);
		mean2.at<double>(2,0) = gaussian2.at<double>(0,2);
		cov1.at<double>(0,0) = gaussian1.at<double>(0,3);
		cov2.at<double>(0,0) = gaussian2.at<double>(0,3);
		cov1.at<double>(1,1) = gaussian1.at<double>(0,4);
		cov2.at<double>(1,1) = gaussian2.at<double>(0,4);
		cov1.at<double>(2,2) = gaussian1.at<double>(0,5);
		cov2.at<double>(2,2) = gaussian2.at<double>(0,5);
		cov1.at<double>(0,1) = gaussian1.at<double>(0,6);
		cov2.at<double>(0,1) = gaussian2.at<double>(0,6);
		cov1.at<double>(0,2) = gaussian1.at<double>(0,7);
		cov2.at<double>(0,2) = gaussian2.at<double>(0,7);
		cov1.at<double>(1,2) = gaussian1.at<double>(0,8);
		cov2.at<double>(1,2) = gaussian2.at<double>(0,8);
		cov1.at<double>(1,0) = gaussian1.at<double>(0,9);
		cov2.at<double>(1,0) = gaussian2.at<double>(0,9);
		cov1.at<double>(2,0) = gaussian1.at<double>(0,10);
		cov2.at<double>(2,0) = gaussian2.at<double>(0,10);
		cov1.at<double>(2,1) = gaussian1.at<double>(0,11);
		cov2.at<double>(2,1) = gaussian2.at<double>(0,11);
	}
	Mat term1 = 1/2*(cov1 + cov2);
	Mat term2 = mean1 - mean2;
	Mat term3 = term1.t()*term1.inv(DECOMP_LU)*term1;
	double term4 = exp(-1/8*term3.at<double>(0,0));
	double term5 = pow(determinant(cov1),0.25)*pow(determinant(cov2),0.25)/pow(determinant(term1),0.5);
	return (1-term5*term4);
}

//Given means of the models find the closest cluster to the sample (feature vector) according to hellinger distance
double findClosestHellDistanceToClustersGauss(Mat means, Mat feature)
{
	double minDist = INF;

	//find the closest cluster according to hellinger distance
	for (int i = 0; i < means.rows; i++)
	{
		//model stuff
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		//feature stuff
		double currDist = findHellDistanceGauss(feature, roiMeans);
		minDist = (currDist < minDist) ? currDist : minDist;
	}
	return minDist;
}

//resultantMat is of type double and 1 channel matrix, it is the patch matrix not an image
void classifyEMBinaryHellGauss(Mat means, vector<vector<Mat>> features, Mat &resultantMat, float threshold)
{
		for (int i = 0; i < features.size(); i++)
			for (int j = 0; j < features[i].size(); j++)
			{
				double dist = findClosestHellDistanceToClustersGauss(means, features[i][j]);
				if (dist < threshold) 
				{
					resultantMat.at<double>(i,j) = 255;
				}
			}
}

//Classification using EM found Gaussians via region growing from the training samples
//classifiedPatches is the patch matrix not an image
void classifyEMThroughTrainingSamplesHellGauss(Mat means, Mat weights, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold)
{
	classifiedPatches = filteredbgrtrPatchesVar;//.clone();
	MatIterator_<int> it = locationPatches.begin<int>();
	
	for (int i = 0; i < locationPatches.rows; i++, ++it)
	{
		vector<int> queueRow, queueCol;
		queueRow.push_back(*it);
		it++;
		queueCol.push_back(*it);
		while (!(queueRow.empty()))
		{
			int currRow = queueRow[queueRow.size() - 1];
			int currCol = queueCol[queueCol.size() - 1];
			queueRow.pop_back();
			queueCol.pop_back();
			
			//extract neighbours
			vector<int> neighborRow, neighborCol;
			//left
			if (currRow != 0)
			{
				neighborRow.push_back(currRow - 1);
				neighborCol.push_back(currCol);
			}
			//right
			if (currRow != classifiedPatches.rows - 1)
			{
				neighborRow.push_back(currRow + 1);
				neighborCol.push_back(currCol);
			}
			//top
			if (currCol != 0)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol - 1);
			}
			//down
			if (currCol != classifiedPatches.cols - 1)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol + 1);
			}

			for (int j = 0; j < neighborRow.size(); j++)
			{
				if (classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) == 0)
				{
					double dist = findClosestHellDistanceToClustersGauss(means, features[neighborRow[j]][neighborCol[j]]);
					if (dist < threshold) 
					{
						classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) = 255;
						queueRow.push_back(neighborRow[j]);
						queueCol.push_back(neighborCol[j]);
						
					}
				}
			}
		}
	}
}

//find a cluster closer than threshold according to hellinger distance
int findClusterHellGauss(Mat means, Mat histMat, float threshold)
{
	int indClosestCluster = -1;
	//find a cluster according to mahalanobis distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		double currDist = findHellDistanceGauss(histMat, roiMeans);	
		indClosestCluster = (currDist < threshold) ? i : indClosestCluster;
		if (indClosestCluster != -1) return indClosestCluster;
	}
	return indClosestCluster;
}

//find the cluster closest to histMat according to hellinger distance
//if cannot find, returns -1
int findClosestClusterHellGauss(Mat means, Mat histMat, float threshold)
{
	int indClosestCluster = -1;
	double minDist = INF;
	//find a cluster according to mahalanobis distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		double currDist = findHellDistanceGauss(histMat, roiMeans);
		indClosestCluster = (currDist < threshold && currDist < minDist) ? i : indClosestCluster;
		minDist = (currDist < minDist) ? currDist : minDist;
		//if (indClosestCluster != -1) return indClosestCluster;
	}
	return indClosestCluster;
}

//////////////////// VIA CHI-SQUARE DISTANCE ////////////////////////

////////////////// FOR HISTOGRAMS ////////////////////////////

//Given means of the models find the closest cluster to the sample (feature vector) according to chi-square distance
double findClosestChiDistanceToClustersHist(Mat means, Mat feature)
{
	double minDist = INF;

	//find the closest cluster according to hellinger distance
	for (int i = 0; i < means.rows; i++)
	{
		//model stuff
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		//feature stuff
		double currDist = compareHist(feature, roiMeans, CV_COMP_CHISQR);
		minDist = (currDist < minDist) ? currDist : minDist;
	}
	return minDist;
}

//resultantMat is of type double and 1 channel matrix, it is the patch matrix not an image
void classifyEMBinaryChiHist(Mat means, vector<vector<Mat>> features, Mat &resultantMat, float threshold)
{
		for (int i = 0; i < features.size(); i++)
			for (int j = 0; j < features[i].size(); j++)
			{
				double dist = findClosestChiDistanceToClustersHist(means, features[i][j]);
				if (dist < threshold) 
				{
					resultantMat.at<double>(i,j) = 255;
				}
			}
}

//Classification using EM found Gaussians via region growing from the training samples
//classifiedPatches is the patch matrix not an image
void classifyEMThroughTrainingSamplesChiHist(Mat means, Mat weights, vector<vector<Mat>> features, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches, float threshold)
{
	classifiedPatches = filteredbgrtrPatchesVar;//.clone();
	MatIterator_<int> it = locationPatches.begin<int>();
	
	for (int i = 0; i < locationPatches.rows; i++, ++it)
	{
		vector<int> queueRow, queueCol;
		queueRow.push_back(*it);
		it++;
		queueCol.push_back(*it);
		while (!(queueRow.empty()))
		{
			int currRow = queueRow[queueRow.size() - 1];
			int currCol = queueCol[queueCol.size() - 1];
			queueRow.pop_back();
			queueCol.pop_back();
			
			//extract neighbours
			vector<int> neighborRow, neighborCol;
			//left
			if (currRow != 0)
			{
				neighborRow.push_back(currRow - 1);
				neighborCol.push_back(currCol);
			}
			//right
			if (currRow != classifiedPatches.rows - 1)
			{
				neighborRow.push_back(currRow + 1);
				neighborCol.push_back(currCol);
			}
			//top
			if (currCol != 0)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol - 1);
			}
			//down
			if (currCol != classifiedPatches.cols - 1)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol + 1);
			}

			for (int j = 0; j < neighborRow.size(); j++)
			{
				if (classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) == 0)
				{
					double dist = findClosestChiDistanceToClustersHist(means, features[neighborRow[j]][neighborCol[j]]);
					if (dist < threshold) 
					{
						classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) = 255;
						queueRow.push_back(neighborRow[j]);
						queueCol.push_back(neighborCol[j]);
						
					}
				}
			}
		}
	}
}

//find a cluster closer than threshold according to chi-square distance
int findClusterChiHist(Mat means, Mat histMat, float threshold)
{
	int indClosestCluster = -1;
	//find a cluster according to mahalanobis distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		double currDist = compareHist(histMat, roiMeans, CV_COMP_CHISQR);	
		indClosestCluster = (currDist < threshold) ? i : indClosestCluster;
		if (indClosestCluster != -1) return indClosestCluster;
	}
	return indClosestCluster;
}

//find the cluster closest to histMat according to chi-square distance
//if cannot find, returns -1
int findClosestClusterChiHist(Mat means, Mat histMat, float threshold)
{
	int indClosestCluster = -1;
	double minDist = INF;
	//find a cluster according to chi-square distance
	for (int i = 0; i < means.rows; i++)
	{
		Mat roiMeans = means(Range(i,(i+1)),Range(0,means.cols));
		double currDist = compareHist(histMat, roiMeans, CV_COMP_CHISQR);
		indClosestCluster = (currDist < threshold && currDist < minDist) ? i : indClosestCluster;
		minDist = (currDist < minDist) ? currDist : minDist;
		//if (indClosestCluster != -1) return indClosestCluster;
	}
	return indClosestCluster;
}


//////////////////////////////////////////////////////////////////////
///////////////////// SVM CLUSTERING OPERATIONS ///////////////////////
//////////////////////////////////////////////////////////////////////

/////////////////////////// MULTI MODEL //////////////////////////////

//////////////////////////////////////////////////////////////////////
///// NOT WORKING WITH OPENCV 2.4.3 SINCE CvSVM COPY CONSTRUCTOR /////
/////////////////// DOES NOT WORKING PROPERLY ////////////////////////
///////////////// WILL IMPLEMENT MY OWN SVN LIB //////////////////////

/*
void classifySVMThroughTrainingSamples(vector<CvSVM> &models, Mat weights, vector<vector<Mat>> featureVec, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches)
{
	classifiedPatches = filteredbgrtrPatchesVar;//.clone();
	MatIterator_<int> it = locationPatches.begin<int>();
	
	for (int i = 0; i < locationPatches.rows; i++, ++it)
	{
		vector<int> queueRow, queueCol;
		queueRow.push_back(*it);
		it++;
		queueCol.push_back(*it);
		while (!(queueRow.empty()))
		{
			int currRow = queueRow[queueRow.size() - 1];
			int currCol = queueCol[queueCol.size() - 1];
			queueRow.pop_back();
			queueCol.pop_back();
			
			//extract neighbours
			vector<int> neighborRow, neighborCol;
			//left
			if (currRow != 0)
			{
				neighborRow.push_back(currRow - 1);
				neighborCol.push_back(currCol);
			}
			//right
			if (currRow != classifiedPatches.rows - 1)
			{
				neighborRow.push_back(currRow + 1);
				neighborCol.push_back(currCol);
			}
			//top
			if (currCol != 0)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol - 1);
			}
			//down
			if (currCol != classifiedPatches.cols - 1)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol + 1);
			}

			for (int j = 0; j < neighborRow.size(); j++)
			{
				if (classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) == 0)
				{
					Mat feat;
					(featureVec[neighborRow[j]][neighborCol[j]]).convertTo(feat, CV_32FC1);
					for (int k = 0; k < models.size(); k++)
						if (models[k].predict(feat) == 1)
						{
							classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) = 255;
							queueRow.push_back(neighborRow[j]);
							queueCol.push_back(neighborCol[j]);
							break;
						}
				}
			}
		}
	}
}
*/

/////////////////////////// SINGLE MODEL //////////////////////////////

void classifySingleSVMThroughTrainingSamples(CvSVM &model, vector<vector<Mat>> featureVec, Mat filteredbgrtrPatchesVar, Mat locationPatches, Mat &classifiedPatches)
{
	classifiedPatches = filteredbgrtrPatchesVar;//.clone();
	MatIterator_<int> it = locationPatches.begin<int>();
	
	for (int i = 0; i < locationPatches.rows; i++, ++it)
	{
		vector<int> queueRow, queueCol;
		queueRow.push_back(*it);
		it++;
		queueCol.push_back(*it);
		while (!(queueRow.empty()))
		{
			int currRow = queueRow[queueRow.size() - 1];
			int currCol = queueCol[queueCol.size() - 1];
			queueRow.pop_back();
			queueCol.pop_back();
			
			//extract neighbours
			vector<int> neighborRow, neighborCol;
			//left
			if (currRow != 0)
			{
				neighborRow.push_back(currRow - 1);
				neighborCol.push_back(currCol);
			}
			//right
			if (currRow != classifiedPatches.rows - 1)
			{
				neighborRow.push_back(currRow + 1);
				neighborCol.push_back(currCol);
			}
			//top
			if (currCol != 0)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol - 1);
			}
			//down
			if (currCol != classifiedPatches.cols - 1)
			{
				neighborRow.push_back(currRow);
				neighborCol.push_back(currCol + 1);
			}

			for (int j = 0; j < neighborRow.size(); j++)
			{
				if (classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) == 0)
				{
					Mat feat;
					(featureVec[neighborRow[j]][neighborCol[j]]).convertTo(feat, CV_32FC1);
					if (model.predict(feat) == 1)
					{
						classifiedPatches.at<double>(neighborRow[j],neighborCol[j]) = 255;
						queueRow.push_back(neighborRow[j]);
						queueCol.push_back(neighborCol[j]);
					}
				}
			}
		}
	}
}
