#ifndef ROADDETECTIONGPU_IOMETHODS_H__
#define ROADDETECTIONGPU_IOMETHODS_H__

//////////////////////////////////////////////////////////////////////
//////////////////////////// FILE I/O ////////////////////////////////
//////////////////////////////////////////////////////////////////////

//Writes the classification result to the text file as "row col 1/0" per row of the file
//void writeResultDataToFile(string nameResultsFile, Mat classifiedMat);

//Checks if the file given with the filename is exists
bool checkFileExists(string filename);

//Reads the pts file with values (x y z r g b row col) are delimited by space (" ") character and store r,g,b,y into 
//cuda's float4 structure and insert it to the related columns and rows as written in the pts file.
int readPtsDataFromFile(string fileName, float4* data);

int readPPM(string filename, float4* data);

int readData(string fileNamePts, string fileNamePpm, float4* data);

void writePPMGrayuchar(string filename, unsigned char* data, unsigned int width, unsigned int height, float scale);

void writePPMGrayuint(string filename, unsigned int* data, unsigned int width, unsigned int height, float scale);

void writePPMGrayuchar_upsized(string filename, unsigned char* data, unsigned int width, unsigned int height, unsigned int sizepatch, float scale);

void writePPMGrayuint_upsized(string filename, unsigned int* data, unsigned int width, unsigned int height, unsigned int sizepatch, float scale);

void writePPMuint(string filename, float4* data, unsigned int width, unsigned int height, float scale);

#endif ROADDETECTIONGPU_IOMETHODS_H__