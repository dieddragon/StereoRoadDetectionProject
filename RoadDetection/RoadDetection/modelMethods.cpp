#include "main.h"

//////////////////////////////////////////////////////////////////////
////////////////////////// MODEL OPERATIONS /////////////////////////
//////////////////////////////////////////////////////////////////////

//Given the weights of the models, returns the index of the model with minimum weight.
int findIndexMinWeight(Mat weights)
{
	int indMin = -1;
	double weightMin = 1.1;
	MatIterator_<double> it = weights.begin<double>();
	MatIterator_<double> end = weights.end<double>();
	for (int i = 0; it != end; i++, ++it)
	{
		double currWeight = (*it);
		indMin = (currWeight < weightMin) ? i : indMin;
		weightMin = (currWeight < weightMin) ? currWeight : weightMin; 
	}
	return indMin;
}

//////////////////////////////////////////////////////////////////////
////////////////////////// MATRIX OPERATIONS /////////////////////////
//////////////////////////////////////////////////////////////////////

//Inverts a diagonal matrix. 
//origMat is a matrix of one channel double.
Mat invertDiagonalMatrix(Mat origMat)
{
	Mat invMat = Mat::zeros(origMat.rows, origMat.cols, CV_64FC1);
	MatIterator_<double> itInv = invMat.begin<double>();
	MatIterator_<double> itOrig = origMat.begin<double>();
	MatIterator_<double> end = origMat.end<double>();
	for (; itOrig != end; ++itOrig)
	{
		if ((*itOrig) < 0.000001)
		{
			(*itInv) = INF;
		}
		else
			(*itInv) = 1.0/(*itOrig);
		itOrig += origMat.cols;
		itInv += (origMat.cols + 1);
	}
	return invMat;
}

//Inverts a vector of diagonal matrices.
//Each matrix is a matrix type of one channel double. 
vector<Mat> invertDiagonalMatrixAll(vector<Mat> origMat)
{
	vector<Mat> invMat;
	vector<Mat>::iterator it = origMat.begin();
	vector<Mat>::iterator end = origMat.end();
	for (; it != end; ++it)
		invMat.push_back(invertDiagonalMatrix((*it)));
	return invMat;
}

//////////////////////////////////////////////////////////////////////
////////////////// TRAINING SAMPLE MEMORY OPERATIONS /////////////////
//////////////////////////////////////////////////////////////////////


//Inserts the training patches filtered to the short term buffer. 
//trPatches should be a matrix of doubles
//hists are the histograms calculated from the patches
//trainingBuffer is the short term training example buffer holds the selected histograms of the training patches
//void insertTrainingPatches(Mat trPatches, vector<vector<MatND>> hists, vector<MatND> trainingBuffer)
//{
//	
//	int count = 0;
//	for (int i = 0; i < trPatches.rows; i++)
//		for (int j = 0; j < trPatches.cols; j++)
//			if (trPatches.at<double>(i,j) == 255)
				//if (trainingBuffer.size() < N_SAMPLES_MAX)
				//{
//					trainingBuffer.insert(trainingBuffer.end(), hists[i][j]);
//					count++;
				//}
				//else
				//{

				//}
//	cout << trainingBuffer.size() << endl;
//	cout << count << endl;
//}

//Inserts the training patches filtered of RGB or HSV or HS histograms or gaussians to the short term buffer and insert the location of these pathces into a matrix
//trPatches should be a matrix of doubles
//features are the features calculated from the patches (histograms or gaussians)
//trainingBuffer is the short term training example buffer holds the selected features of the training patches
//location patches should be a matrix of int
void insertTrainingPatches(Mat trPatches, vector<vector<Mat>> features, Mat &buffer, Mat &locationPatches)
{
	MatIterator_<double> it = trPatches.begin<double>();
	for (int i = 0; i < trPatches.rows; i++)
		for (int j = 0; j < trPatches.cols; j++)
		{
			if ((*it) == 255)
			{
				Mat mat = (Mat_<int>(1,2) << i, j);
				locationPatches.push_back(mat);
				buffer.push_back(features[i][j]);
			}
			it++;
		}
		if (debug) cout << "Number of training patches = " << buffer.rows << endl;
}

//////////////////////////////////////////////////////////////////////
////////// AGGLOMERATIVE CLUSTERING OF SAMPLES OPERATIONS ////////////
//////////////////////////////////////////////////////////////////////

//Initialize the clusters from the given training samples one cluster for each row (each sample) in the training buffer.
void initializeClusters(vector<Mat> &clusters, Mat trainingBuffer)
{
	for (int i = 0; i < trainingBuffer.rows; i++)
	{
		Mat roi = trainingBuffer(Range(i,i+1),Range(0,trainingBuffer.cols));
		clusters.push_back(roi);
	}
	cout << "Number of initial clusters are : " << clusters.size() << endl;
}



//merges clusters i and j into one and put back into the original clusters array.
//clusters is a vector of Mat of doubles
void mergeClusters(vector<Mat> &clusters, int i, int j)
{
	int nRows = clusters[i].rows + clusters[j].rows;
	int nCols = clusters[i].cols;
	Mat merged = Mat::zeros(nRows, nCols, CV_64FC1);

	int k = 0;
	for (int m = 0; m < clusters[i].rows; m++, k++)
		for (int l = 0; l < nCols; l++)
			merged.at<double>(k,l) = clusters[i].at<double>(m,l);
	for (int m = 0; m < clusters[j].rows; m++, k++)
		for (int l = 0; l < nCols; l++)
			merged.at<double>(k,l) = clusters[j].at<double>(m,l);
	//cout << clusters[i] << endl;
	clusters.erase(clusters.begin() + i);
	if (i > j)
	{
		//cout << clusters[j] << endl;
		clusters.erase(clusters.begin() + j);
	}
	else
	{
		//cout << clusters[j - 1] << endl;
		clusters.erase(clusters.begin() + j - 1);
	}
	clusters.push_back(merged);
	//cout << clusters[clusters.size() - 1] << endl;

}

