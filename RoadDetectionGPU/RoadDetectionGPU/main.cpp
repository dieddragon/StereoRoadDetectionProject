#include "main.h"

//Data files
//Index of the file
int countFile = 0;
//File extensions
const string fileExtImg = ".ppm";
const string fileExtPts = ".pts";
//Folders
//const string folderRoot = "C:\\Users\\KBO\\Dropbox\\Academy\\Projects\\Road Detection, Extraction and Tracking\\Stereo Vision\\Stereo Road Detection\\Software\\RoadDetectionv3 - OpenCV - i7\\RoadDetection\\";
//const string folderRoot = "C:\\RoadDetectionv5 - OpenCV - Efficient - lenovo - Copy\\RoadDetection\\";
const string folderRoot = "C:\\Users\\kbo\\Desktop\\";
const string folderPts = "data\\pts\\";
const string folderImg = "data\\color\\";
//File names
string nameImg = folderRoot + folderImg + "(1).ppm";
string namePtsFile = folderRoot + folderPts + "(1).pts";


//Results
//Change algorithm params to string
string nBinsStr = static_cast<ostringstream*>( &(ostringstream() << N_BINS) )->str();
string sizePatchStr = static_cast<ostringstream*>( &(ostringstream() << SIZE_PATCH) )->str();
//string nSamplesMinStr = static_cast<ostringstream*>( &(ostringstream() << N_SAMPLES_MIN) )->str();
string nClustersMaxStr = static_cast<ostringstream*>( &(ostringstream() << N_CLUSTERS_MAX) )->str();
string nClustersInitialStr = static_cast<ostringstream*>( &(ostringstream() << N_CLUSTERS_INITIAL) )->str();
string nClustersNewStr = static_cast<ostringstream*>( &(ostringstream() << N_CLUSTERS_NEW) )->str();
string thresholdVarYStr = static_cast<ostringstream*>( &(ostringstream() << THRESHOLD_VAR_Y) )->str();
string thresholdUpdateStr = static_cast<ostringstream*>( &(ostringstream() << THRESHOLD_UPDATE_MAH) )->str();
string thresholdClassifyStr = static_cast<ostringstream*>( &(ostringstream() << THRESHOLD_CLASSIFY_MAH) )->str();
//Result folder
string folderWrite = "results\\";
//Result File name prefix
string prefixResultsFile = "hs_" + nBinsStr + "bins_" + sizePatchStr + "pchs_" + nClustersMaxStr + "_" + nClustersInitialStr + "_" + nClustersNewStr + "_clst_max_init_new_" + thresholdVarYStr + "tVarY_" + thresholdUpdateStr + "tUp_" + thresholdClassifyStr + "tClsfy_12blc256thr";
//string prefixResultsFile = "hs_" + nBinsStr + "bins_" + sizePatchStr + "pchs_" + nSamplesMinStr + "nsmpsmin_" + nClustersMaxStr + "_clst_max_" + thresholdUpdateStr + "thrUpd_" + thresholdClassifyStr + "thrClsfy_";
//Result file extension
const string fileExtResult = ".txt";
//Result file names
string nameResultsFile = folderWrite + prefixResultsFile + "(1).txt";
string nameResultsImg = folderWrite + prefixResultsFile + "(1).ppm";
string nameTimeFile = folderWrite + "GTX650_Times_in_ms_new.txt";

