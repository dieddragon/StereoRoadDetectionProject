#include "main.h"

//////////////////////////////////////////////////////////////////////
/////////////////////// POINT CLOUD PROCESSING ///////////////////////
//////////////////////////////////////////////////////////////////////

//from a patch of xyz values, calculates the unbiased variance of y 
//xyzPatch should be a 3-channel matrix of doubles
double calculateVarianceYforPatch(Mat xyzPatch)
{
	int n = xyzPatch.rows * xyzPatch.cols;
	double sum = 0, sumSqrt = 0, dummy = 0;
	MatIterator_<Vec3d> end = xyzPatch.end<Vec3d>();
	for (MatIterator_<Vec3d> it = xyzPatch.begin<Vec3d>(); it != end; ++it)
	{
		dummy = (*it)[1];
		sum += dummy;
		sumSqrt += dummy * dummy;
	}
	return (sumSqrt - ((sum*sum)/n))/(n-1);
}

//from the matrix of xyz values (double), calculates the unbiased variance of y for all patches 
//xyzMat should be a 3-channel matrix of doubles.
//sizePatch is the number of pixels in one side of the patch
Mat calculateVarianceYforPatches(Mat xyzMat, int sizePatch)
{
	int nRows = xyzMat.rows / sizePatch;
	int nCols = xyzMat.cols / sizePatch;
	Mat varYPatches; //= Mat::zeros(nRows, nCols, CV_64FC1);
	varYPatches.create(nRows, nCols, CV_64FC1);
	MatIterator_<double> it = varYPatches.begin<double>();
	for (int i = 0; i < nRows; i++)
		for (int j = 0; j < nCols; j++)
		{
			Mat roi = xyzMat(Range(i*sizePatch,(i+1)*sizePatch),Range(j*sizePatch,(j+1)*sizePatch));
			(*it) = calculateVarianceYforPatch(roi);
			it++;
		}
	return varYPatches;
}

//calculates the maximum difference in Y coord within the patch
//returns the absolute value of it.
//xyzPatch should be a 3-channel matrix of doubles.
double calculateDifferenceMaxYforPatch(Mat xyzPatch)
{
	double max = -INF;
	double min = INF;
	double res = 0, curr = 0;
	MatIterator_<Vec3d> end = xyzPatch.end<Vec3d>();
	for (MatIterator_<Vec3d> it = xyzPatch.begin<Vec3d>(); it != end; ++it)
	{
		curr = (*it)[1];
		if (curr > max)
			max = curr;
		else if (curr < min)
			min = curr;
	}
	res = max - min;
	if (res < 0) return -res;
	return res;
}

//calculates the maximum difference in Y coord within the patch
//returns the absolute value of it.
//xyzMat should be a 3-channel matrix of doubles.
//sizePatch is the number of pixels in one side of the patch.
Mat calculateDifferenceMaxYforPatches(Mat xyzMat, int sizePatch)
{
	int nRows = xyzMat.rows / sizePatch;
	int nCols = xyzMat.cols / sizePatch;
	Mat diffMaxYPatches = Mat::zeros(nRows, nCols, CV_64FC1);
	//diffMaxYPatches.create(nRows, nCols, CV_64FC1);
	MatIterator_<double> it = diffMaxYPatches.begin<double>();
	for (int i = 0; i < nRows; i++)
		for (int j = 0; j < nCols; j++)
		{
			Mat roi = xyzMat(Range(i*sizePatch,(i+1)*sizePatch),Range(j*sizePatch,(j+1)*sizePatch));
			(*it) = calculateDifferenceMaxYforPatch(roi);
			it++;
		}
	return diffMaxYPatches;
}

//////////////////////////////////////////////////////////////////////
////////////////////////// COLOR PROCESSING //////////////////////////
//////////////////////////////////////////////////////////////////////