//given current clusters, calculate the pseudo-F statistic of the cluster group
float calculatePseudoFScore(vector<Mat> clusters, Mat trainingBuffer)
{
	float pseudoFScore = 0;
	if (trainingBuffer.rows == clusters.size())
		return pseudoFScore;
	Mat SST;	//Total sum of squares
	Mat SSW = Mat::zeros(trainingBuffer.cols, trainingBuffer.cols, CV_64FC1);	//Within cluster sum of squares
	Mat GM;		//General mean of data
	//cout << " Samples : " << trainingBuffer << endl;
	calcCovarMatrix(trainingBuffer, SST, GM, CV_COVAR_NORMAL | CV_COVAR_ROWS);
	//int nSamps = 0;
	//for (int i = 0; i < clusters.size(); i++)
	//	nSamps += clusters[i].rows;
	//cout << "N Samples : " << nSamps << endl;
	for (int i = 0; i < clusters.size(); i++)
		if (clusters[i].rows > 1)
		{
			Mat dummyCov, dummyMean;
			calcCovarMatrix(clusters[i], dummyCov, dummyMean, CV_COVAR_NORMAL | CV_COVAR_ROWS);
			SSW += dummyCov;
		}
		//cout << "Inter cluster SSB :" << trace(SST-SSW)[0]/(clusters.size()-1) << endl;
		//cout << "Intra cluster SSW :" << trace(SSW)[0]/(trainingBuffer.rows - clusters.size()) << endl;
		//cout << "GM : " << GM << endl;
		//cout << "SST : " << SST << endl;
		//cout << "SSW : " << SSW << endl;
	if (trace(SSW)[0] > 0)
		pseudoFScore = (trace(SST-SSW)[0]/(clusters.size()-1))/(trace(SSW)[0]/(trainingBuffer.rows - clusters.size()));
	return pseudoFScore;
}

////////////////////// AVERAGE LINKAGE METHODS ///////////////////////

//Finds the average Euclidian distance between two clusters of samples.
//Each row of cluster matrix holds one sample 
float findAverageDistance(Mat cluster1, Mat cluster2)
{
	float dist = 0;
	for (int i = 0; i < cluster1.rows; i++)
		for (int j = 0; j < cluster2.rows; j++)
		{
			Mat diff = cluster1.row(i) - cluster2.row(j);
			diff = diff*diff.t();
			//cout << *(diff.begin<float>()) << endl;
			dist += sqrt((double)(*(diff.begin<double>())));
		}
	dist = dist / (cluster1.rows * cluster2.rows);
	return dist;
}

//Finds the closest cluster to the cluster k according to the euclidian average distance and returns the found distance
float findClosestClusterAverageLinkage(vector<Mat> clusters, int k, int &l)
{
	float minDist = INF;
	for (int i = 0; i < clusters.size(); i++)
		if (i != k)
		{
			float currDist = findAverageDistance(clusters[k],clusters[i]);
			if (currDist < minDist)
			{
				minDist = currDist;
				l = i;
				if (minDist == 0) break;
			}
		}
	return minDist;
}

//Given a vector of clusters, finds the index of the closest two clusters according to the average linkage criterion with Euclidian distance
void findClosestClusterPairAverageLinkage(vector<Mat> clusters, int &i, int &j)
{
	float minDist = INF;
	int l;
	for (int k = 0; k < clusters.size(); k++)
	{
		float currDist = findClosestClusterAverageLinkage(clusters, k, l);
		if (currDist < minDist)
		{
			minDist = currDist;
			i = k;
			j = l;
		}
		if (minDist == 0) break;
	}
}

//hierarchical clustering algorithm using Average linkage distance criterion.
//starts with the clusters equals to the training samples and unifies the clusters until pseudo-F test gives a maximum or there
//remains only one cluster.
int clusterHierarchicalAverageLinkage(vector<Mat> &clusters, Mat trainingBuffer)
{
	int i, j;
	float pseudoFScore, pseudoFScoreMax;
	initializeClusters(clusters, trainingBuffer);
	vector<Mat> clustersMax = clusters;
	pseudoFScore = pseudoFScoreMax = 0;  //No of clusters are equal to the no of samples, hence zero pseudo-F score
	do
	{
		findClosestClusterPairAverageLinkage(clusters, i, j);
		//cout << "Distance =	" << dist << endl;
		mergeClusters(clusters, i, j);
		pseudoFScore = calculatePseudoFScore(clusters, trainingBuffer);
		if (pseudoFScore > pseudoFScoreMax && clusters.size() > 1 && clusters.size() < 20)
		{
			pseudoFScoreMax = pseudoFScore;
			clustersMax = clusters;
		}
		//cout << "Number of clusters : " << clusters.size() << endl;
		//cout << "Number of clustersMax : " << clustersMax.size() << endl;
		//cout << "pseudo-F Score : " << pseudoFScore << endl;
	}while(clusters.size() > 1);
	clustersMax = (pseudoFScoreMax == 0) ? clusters : clustersMax;
	clusters = clustersMax;
	return clusters.size();
}

////////////////////// COMPLETE LINKAGE METHODS ///////////////////////