void insertRandIndicesHost(unsigned int *iRand, unsigned int *ntr)
{
	for (int i = 0; i < N_CLUSTERS_INITIAL; i++)
	{
		if (i == 0)
		{
			int r = rand() % ntr[0];
			iRand[i] = r;
		}
		else
		{
			bool isRandNotFound = true;
			while (isRandNotFound)
			{
				isRandNotFound = false;
				int r = rand() % ntr[0];
				for (int j = 0; j < i; j++)
				{
					if (r == iRand[j])
					{
						isRandNotFound = true;
						break;
					}
				}
				iRand[i] = r;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////
// declaration, forward
extern "C" void initialize(const int argc, const char **argv);

extern "C" float detectRoad(const int argc, const char **argv, float4 *data, unsigned int *n_models, bool isFirstFrame, unsigned int *classified);

extern "C" void cleanup();

int main(int argc, char **argv)
{
	int n_pix = WIDTH * HEIGHT;
	int n_hists = n_pix / SIZE_PATCH / SIZE_PATCH * N_BINS * N_BINS;
	int n_patches = n_pix / SIZE_PATCH / SIZE_PATCH;
	unsigned int *n_models = new unsigned int[1];
	*n_models = 0;

	//// HOST ARRAYS ////

	//raw data r,g,b and y coordinate
	float4 *data = new float4[n_pix];
	unsigned int *hists = new unsigned int[n_hists];
	float *vars = new float[n_patches];
	unsigned int *ghist = new unsigned int[256 * 3];
	unsigned int *cdf = new unsigned int[257 * 3];
	unsigned int *tr = new unsigned int[n_hists];
	uint4 *min = new uint4[1];
	uint2 *loc = new uint2[n_patches];
	unsigned int *trlabels = new unsigned int[n_patches];
	unsigned int *trsize = new unsigned int[n_patches];
	unsigned int *max = new unsigned int[1];
	unsigned int *maxgid = new unsigned int[1];
	unsigned int *trcompact = new unsigned int[n_patches];
	unsigned int *classified = new unsigned int[n_patches];
	unsigned int *roadlabel = new unsigned int[1];
	unsigned int *ntr = new unsigned int[1];

	float *probs = new float[N_CLUSTERS_MAX * n_patches];
	float *n = new float[N_CLUSTERS_MAX];
	float *means = new float[N_CLUSTERS_MAX * N_BINS_SQR];
	float *invCovs = new float[N_CLUSTERS_MAX * N_BINS_SQR];
	float *logconsts = new float[N_CLUSTERS_MAX];
	unsigned int *iRand = new unsigned int[N_CLUSTERS_MAX];
	float *likelihood = new float[n_patches / 512 + 1];
	
	//initialization
	cout << "Initializing..." << endl;
	for (int i = 0; i < n_pix; i++)
		data[i] = make_float4(0,0,0,0);
	for (int i = 0; i < n_patches; i++)
		classified[i] = 0;	

	std::ofstream fileTime;
	if (checkFileExists(nameTimeFile))
		fileTime.open(nameTimeFile.c_str(), std::fstream::app);
	else
		fileTime.open(nameTimeFile.c_str());
	fileTime << nameResultsImg << endl;
	
	
	initialize(argc, (const char **)argv);

	//for (int i = 0; i < 1; i++)
	for (int i = 0; i < 359; i++)
	{
		//increase the file counter by 1
		countFile++;
		//construct the next file names
		string countFileStr = static_cast<ostringstream*>( &(ostringstream() << countFile) )->str();
		nameImg = folderRoot + folderImg + "(" + countFileStr + ")" + fileExtImg;
		namePtsFile = folderRoot + folderPts + "(" + countFileStr + ")" + fileExtPts;
		nameResultsFile = folderWrite + prefixResultsFile + "(" + countFileStr + ")" + fileExtResult;
		nameResultsImg = folderWrite + prefixResultsFile + "(" + countFileStr + ")" + fileExtImg;

		for (int j = 0; j < n_pix; j++)
			data[j] = make_float4(0,0,0,0);

		readData(namePtsFile, nameImg, data);

		float time;
		if (i == 0)
			time = detectRoad(argc, (const char **)argv, data, n_models, true, classified);
		else
			time = detectRoad(argc, (const char **)argv, data, n_models, false, classified);

		float maxClassified = 0, minClassified = INF;
		for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
			maxClassified = (classified[i] > maxClassified) ? classified[i] : maxClassified;
		for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
			minClassified = (classified[i] < minClassified) ? classified[i] : minClassified;
	
		writePPMGrayuint(nameResultsImg, classified, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxClassified);
		
		string strTime = static_cast<ostringstream*>( &(ostringstream() << time) )->str();
		fileTime << strTime << endl;

		cout << "Time passed in ms: " << time << endl;
	}

	cleanup();

	fileTime.close();

	cudaDeviceReset();
}