//Filters the hsv image and point cloud data according to the saturation channel of hsv image
//Pixels which have a saturation channel value greater than "thresholdSatBandMin" passes
//while others grounded to zero
void filterSatChannel(Mat xyzMat, Mat hsvMat, int thresholdSatBandMin)
{
	MatIterator_<Vec<uchar,3>> itHSV = hsvMat.begin<Vec<uchar,3>>();
	MatIterator_<Vec<double,3>> itXYZ = xyzMat.begin<Vec<double,3>>();
	MatIterator_<Vec<uchar,3>> endHSV = hsvMat.end<Vec<uchar,3>>();
	MatIterator_<Vec<double,3>> endXYZ = xyzMat.end<Vec<double,3>>();

	for (; itHSV != endHSV; itHSV++)
	{
		if ((*itHSV)[1] < thresholdSatBandMin)
		{
			//(*itHSV)[0] = 0;
			//(*itHSV)[1] = 0;
			//(*itHSV)[2] = 0;
			(*itXYZ)[0] = 0;
			(*itXYZ)[1] = 0;
			(*itXYZ)[2] = 0;
		}
		itXYZ++;
	}
}

//Filters the hsv image and point cloud data according to the value channel of hsv image
//Pixels which have a value channel value between "thresholdValBandMin" and "thresholdValBandMax" passes
//while others grounded to zero
void filterValueChannel(Mat xyzMat, Mat hsvMat, int thresholdValBandMin, int thresholdValBandMax)
{
	MatIterator_<Vec<uchar,3>> itHSV = hsvMat.begin<Vec<uchar,3>>();
	MatIterator_<Vec<double,3>> itXYZ = xyzMat.begin<Vec<double,3>>();
	MatIterator_<Vec<uchar,3>> endHSV = hsvMat.end<Vec<uchar,3>>();
	MatIterator_<Vec<double,3>> endXYZ = xyzMat.end<Vec<double,3>>();

	for (; itHSV != endHSV; itHSV++)
	{
		if ((*itHSV)[2] < thresholdValBandMin || (*itHSV)[2] > thresholdValBandMax)
		{
			(*itHSV)[0] = 0;
			(*itHSV)[1] = 0;
			(*itHSV)[2] = 0;
			(*itXYZ)[0] = 0;
			(*itXYZ)[1] = 0;
			(*itXYZ)[2] = 0;
		}
		itXYZ++;
	}
}

//takes a 3 channel (HSV) matrix of uchars and means of each channel, and calculates the 3 elements of covariance matrix
//and store the square root of them into the last 3 elements of means vector
//means is a matrix of 1 channels of doubles
void calculateCovs2D(Mat m, Mat &means)
{
	MatIterator_<double> itMeans = means.begin<double>();
	double var00 = 0, var11 = 0, var01 = 0;
	double meanH = sqrt((*itMeans));
	double meanS = sqrt(*(++itMeans));
	double divider = m.rows * m.cols - 1;

	MatIterator_<Vec<uchar,3>> it = m.begin<Vec<uchar,3>>();
	MatIterator_<Vec<uchar,3>> end = m.end<Vec<uchar,3>>();
	for (; it != end; ++it)
	{
		double h = (*it)[0];
		double s = (*it)[1];
		var00 += ((h-meanH)*(h-meanH));
		var11 += ((s-meanS)*(s-meanS));
		var01 += ((h-meanH)*(s-meanS));
	}
	*(++itMeans) = var00 / divider;
	*(++itMeans) = var11 / divider;
	*(++itMeans) = var01 / divider;
}

//takes a 3 channel (HSV) matrix of uchars and calculates means of H and S channels and store them into the first two elements of means
//means is a matrix of 1 channels of doubles
void calculateMeans2D(Mat m, Mat &means)
{
	double tot0 = 0, tot1 = 0;
	MatIterator_<Vec<uchar,3>> it = m.begin<Vec<uchar,3>>();
	MatIterator_<Vec<uchar,3>> end = m.end<Vec<uchar,3>>();
	for (; it != end; ++it)
	{
		tot0 += (*it)[0];
		tot1 += (*it)[1];
	}
	double nPixels = m.rows * m.cols;
	MatIterator_<double> itMeans = means.begin<double>();
	*itMeans = pow(tot0 / nPixels,2);
	*(++itMeans) = pow(tot1 / nPixels,2);
}