//Finds the farthest Euclidian distance between two clusters of samples.
//Each row of cluster matrix holds one sample 
float findCompleteDistance(Mat cluster1, Mat cluster2)
{
	float maxDist = 0;
	for (int i = 0; i < cluster1.rows; i++)
		for (int j = 0; j < cluster2.rows; j++)
		{
			float currDist = 0;
			Mat diff = cluster1.row(i) - cluster2.row(j);
			diff = diff*diff.t();
			currDist = sqrt(*(diff.begin<double>()));
			maxDist = (currDist > maxDist) ? currDist : maxDist;
		}
	return maxDist;
}

//Finds the closest cluster to the cluster k according to the farthest neighbour and returns the found distance
float findClosestClusterCompleteLinkage(vector<Mat> clusters, int k, int &l)
{
	float minDist = INF;
	for (int i = 0; i < clusters.size(); i++)
		if (i != k)
		{
			float currDist = findCompleteDistance(clusters[k],clusters[i]);
			if (currDist < minDist)
			{
				minDist = currDist;
				l = i;
				if (minDist == 0) break;
			}
		}
	return minDist;
}

//Given a vector of clusters, finds the index of the closest two clusters according to the farthest neighbour criterion
//returns the maximum Euclidian distance between the closest cluster pair
float findClosestClusterPairCompleteLinkage(vector<Mat> clusters, int &i, int &j)
{
	float minDist = INF;
	int l;
	for (int k = 0; k < clusters.size(); k++)
	{
		float currDist = findClosestClusterCompleteLinkage(clusters, k, l);
		if (currDist < minDist)
		{
			minDist = currDist;
			i = k;
			j = l;
		}
		if (minDist == 0) break;
	}
	return minDist;
}

//hierarchical clustering algorithm using Complete linkage distance criterion.
//starts with the clusters equals to the training samples and unifies the clusters until pseudo-F test gives a maximum or there
//remains only one cluster.
int clusterHierarchicalCompleteLinkage(vector<Mat> &clusters, Mat trainingBuffer)
{
	int i, j;
	float pseudoFScore, pseudoFScoreMax;
	initializeClusters(clusters, trainingBuffer);
	vector<Mat> clustersMax = clusters;
	pseudoFScore = pseudoFScoreMax = 0;  //No of clusters are equal to the no of samples, hence zero pseudo-F score
	do
	{
		findClosestClusterPairCompleteLinkage(clusters, i, j);
		//cout << "Distance =	" << dist << endl;
		mergeClusters(clusters, i, j);
		pseudoFScore = calculatePseudoFScore(clusters, trainingBuffer);
		if (pseudoFScore > pseudoFScoreMax && clusters.size() > 1 && clusters.size() < 20)
		{
			pseudoFScoreMax = pseudoFScore;
			clustersMax = clusters;
		}
		cout << "Number of clusters : " << clusters.size() << endl;
		cout << "Number of clustersMax : " << clustersMax.size() << endl;
		cout << "pseudo-F Score : " << pseudoFScore << endl;
	}while(clusters.size() > 1);
	clustersMax = (pseudoFScoreMax == 0) ? clusters : clustersMax;
	clusters = clustersMax;
	return clusters.size();
}

////////////////////// SINGLE LINKAGE METHODS ///////////////////////

//Finds the closest Euclidian distance between two clusters of samples.
//Each row of cluster matrix holds one sample 
float findSingleDistance(Mat cluster1, Mat cluster2)
{
	float minDist = INF;
	for (int i = 0; i < cluster1.rows; i++)
		for (int j = 0; j < cluster2.rows; j++)
		{
			float currDist = 0;
			Mat diff = cluster1.row(i) - cluster2.row(j);
			diff = diff*diff.t();
			currDist = sqrt(*(diff.begin<double>()));
			minDist = (currDist < minDist) ? currDist : minDist;
		}
	return minDist;
}

//Finds the closest cluster to the cluster k according to the closest neighbour and returns the found distance
float findClosestClusterSingleLinkage(vector<Mat> clusters, int k, int &l)
{
	float minDist = INF;
	for (int i = 0; i < clusters.size(); i++)
		if (i != k)
		{
			float currDist = findSingleDistance(clusters[k],clusters[i]);
			if (currDist < minDist)
			{
				minDist = currDist;
				l = i;
				if (minDist == 0) break;
			}
		}
	return minDist;
}

//Given a vector of clusters, finds the index of the closest two clusters according to the closest neighbour criterion
//returns the minimum Euclidian distance between the closest cluster pair
float findClosestClusterPairSingleLinkage(vector<Mat> clusters, int &i, int &j)
{
	float minDist = INF;
	int l;
	for (int k = 0; k < clusters.size(); k++)
	{
		float currDist = findClosestClusterSingleLinkage(clusters, k, l);
		if (currDist < minDist)
		{
			minDist = currDist;
			i = k;
			j = l;
		}
		if (minDist == 0) break;
	}
	return minDist;
}

//hierarchical clustering algorithm using Single linkage distance criterion.
//starts with the clusters equals to the training samples and unifies the clusters until pseudo-F test gives a maximum or there
//remains only one cluster.
int clusterHierarchicalSingleLinkage(vector<Mat> &clusters, Mat trainingBuffer)
{
	int i, j;
	float pseudoFScore, pseudoFScoreMax;
	initializeClusters(clusters, trainingBuffer);
	vector<Mat> clustersMax = clusters;
	pseudoFScore = pseudoFScoreMax = 0;  //No of clusters are equal to the no of samples, hence zero pseudo-F score
	do
	{
		findClosestClusterPairSingleLinkage(clusters, i, j);
		//cout << "Distance =	" << dist << endl;
		mergeClusters(clusters, i, j);
		pseudoFScore = calculatePseudoFScore(clusters, trainingBuffer);
		if (pseudoFScore > pseudoFScoreMax && clusters.size() > 1 && clusters.size() < 20)
		{
			pseudoFScoreMax = pseudoFScore;
			clustersMax = clusters;
		}
		cout << "Number of clusters : " << clusters.size() << endl;
		cout << "Number of clustersMax : " << clustersMax.size() << endl;
		cout << "pseudo-F Score : " << pseudoFScore << endl;
	}while(clusters.size() > 1);
	clustersMax = (pseudoFScoreMax == 0) ? clusters : clustersMax;
	clusters = clustersMax;
	return clusters.size();
}


