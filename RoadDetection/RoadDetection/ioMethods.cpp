#include "main.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////// FILE I/O ////////////////////////////////
//////////////////////////////////////////////////////////////////////

void writeResultDataToFile(string nameResultsFile, Mat classifiedMat)
{
	//string asd = "results\\asd.txt";
	std::ofstream outfile(nameResultsFile.c_str());
	std::string line;
	//outfile << "aad";
	const char delimiter = ' ';
	//outfile.open();

	if (outfile.is_open())
	{
		for (int i = 0; i < classifiedMat.rows; i++)
			for (int j = 0; j < classifiedMat.cols; j++)
			{
				string indRowStr = static_cast<ostringstream*>( &(ostringstream() << i) )->str();
				string indColStr = static_cast<ostringstream*>( &(ostringstream() << j) )->str();
				int val = (classifiedMat.at<Vec<uchar,3>>(i,j)[1] == 255) ? 1 : 0;
				string valStr = static_cast<ostringstream*>( &(ostringstream() << val) )->str();
				line = indRowStr + delimiter + indColStr + delimiter + valStr;
				outfile << line << endl;
			}
	}
	outfile.close();
}


int readPtsDataFromFile(string fileName, Mat xyzImg)
{
	std::ifstream infile;
	std::string line;
	std::string element;
	int generalCounter = 0;
	int innerCounter = 0;
	const char delimiter = ' ';

	double xVal, yVal, zVal;
	int rVal, gVal, bVal, rowVal, colVal;

	infile.open(fileName.c_str(), std::ifstream::in);

	if (infile.is_open())
	{
		while ( infile.good() )
		{
			if (std::getline(infile,line))
			{
				std::istringstream issLine(line);
				while (issLine.good())
					if (std::getline(issLine, element, delimiter))
					{
						std::istringstream issElement(element);
						switch(generalCounter % 8)
						{
							case 0:
								issElement >> xVal;
								break;
							case 1:
								issElement >> yVal;
								break;
							case 2:
								issElement >> zVal;
								break;
							case 3:
								issElement >> rVal;
								break;
							case 4:
								issElement >> gVal;
								break;
							case 5:
								issElement >> bVal;
								break;
							case 6:
								issElement >> rowVal;
								break;
							case 7:
								issElement >> colVal;
								break;
						}
						generalCounter++;
					}

				xyzImg.at<Vec3d>(rowVal,colVal)[0] = xVal;
				xyzImg.at<Vec3d>(rowVal,colVal)[1] = yVal;
				xyzImg.at<Vec3d>(rowVal,colVal)[2] = zVal;
			}
		}
		infile.close();
	}
	else
		std::cout << "Not a single file is opened that day!" << std::endl;

	return innerCounter / 3;	
}




//////////////////////////////////////////////////////////////////////
///////////////////////// CONSOLE WRITING ////////////////////////////
//////////////////////////////////////////////////////////////////////


//write all the elements of a 3 channel matrix of doubles to the console
void writeElementsOfMatrix3Chd(Mat matrix, string nameMatrix)
{
	int nRows = matrix.rows;
	int nCols = matrix.cols;
	int nChan = matrix.channels();
	cout << "The elements of the matrix " << nameMatrix << " is :" << endl;
	for (int i = 0; i < nRows; i++)
		for (int j = 0; j < nCols; j++)
		{
			cout << "(" << i << "," << j << ") : ";
			for (int d = 0; d < nChan; d++)
			{
				cout << matrix.at<Vec<double,3>>(i,j)[d] << " ";				
			}
			cout << endl;
		}
}

//write all the elements of a 3 channel matrix of uchars to the console
void writeElementsOfMatrix3Chb(Mat matrix, string nameMatrix)
{
	int nRows = matrix.rows;
	int nCols = matrix.cols;
	int nChan = matrix.channels();
	cout << "The elements of the matrix " << nameMatrix << " is :" << endl;
	for (int i = 0; i < nRows; i++)
		for (int j = 0; j < nCols; j++)
		{
			cout << "(" << i << "," << j << ") : ";
			for (int d = 0; d < nChan; d++)
			{
				cout << (int)matrix.at<Vec<uchar,3>>(i,j)[d] << " ";				
			}
			cout << endl;
		}
}

//write all the elements of a 1 channel matrix of doubles to the console
void writeElementsOfMatrix1Chd(Mat matrix, string nameMatrix)
{
	int nRows = matrix.rows;
	int nCols = matrix.cols;
	cout << "The elements of the matrix " << nameMatrix << " is :" << endl;
	for (int i = 0; i < nRows; i++)
		for (int j = 0; j < nCols; j++)
		{
			cout << "(" << i << "," << j << ") : " << matrix.at<double>(i,j) << endl;				
		}
}