//calculates the 2D HS Gaussians of image patches and return them as a matrix of 5 dims vector (2 means + 3 std devs)  
vector<vector<Mat>> calculateGaussiansHS2D(Mat m, int sizePatch)
{
	int nRows = m.rows / sizePatch;
	int nCols = m.cols / sizePatch;
	vector<vector<Mat>> gaussians;
	gaussians.resize( nRows, vector<Mat> (nCols, Mat()));

	//iterators
	vector<vector<Mat>>::iterator it = gaussians.begin();
	vector<Mat>::iterator itInner;
	
	for (int i = 0; i < nRows; i++, ++it)
	{
		itInner = (*it).begin();
		for (int j = 0; j < nCols; j++, ++itInner)
		{
			Mat gaussianDummy;// = Mat::zeros(1, 6, CV_64FC1);
			gaussianDummy.create(1, 5, CV_64FC1);
			Mat roi = m(Range(i*sizePatch,(i+1)*sizePatch),Range(j*sizePatch,(j+1)*sizePatch));
			calculateMeans2D(roi, gaussianDummy);
			calculateCovs2D(roi, gaussianDummy);
			(*itInner) = gaussianDummy;
		}
	}
	return gaussians;
}

//takes a 3 channel matrix of uchars and means of each channel, and calculates the 6 elements of covariance matrix
//and store the square root of them into the last 6 elements of means vector
//means is a matrix of 1 channels of doubles
void calculateCovs3D(Mat m, Mat &means)
{
	MatIterator_<double> itMeans = means.begin<double>();
	double var00 = 0, var11 = 0, var22 = 0, var01 = 0, var02 = 0, var12 = 0;
	double meanB = sqrt((*itMeans));
	double meanG = sqrt(*(++itMeans));
	double meanR = sqrt(*(++itMeans));
	double divider = m.rows * m.cols - 1;
	MatIterator_<Vec<uchar,3>> it = m.begin<Vec<uchar,3>>();
	MatIterator_<Vec<uchar,3>> end = m.end<Vec<uchar,3>>();
	for (; it != end; ++it)
	{
		double b = (*it)[0];
		double g = (*it)[1];
		double r = (*it)[2];
		var00 += ((b-meanB)*(b-meanB));
		var11 += ((g-meanG)*(g-meanG));
		var22 += ((r-meanR)*(r-meanR));
		var01 += ((b-meanB)*(g-meanG));
		var02 += ((b-meanB)*(r-meanR));
		var12 += ((g-meanG)*(r-meanR));
	}
	*(++itMeans) = var00 / divider;
	*(++itMeans) = var11 / divider;
	*(++itMeans) = var22 / divider;
	*(++itMeans) = var01 / divider;
	*(++itMeans) = var02 / divider;
	*(++itMeans) = var12 / divider;
}

//takes a 3 channel matrix of uchars and calculates means of each channel and store them into the first three elements of means
//means is a matrix of 1 channels of doubles
void calculateMeans3D(Mat m, Mat &means)
{
	MatIterator_<Vec<uchar,3>> it = m.begin<Vec<uchar,3>>();
	MatIterator_<Vec<uchar,3>> end = m.end<Vec<uchar,3>>();
	double tot0 = 0, tot1 = 0, tot2 = 0;
	for (; it != end; ++it)
	{
		tot0 += (*it)[0];
		tot1 += (*it)[1];
		tot2 += (*it)[2];
	}
	double nPixels = m.rows * m.cols;
	MatIterator_<double> itMeans = means.begin<double>();
	*itMeans = pow(tot0 / nPixels,2);
	*(++itMeans) = pow(tot1 / nPixels,2);
	*(++itMeans) = pow(tot2 / nPixels,2);
}