//////////////////////////////////////////////////////////////////////
/////////////////// K-MEANS CLUSTERING OPERATIONS ////////////////////
//////////////////////////////////////////////////////////////////////

//Clusters using kMeans clustering algorithm of OpenCV
void clusterKMeans(Mat trainingBuffer, int nClusters, Mat &labels, Mat &centers)
{
	kmeans(trainingBuffer, nClusters, labels,  TermCriteria( CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 1000, 0.01), 5, KMEANS_PP_CENTERS, centers);
}

//////////////////////////////////////////////////////////////////////
///////////////////// EM CLUSTERING OPERATIONS ///////////////////////
//////////////////////////////////////////////////////////////////////

//Clustering using Expectation Maximization algorithm (if nGaussians > 1) or MLE (if nGaussians = 1)
//Takes training buffer of histograms 1xd matrices
//Takes number of Gaussians to be found
//means, covs and weights should not be initialized
bool clusterEM(Mat trainingBuffer, int nGaussians, Mat &means, vector<Mat> &covs, Mat &weights)
{
	//if (nGaussians == 1)
	//{
	//	Mat var;
	//	Mat varDiag = Mat::zeros(trainingBuffer.cols, trainingBuffer.cols, CV_64FC1);
	//	calcCovarMatrix(trainingBuffer, var, means, CV_COVAR_NORMAL | CV_COVAR_ROWS);
	//	var = var / (trainingBuffer.rows - 1);
	//	for (int i = 0; i < trainingBuffer.cols; i++)
	//	{
	//		varDiag.at<double>(i,i) = var.at<double>(i,i);
	//	}
	//	covs.push_back(varDiag);
	//	return true;
	//}
	//if (nGaussians > 1)
	//{
		EM em = EM::EM(nGaussians, EM::COV_MAT_DIAGONAL, TermCriteria( CV_TERMCRIT_EPS+CV_TERMCRIT_ITER, 100, 0.1));
		if (em.train(trainingBuffer))
		{   
			covs = em.get<vector<Mat>>("covs");
			means = em.get<Mat>("means");
			weights = em.get<Mat>("weights");
			if (debug) cout << "MEAN: " << means.row(0) << endl;
			if (debug) cout << "COV: " << covs[0] << endl;
			return true;
		}
		return false;
	//}
	//return false;
}

//removes the k clusters (or gaussians) with the smallest weights from the model library but does not update the weights, only remove
void removekClusterswSmallestWeightCorrected(Mat &means, vector<Mat> &covs, vector<Mat> &covsInv, Mat &weights, int k)
{
	//double wTotFinal = 1;
	for (int i = 0; i < k; i++)
	{
		int indexMin = findIndexMinWeight(weights);
		//wTotFinal -= weights.at<double>(0,indexMin);
		if (indexMin == means.rows - 1 || indexMin == 0)
		{
			if (indexMin == means.rows - 1)
			{
				means.pop_back(1);
				Mat wTrans = weights.t();
				wTrans.pop_back(1);
				weights = wTrans.t();
			}
			else
			{
				Mat dummyMeans;
				Mat roiMeansDown = means(Range(1, means.rows),Range(0, means.cols));
				dummyMeans.push_back(roiMeansDown);
				means = dummyMeans;

				Mat dummyWeights;
				Mat roiWeightsDown = weights(Range(0, 1), Range(1, weights.cols));
				dummyWeights.push_back(roiWeightsDown);
				weights = dummyWeights;
			}
		}
		else
		{
			Mat dummyMeans;
			Mat roiMeansUp = means(Range(0,indexMin),Range(0,means.cols));
			Mat roiMeansDown = means(Range(indexMin + 1, means.rows),Range(0, means.cols));
			dummyMeans.push_back(roiMeansUp);
			dummyMeans.push_back(roiMeansDown);
			means = dummyMeans;

			Mat dummyWeights;
			Mat roiWeightsUp = weights(Range(0, 1), Range(0, indexMin)).t();
			Mat roiWeightsDown = weights(Range(0, 1), Range(indexMin + 1, weights.cols)).t();
			dummyWeights.push_back(roiWeightsUp);
			dummyWeights.push_back(roiWeightsDown);
			weights = dummyWeights.t();
		}
		covs.erase(covs.begin()+indexMin);
		covsInv.erase(covsInv.begin()+indexMin);
	}
	//weights = weights / wTotFinal;
}

