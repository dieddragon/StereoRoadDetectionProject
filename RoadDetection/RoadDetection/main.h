#ifndef ROADDETECTION_MAIN_H__
#define ROADDETECTION_MAIN_H__

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <math.h>
#include <windows.h>
#include <string.h>
#include <vector>

#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/ml/ml.hpp>
#include <opencv2/flann/flann.hpp>

using namespace cv;
using namespace std;

#include "iomethods.h"
#include "algorithmParams.h"
#include "procMethods.h"
#include "modelMethods.h"
#include "classificationMethods.h"
#include "algorithm.h"

#define INF 10000000000;

extern bool debug;

#endif ROADDETECTION_MAIN_H__