//calculates the 3D BGR Gaussians of image patches and return them as a matrix of 9 dims vector (3 means + 6 std devs)  
vector<vector<Mat>> calculateGaussiansBGR3D(Mat m, int sizePatch)
{
	int nRows = m.rows / sizePatch;
	int nCols = m.cols / sizePatch;
	vector<vector<Mat>> gaussians;
	gaussians.resize( nRows, vector<Mat> (nCols, Mat()));

	//iterators
	vector<vector<Mat>>::iterator it = gaussians.begin();
	vector<Mat>::iterator itInner;

	for (int i = 0; i < nRows; i++, ++it)
	{
		itInner = (*it).begin();
		for (int j = 0; j < nCols; j++, ++itInner)
		{
			Mat gaussianDummy;// = Mat::zeros(1, 12, CV_64FC1);
			gaussianDummy.create(1, 9, CV_64FC1);
			Mat roi = m(Range(i*sizePatch,(i+1)*sizePatch),Range(j*sizePatch,(j+1)*sizePatch));
			calculateMeans3D(roi, gaussianDummy);
			calculateCovs3D(roi, gaussianDummy);
			(*itInner) = gaussianDummy;
		}
	}
	return gaussians;
}

//calculates the joint histograms of image patches and return them as a matrix of histograms
vector<vector<Mat>> calculateHistograms(Mat m, int sizePatch, const int* channels, int dims, const int* histSize, const float** ranges, bool isNormalized)
{
	int nRows = m.rows / sizePatch;
	int nCols = m.cols / sizePatch;
	int nColsHist = pow((double)n_bins,(double)dims);
	vector<vector<Mat>> hists;
	hists.resize( nRows , vector<Mat>( nCols , Mat()) );
	
	//iterators
	vector<vector<Mat>>::iterator it = hists.begin();
	vector<Mat>::iterator itInner;
	MatIterator_<double> itDummy;
	MatIterator_<float> itDummyND;
	MatIterator_<float> endDummyND;
	
	for (int i = 0; i < nRows; i++)
	{
		itInner = (*it).begin();
		for (int j = 0; j < nCols; j++)
		{
			MatND histDummyND;
			Mat histDummy;
			histDummy.create(1,nColsHist,CV_64FC1);
			Mat roi = m(Range(i*sizePatch,(i+1)*sizePatch),Range(j*sizePatch,(j+1)*sizePatch));
			calcHist(&roi, 1, channels, Mat(), histDummyND, dims, histSize, ranges, true, false);
			if (isNormalized)
				normalize(histDummyND, histDummyND, 0, 1, NORM_MINMAX, -1, Mat()); 
			itDummy = histDummy.begin<double>();
			itDummyND = histDummyND.begin<float>();
			endDummyND = histDummyND.end<float>();
			for (; itDummyND != endDummyND; ++itDummy, ++itDummyND)
				(*itDummy) = (*itDummyND);
			(*itInner) = histDummy;
			itInner++;
		}
		it++;
	}
	return hists;
}

//given a matrix of y variance or y diff patches, original image and the patch size, the road and non-road areas are
//overlaid on the original image. Green shows road, reddish shows non-road.
//Patches should be a matrix of doubles and img should be a 3-channel matrix of uchar
Mat transformPatches2Color3Chi(Mat patches, Mat img, int sizePatch)
{
	Mat outputImg = img;//.clone();
	MatIterator_<Vec3b> it = outputImg.begin<Vec3b>();
	double val;
	int rw = width % sizePatch;
	int rh = height % sizePatch;
	int w = width - rw;
	int h = height - rh;
	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			val = patches.at<double>(i/sizePatch,j/sizePatch);
			(*it)[1] = (val == 255) ? 255 : 0;
			it++;
		}
		it += rw;
	}
	return outputImg;
}