//removes the k clusters (or gaussians) with the smallest weights from the model library
void removekClusterswSmallestWeight(Mat &means, vector<Mat> &covs, vector<Mat> &covsInv, Mat &weights, int k)
{
	double wTotFinal = 1;
	for (int i = 0; i < k; i++)
	{
		int indexMin = findIndexMinWeight(weights);
		wTotFinal -= weights.at<double>(0,indexMin);
		if (indexMin == means.rows - 1 || indexMin == 0)
		{
			if (indexMin == means.rows - 1)
			{
				means.pop_back(1);
				Mat wTrans = weights.t();
				wTrans.pop_back(1);
				weights = wTrans.t();
			}
			else
			{
				Mat dummyMeans;
				Mat roiMeansDown = means(Range(1, means.rows),Range(0, means.cols));
				dummyMeans.push_back(roiMeansDown);
				means = dummyMeans;

				Mat dummyWeights;
				Mat roiWeightsDown = weights(Range(0, 1), Range(1, weights.cols));
				dummyWeights.push_back(roiWeightsDown);
				weights = dummyWeights;
			}
		}
		else
		{
			Mat dummyMeans;
			Mat roiMeansUp = means(Range(0,indexMin),Range(0,means.cols));
			Mat roiMeansDown = means(Range(indexMin + 1, means.rows),Range(0, means.cols));
			dummyMeans.push_back(roiMeansUp);
			dummyMeans.push_back(roiMeansDown);
			means = dummyMeans;

			Mat dummyWeights;
			Mat roiWeightsUp = weights(Range(0, 1), Range(0, indexMin)).t();
			Mat roiWeightsDown = weights(Range(0, 1), Range(indexMin + 1, weights.cols)).t();
			dummyWeights.push_back(roiWeightsUp);
			dummyWeights.push_back(roiWeightsDown);
			weights = dummyWeights.t();
		}
		covs.erase(covs.begin()+indexMin);
		covsInv.erase(covsInv.begin()+indexMin);
	}
	weights = weights / wTotFinal;
}

//inserts newly learned clusters to the model library
void insertClusters2LibEMCorrected(Mat &means, vector<Mat> &covs, vector<Mat> &covsInv, Mat &weights, Mat meansNew, vector<Mat> covsNew, Mat weightNew)
{
	if (means.rows > n_clusters_max - n_clusters_new)
	{
		removekClusterswSmallestWeightCorrected(means, covs, covsInv, weights, n_clusters_new);
	}
	means.push_back(meansNew);
	Mat dummyWeights;//= Mat::zeros(1,weights.cols + weightNew.cols, CV_64FC1);
	dummyWeights.create(1,weights.cols + weightNew.cols, CV_64FC1);
	for (int i = 0; i < weights.cols; i++)
		dummyWeights.at<double>(0,i) = weights.at<double>(0,i);
	for (int i = 0; i < weightNew.cols; i++)
		dummyWeights.at<double>(0,i+weights.cols) = weightNew.at<double>(0,i);
	weights = dummyWeights;
	double wTot = 0;
	for (int i = 0; i < weights.cols; i++)
	{
		wTot += weights.at<double>(0,i);
		if (i < weightNew.cols)
		{
			covs.push_back(covsNew[i]);
			covsInv.push_back(invertDiagonalMatrix(covsNew[i]));
		}
	}
	weights = weights / wTot;
}

//inserts newly learned clusters to the model library
void insertClusters2LibEM(Mat &means, vector<Mat> &covs, vector<Mat> &covsInv, Mat &weights, Mat meansNew, vector<Mat> covsNew, Mat weightNew)
{
	if (means.rows > n_clusters_max - n_clusters_new)
	{
		removekClusterswSmallestWeight(means, covs, covsInv, weights, n_clusters_new);
	}
	means.push_back(meansNew);
	Mat dummyWeights;//= Mat::zeros(1,weights.cols + weightNew.cols, CV_64FC1);
	dummyWeights.create(1,weights.cols + weightNew.cols, CV_64FC1);
	for (int i = 0; i < weights.cols; i++)
		dummyWeights.at<double>(0,i) = weights.at<double>(0,i);
	for (int i = 0; i < weightNew.cols; i++)
		dummyWeights.at<double>(0,i+weights.cols) = weightNew.at<double>(0,i);
	weights = dummyWeights;
	double wTot = 1;
	for (int i = 0; i < meansNew.rows; i++)
	{
		wTot += weightNew.at<double>(0,i);
		covs.push_back(covsNew[i]);
		covsInv.push_back(invertDiagonalMatrix(covsNew[i]));
	}
	weights = weights / wTot;
}

////////////////// MODEL LIBRARY UPDATE METHODS ////////////////
////////////////////////////////////////////////////////////////

////////////////// VIA MAHALANOBIS DISTANCE ////////////////////

//updates the EM clusters according to the threshold given and returns remaining training samples.
//distance is calculated via Mahalanobis distance
void updateClustersEMMahCorrected(Mat &trainingBuffer, float threshold, Mat &means, vector<Mat> &covs, vector<Mat> &covsInv, Mat &weights)
{
	Mat meansUpdateSum = Mat::zeros(means.rows, means.cols, means.type());
	vector<Mat> covsUpdateSum(covs.size());
	//Doesnt like this one :((
	for (int i = 0; i < covs.size(); i++)
		covsUpdateSum[i] = Mat::zeros(means.cols, means.cols, means.type());
	Mat weightsUpdateSum = Mat::zeros(weights.rows, weights.cols, weights.type());

	int nTB = trainingBuffer.rows;
	double w = (1.0/nTB);
	Mat remainingBuffer;
	
	for (int i = nTB - 1; i > -1 ; i--)
	{
		Mat roi = trainingBuffer(Range(i,i+1),Range(0,trainingBuffer.cols));
		int c = findClosestClusterMah(means, covsInv, roi, threshold);
		if (c != -1)
		{
			meansUpdateSum.row(c) += w * roi;
			covsUpdateSum[c] += w * ((roi - means.row(c)).t() * (roi - means.row(c)));
			weightsUpdateSum.at<double>(0,c) += w;

			/*
			double wOld = weights.at<double>(0,c);
			Mat roiMeans = means.row(c);
			MatIterator_<double> itRoi = roi.begin<double>();
			MatIterator_<double> itMeans = roiMeans.begin<double>();
			MatIterator_<double> end = roiMeans.end<double>();
			for (int j = 0; j < means.cols; j++)
			{
				//update the corresponding mean
				(*itMeans) = (wOld * (*itMeans) + w * (*itRoi))/(wOld + w);
				//update the corresponding cov
				double dummyCov =  ((*itRoi) - (*itMeans))*((*itRoi) - (*itMeans));
				covs[c].at<double>(j,j) = (wOld * covs[c].at<double>(j,j) + w * dummyCov)/(wOld + w);
			}
			//update inverse covariance matrix
			covsInv[c] = invertDiagonalMatrix(covs[c]);
			//update weights
			weights.at<double>(0,c) = (wOld + w);
			weights = weights/(1+w);
			*/
		}
		else
			remainingBuffer.push_back(roi);
		trainingBuffer.pop_back(1);
	}
	//Update the models
	for (int i = 0; i < means.rows; i++)
	{
		double wOld = weights.at<double>(0,i);
		double weightTot = wOld + weightsUpdateSum.at<double>(0,i);
		means.row(i) = (wOld * means.row(i) + meansUpdateSum.row(i)) / weightTot;  
		covs[i] = (wOld * covs[i] + covsUpdateSum[i]) / weightTot;
	}
	//Update weights
	if (remainingBuffer.rows == 0 || remainingBuffer.rows > n_samples_min)
		weights = (weights + weightsUpdateSum) / 2;
	else
		weights = (weights + weightsUpdateSum) / (2 - (double)(remainingBuffer.rows)/(double)(nTB));

	trainingBuffer = remainingBuffer;
}

