#ifndef ROADDETECTION_IOMETHODS_H__
#define ROADDETECTION_IOMETHODS_H__

//////////////////////////////////////////////////////////////////////
//////////////////////////// FILE I/O ////////////////////////////////
//////////////////////////////////////////////////////////////////////

//Writes the classification result to the text file as "row col 1/0" per row of the file
void writeResultDataToFile(string nameResultsFile, Mat classifiedMat);

//Reads the pts file with values (x y z r g b row col) are delimited by space (" ") character and store x,y,z into 
//opencv matrix's related columns and rows as written in the pts file.
//The matrix has 3 channels of doubles.
int readPtsDataFromFile(string fileName, Mat xyzImg);

//////////////////////////////////////////////////////////////////////
///////////////////////// CONSOLE WRITING ////////////////////////////
//////////////////////////////////////////////////////////////////////

//write all the elements of a 3 channel matrix of doubles to the console
void writeElementsOfMatrix3Chd(Mat matrix, string nameMatrix);

//write all the elements of a 3 channel matrix of uchars to the console
void writeElementsOfMatrix3Chb(Mat matrix, string nameMatrix);

//write all the elements of a 1 channel matrix of doubles to the console
void writeElementsOfMatrix1Chd(Mat matrix, string nameMatrix);

#endif ROADDETECTION_IOMETHODS_H__