//thresholds the source patches and make their values to 255 if smaller than threshold
//make them 0 else. Source matrix should be 1 channel and be of type double
Mat thresholdPatches1Chi(Mat source, double threshold, int *ntr)
{
	Mat outputMat;// = source.clone();
	outputMat.create(source.rows, source.cols, CV_64FC1);
	MatIterator_<double> it = source.begin<double>();
	MatIterator_<double> itOutput = outputMat.begin<double>();
	MatIterator_<double> end = source.end<double>();
	for (; it != end; ++it, ++itOutput)
	{
		(*itOutput) = (((*it) < threshold) && ((*it) != 0)) ? 255 : 0;
		if (((*it) < threshold) && ((*it) != 0))
			(*ntr)++; 
	}
	return outputMat;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////// BLOB FINDING ////////////////////////////
//////////////////////////////////////////////////////////////////////

//finds the blobs in the given image patch. In this case a matrix of doubles which only has either 255 or 0 are used.
void findBlobs(Mat imgPatch, vector<blob> &blobs)
{
    blobs.clear();

    // Fill the label_image with the blobs
    // 0  - background
    // 1  - unlabelled foreground
    // 2+ - labelled foreground

	Mat label_image;
    imgPatch.convertTo(label_image, CV_32FC1); // weird it doesn't support CV_32S!
    int label_count = 2; // starts at 2 because 0,1 are used already
	
	MatIterator_<float> it = label_image.begin<float>(); 
    for(int y = 0; y < imgPatch.rows; y++) {
        for(int x=0; x < imgPatch.cols; x++) {
            if((*it) != 255) {
				it++;
                continue;
            }			
            Rect rect;
            floodFill(label_image, Point(x,y), Scalar(label_count), &rect, Scalar(0), Scalar(0), 4);
			blob blob;
			blob.count = 0;

			Mat roi = label_image(Range(rect.y, rect.y+rect.height), Range(rect.x, rect.x+rect.width));
			MatIterator_<float> roiIt = roi.begin<float>();
			int nRowsRect = rect.y+rect.height;
			int nColsRect = rect.x+rect.width;
            for(int i=rect.y; i < nRowsRect; i++) {
                for(int j=rect.x; j < nColsRect; j++) {
                    if((*roiIt) != label_count) {
						roiIt++;
                        continue;
                    }

					blob.patchCoords.push_back(cv::Point2i(j,i));
					blob.count++;
					roiIt++;
                }
            }

            blobs.push_back(blob);
            label_count++;
			it++;
        }
    }
}

//Debug mode
Mat filterBiggestBlob(Mat bgrMat, Mat patchesVarThresholded, Mat &resultantMat)
{
	Mat patchesBiggestBlob = Mat::zeros(height/size_patch, width/size_patch, CV_64FC1);
	vector<blob> blobs;
	findBlobs(patchesVarThresholded, blobs);
	int indexMax = -1;
	int countMax = -1;
	for (int i = 0; i < blobs.size(); i++)
	{
		if (blobs[i].count > countMax)
		{
			countMax = blobs[i].count;
			indexMax = i;
		} 
	}
	for (int i = 0; i < blobs[indexMax].patchCoords.size(); i++)
			patchesBiggestBlob.at<double>(blobs[indexMax].patchCoords[i].y,blobs[indexMax].patchCoords[i].x) = 255.0;

	resultantMat = transformPatches2Color3Chi(patchesBiggestBlob, bgrMat, size_patch);
	return patchesBiggestBlob;
}

//filters out the biggest blob in the given matrix of y variance or y difference patches (matrix of doubles)
//and returns the image with the biggest found road area
//patchesVarThresholded is a 1 channel matrix of doubles
//resultantPatches is a 3-channel matrix of uchars
//returns the training samples in the biggest blob (a matrix of doubles)
Mat filterBiggestBlob(Mat patchesVarThresholded, int *ntr)
{
	Mat patchesBiggestBlob = Mat::zeros(height/size_patch, width/size_patch, CV_64FC1);
	//patchesBiggestBlob.create(height/size_patch, width/size_patch, CV_64FC1);
	vector<blob> blobs;
	vector<blob>::iterator it;
	findBlobs(patchesVarThresholded, blobs);
	vector<blob>::iterator end = blobs.end();
	int indexMax = -1;
	int countMax = -1;
	int i;
	for (it = blobs.begin(), i = 0; it != end; i++, ++it)
	{
		if ((*it).count > countMax)
		{
			countMax = (*it).count;
			indexMax = i;
		} 
	} 
	int size = blobs[indexMax].patchCoords.size();
	*ntr = size;
	for (int i = 0; i < size; i++)
			patchesBiggestBlob.at<double>(blobs[indexMax].patchCoords[i].y,blobs[indexMax].patchCoords[i].x) = 255.0;
	return patchesBiggestBlob;
}