//updates the EM clusters according to the threshold given and returns remaining training samples.
//distance is calculated via Mahalanobis distance
void updateClustersEMMah(Mat &trainingBuffer, float threshold, Mat &means, vector<Mat> &covs, vector<Mat> &covsInv, Mat &weights)
{
	int nTB = trainingBuffer.rows;
	double w = (1.0/nTB);
	Mat remainingBuffer;
	
	for (int i = nTB - 1; i > -1 ; i--)
	{
		Mat roi = trainingBuffer(Range(i,i+1),Range(0,trainingBuffer.cols));
		int c = findClosestClusterMah(means, covsInv, roi, threshold);
		if (c != -1)
		{
			double wOld = weights.at<double>(0,c);
			Mat roiMeans = means.row(c);
			MatIterator_<double> itRoi = roi.begin<double>();
			MatIterator_<double> itMeans = roiMeans.begin<double>();
			MatIterator_<double> end = roiMeans.end<double>();
			for (int j = 0; j < means.cols; j++)
			{
				//update the corresponding mean
				(*itMeans) = (wOld * (*itMeans) + w * (*itRoi))/(wOld + w);
				//update the corresponding cov
				double dummyCov =  ((*itRoi) - (*itMeans))*((*itRoi) - (*itMeans));
				covs[c].at<double>(j,j) = (wOld * covs[c].at<double>(j,j) + w * dummyCov)/(wOld + w);
			}
			//update inverse covariance matrix
			covsInv[c] = invertDiagonalMatrix(covs[c]);
			//update weights
			weights.at<double>(0,c) = (wOld + w);
			weights = weights/(1+w);
		}
		else
			remainingBuffer.push_back(roi);
		trainingBuffer.pop_back(1);
	}
	trainingBuffer = remainingBuffer;
}

////////////////// VIA EUCLIDIAN DISTANCE ////////////////////

//updates the EM clusters according to the threshold given and returns remaining training samples.
//distance is calculated via Euclidian distance
void updateClustersEMEuc(Mat &trainingBuffer, float threshold, Mat &means, Mat &weights)
{
	int nTB = trainingBuffer.rows;
	double w = (1.0/nTB);
	Mat remainingBuffer;
	
	for (int i = nTB - 1; i > -1 ; i--)
	{
		Mat roi = trainingBuffer(Range(i,i+1),Range(0,trainingBuffer.cols));
		int c = findClosestClusterEuc(means, roi, threshold);
		if (c != -1)
		{
			double wOld = weights.at<double>(0,c);
			Mat roiMeans = means.row(c);
			MatIterator_<double> itRoi = roi.begin<double>();
			MatIterator_<double> itMeans = roiMeans.begin<double>();
			MatIterator_<double> end = roiMeans.end<double>();
			for (int j = 0; j < means.cols; j++)
			{
				//update the corresponding mean
				(*itMeans) = (wOld * (*itMeans) + w * (*itRoi))/(wOld + w);
			}
			//update weights
			weights.at<double>(0,c) = (wOld + w);
			weights = weights/(1+w);
		}
		else
			remainingBuffer.push_back(roi);
		trainingBuffer.pop_back(1);
	}
	trainingBuffer = remainingBuffer;
}

////////////////// VIA MANHATTAN DISTANCE ////////////////////

//updates the EM clusters according to the threshold given and returns remaining training samples.
//distance is calculated via Manhattan distance
void updateClustersEMMan(Mat &trainingBuffer, float threshold, Mat &means, Mat &weights)
{
	int nTB = trainingBuffer.rows;
	double w = (1.0/nTB);
	Mat remainingBuffer;
	
	for (int i = nTB - 1; i > -1 ; i--)
	{
		Mat roi = trainingBuffer(Range(i,i+1),Range(0,trainingBuffer.cols));
		int c = findClosestClusterMan(means, roi, threshold);
		if (c != -1)
		{
			double wOld = weights.at<double>(0,c);
			Mat roiMeans = means.row(c);
			MatIterator_<double> itRoi = roi.begin<double>();
			MatIterator_<double> itMeans = roiMeans.begin<double>();
			MatIterator_<double> end = roiMeans.end<double>();
			for (int j = 0; j < means.cols; j++)
			{
				//update the corresponding mean
				(*itMeans) = (wOld * (*itMeans) + w * (*itRoi))/(wOld + w);
			}
			//update weights
			weights.at<double>(0,c) = (wOld + w);
			weights = weights/(1+w);
		}
		else
			remainingBuffer.push_back(roi);
		trainingBuffer.pop_back(1);
	}
	trainingBuffer = remainingBuffer;
}

////////////////// VIA CHEBYSHEV DISTANCE ////////////////////

//updates the EM clusters according to the threshold given and returns remaining training samples.
//distance is calculated via Chebyshev distance
void updateClustersEMCheb(Mat &trainingBuffer, float threshold, Mat &means, Mat &weights)
{
	int nTB = trainingBuffer.rows;
	double w = (1.0/nTB);
	Mat remainingBuffer;
	
	for (int i = nTB - 1; i > -1 ; i--)
	{
		Mat roi = trainingBuffer(Range(i,i+1),Range(0,trainingBuffer.cols));
		int c = findClosestClusterCheb(means, roi, threshold);
		if (c != -1)
		{
			double wOld = weights.at<double>(0,c);
			Mat roiMeans = means.row(c);
			MatIterator_<double> itRoi = roi.begin<double>();
			MatIterator_<double> itMeans = roiMeans.begin<double>();
			MatIterator_<double> end = roiMeans.end<double>();
			for (int j = 0; j < means.cols; j++)
			{
				//update the corresponding mean
				(*itMeans) = (wOld * (*itMeans) + w * (*itRoi))/(wOld + w);
			}
			//update weights
			weights.at<double>(0,c) = (wOld + w);
			weights = weights/(1+w);
		}
		else
			remainingBuffer.push_back(roi);
		trainingBuffer.pop_back(1);
	}
	trainingBuffer = remainingBuffer;
}

////////////////// VIA HELLINGER DISTANCE ////////////////////

////////////////// FOR HISTOGRAMS ////////////////////////////

//updates the EM clusters according to the threshold given and returns remaining training samples.
//distance is calculated via Hellinger distance
void updateClustersEMHell(Mat &trainingBuffer, float threshold, Mat &means, Mat &weights)
{
	int nTB = trainingBuffer.rows;
	double w = (1.0/nTB);
	Mat remainingBuffer;
	
	for (int i = nTB - 1; i > -1 ; i--)
	{
		Mat roi = trainingBuffer(Range(i,i+1),Range(0,trainingBuffer.cols));
		int c = findClosestClusterHellHist(means, roi, threshold);
		if (c != -1)
		{
			double wOld = weights.at<double>(0,c);
			Mat roiMeans = means.row(c);
			MatIterator_<double> itRoi = roi.begin<double>();
			MatIterator_<double> itMeans = roiMeans.begin<double>();
			MatIterator_<double> end = roiMeans.end<double>();
			for (int j = 0; j < means.cols; j++)
			{
				//update the corresponding mean
				(*itMeans) = (wOld * (*itMeans) + w * (*itRoi))/(wOld + w);
				//update the corresponding cov
				//double dummyCov =  ((*itRoi) - (*itMeans))*((*itRoi) - (*itMeans));
				//covs[c].at<double>(j,j) = (wOld * covs[c].at<double>(j,j) + w * dummyCov)/(wOld + w);
			}
			//update inverse covariance matrix
			//covsInv[c] = invertDiagonalMatrix(covs[c]);
			//update weights
			weights.at<double>(0,c) = (wOld + w);
			weights = weights/(1+w);
		}
		else
			remainingBuffer.push_back(roi);
		trainingBuffer.pop_back(1);
	}
	trainingBuffer = remainingBuffer;
}

////////////////// VIA CHI-SQUARE DISTANCE ////////////////////

////////////////// FOR HISTOGRAMS ////////////////////////////

//updates the EM clusters according to the threshold given and returns remaining training samples.
//distance is calculated via Chi-square distance
void updateClustersEMChi(Mat &trainingBuffer, float threshold, Mat &means, Mat &weights)
{
	int nTB = trainingBuffer.rows;
	double w = (1.0/nTB);
	Mat remainingBuffer;
	
	for (int i = nTB - 1; i > -1 ; i--)
	{
		Mat roi = trainingBuffer(Range(i,i+1),Range(0,trainingBuffer.cols));
		int c = findClosestClusterChiHist(means, roi, threshold);
		if (c != -1)
		{
			double wOld = weights.at<double>(0,c);
			Mat roiMeans = means.row(c);
			MatIterator_<double> itRoi = roi.begin<double>();
			MatIterator_<double> itMeans = roiMeans.begin<double>();
			MatIterator_<double> end = roiMeans.end<double>();
			for (int j = 0; j < means.cols; j++)
			{
				//update the corresponding mean
				(*itMeans) = (wOld * (*itMeans) + w * (*itRoi))/(wOld + w);
				//update the corresponding cov
				//double dummyCov =  ((*itRoi) - (*itMeans))*((*itRoi) - (*itMeans));
				//covs[c].at<double>(j,j) = (wOld * covs[c].at<double>(j,j) + w * dummyCov)/(wOld + w);
			}
			//update inverse covariance matrix
			//covsInv[c] = invertDiagonalMatrix(covs[c]);
			//update weights
			weights.at<double>(0,c) = (wOld + w);
			weights = weights/(1+w);
		}
		else
			remainingBuffer.push_back(roi);
		trainingBuffer.pop_back(1);
	}
	trainingBuffer = remainingBuffer;
}


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
//models and weights should not be initialized
//CvSVMParams params are the algorithm parameters
bool clusterSVM(vector<Mat> trainingClusters, vector<CvSVM> &models, Mat &weights, CvSVMParams params)
{
	int nTot = 0;
	Mat dummyWeights = Mat(1, trainingClusters.size(), CV_64FC1);
	for (int i = 0; i < trainingClusters.size(); i++)
		nTot += trainingClusters[i].rows;
	CvSVM dummySVM = CvSVM::CvSVM();
	for (int i = 0; i < trainingClusters.size(); i++)
	{
		models.push_back(dummySVM);
		Mat lbl = Mat::eye(trainingClusters[i].rows, 1, CV_32FC1);
		Mat data;
		trainingClusters[i].convertTo(data, CV_32FC1); 
		if (models[models.size() - 1].train(data, lbl, Mat(), Mat(), params))
		{
			
			//models.push_back(dummySVM);
			//models.push_back();
			dummyWeights.at<double>(0,i) = trainingClusters[i].rows / nTot;
		}
		else
			return false;
	}
	weights = dummyWeights;
	return true;
}


//If the training sample is enclosed by one of the old models, delete it and increase that model's weight
void updateClustersSVM(Mat &trainingBuffer, vector<CvSVM> &models, Mat &weights)
{
	Mat dummyTrainingBuffer;
	double d = 1.0/trainingBuffer.rows;
	double divider = 1.0 + d;
	for (int i = 0; i < trainingBuffer.rows; i++)
	{
		int predictCounter = 0;
		Mat roi = trainingBuffer(Range(i,i+1),Range(0,trainingBuffer.cols));
		Mat roi32;
		roi.convertTo(roi32, CV_32FC1);
		for (int j = 0; j < models.size(); j++)
			if ( models[j].predict(roi32) == 1 )
			{
				predictCounter++;
				weights.at<double>(0,j) += d;
				weights /= divider;
				break;
			}
		if (predictCounter == 0)
			dummyTrainingBuffer.push_back(roi);
	}
	trainingBuffer = dummyTrainingBuffer;
}

void insertClusters2LibSVM(vector<CvSVM> &models, Mat &weights, vector<CvSVM> modelsNew, Mat weightsNew)
{
	if (models.size() > n_clusters_max - modelsNew.size())
	{
		removekClusterswSmallestWeightSVM(models, weights, modelsNew.size());
	}
	for (int i = 0; i < modelsNew.size(); i++)
		models.push_back(modelsNew[i]);
	Mat dummyWeights;//= Mat::zeros(1,weights.cols + weightNew.cols, CV_64FC1);
	dummyWeights.create(1,weights.cols + weightsNew.cols, CV_64FC1);
	for (int i = 0; i < weights.cols; i++)
		dummyWeights.at<double>(0,i) = weights.at<double>(0,i);
	for (int i = 0; i < weightsNew.cols; i++)
		dummyWeights.at<double>(0,i+weights.cols) = weightsNew.at<double>(0,i);
	weights = dummyWeights;
	double wTot = 1;
	for (int i = 0; i < modelsNew.size(); i++)
	{
		wTot += weightsNew.at<double>(0,i);
		//covs.push_back(covsNew[i]);
		//covsInv.push_back(invertDiagonalMatrix(covsNew[i]));
	}
	weights = weights / wTot;
}

//removes the k clusters with the smallest weights from the model library
void removekClusterswSmallestWeightSVM(vector<CvSVM> &models, Mat &weights, int k)
{
	double wTotFinal = 1;
	for (int i = 0; i < k; i++)
	{
		int indexMin = findIndexMinWeight(weights);
		wTotFinal -= weights.at<double>(0,indexMin);
		if (indexMin == models.size() - 1 || indexMin == 0)
		{
			if (indexMin == models.size() - 1)
			{
				models.pop_back();
				Mat wTrans = weights.t();
				wTrans.pop_back(1);
				weights = wTrans.t();
			}
			else
			{
				models.erase(models.begin());

				Mat dummyWeights;
				Mat roiWeightsDown = weights(Range(0, 1), Range(1, weights.cols));
				dummyWeights.push_back(roiWeightsDown);
				weights = dummyWeights;
			}
		}
		else
		{
			models.erase(models.begin() + indexMin);
			
			Mat dummyWeights;
			Mat roiWeightsUp = weights(Range(0, 1), Range(0, indexMin)).t();
			Mat roiWeightsDown = weights(Range(0, 1), Range(indexMin + 1, weights.cols)).t();
			dummyWeights.push_back(roiWeightsUp);
			dummyWeights.push_back(roiWeightsDown);
			weights = dummyWeights.t();
		}
		//covs.erase(covs.begin()+indexMin);
		//covsInv.erase(covsInv.begin()+indexMin);
	}
	weights = weights / wTotFinal;
}
*/

/////////////////////////// SINGLE MODEL //////////////////////////////

//Clustering using One-Class SVM
//Takes training samples library 
bool clusterSVMSingle(vector<Mat> trainingBufferLib, CvSVM &model, CvSVMParams params)
{
	Mat dummyTrainingBuffer;
	for (int i = 0; i < trainingBufferLib.size(); i++)
	{
		dummyTrainingBuffer.push_back(trainingBufferLib[i]);
	}
	Mat lbl = Mat::eye(dummyTrainingBuffer.rows, 1, CV_32FC1);
	Mat data;
	dummyTrainingBuffer.convertTo(data, CV_32FC1); 
	if (model.train(data, lbl, Mat(), Mat(), params))
	{
		return true;
	}
	return false;
}

void removeOldSamplesFromLibSVMSingle(vector<Mat> &trainingBufferLib)
{
	int n = trainingBufferLib.size() - n_clusters_max;
	if ( n > 0 )
		trainingBufferLib.erase(trainingBufferLib.begin(), trainingBufferLib.begin() + n);
}