#include "main.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////// FILE I/O ////////////////////////////////
//////////////////////////////////////////////////////////////////////

/*
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

*/

//Checks if the file given with the filename is exists
bool checkFileExists(string filename)
{
	ifstream ifile(filename.c_str());
	return ifile.is_open();
}

int readPtsDataFromFile(string fileName, float4* data)
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

					float4 dummy = {rVal, gVal, bVal, yVal};
					data[rowVal * WIDTH + colVal] = dummy;
				//xyzImg.at<Vec3d>(rowVal,colVal)[0] = xVal;
				//xyzImg.at<Vec3d>(rowVal,colVal)[1] = yVal;
				//xyzImg.at<Vec3d>(rowVal,colVal)[2] = zVal;
			}
		}
		infile.close();
	}
	else
		std::cout << "Not a single file is opened that day!" << std::endl;

	return innerCounter / 3;	
}


int readPPM(string filename, float4* data)
{
		//read image file
		std::string sWidth, sHeight, sMax;
		int width, height, max;
		std::ifstream fileImg;
 		bool isTagEnded = false;
		//unsigned char* data;
		int counter = 0;

		fileImg.open(filename.c_str(), std::ios::binary);

		if (fileImg.is_open())
		{
			while ( fileImg.good() && !isTagEnded)
			{
				if (counter > 2)
				{			
					if (counter == 3)
					{
						std::getline(fileImg, sWidth, ' ');
						std::istringstream issElement(sWidth);
						issElement >> width;
					}
					else if (counter == 3 + sWidth.length())
					{
						std::getline(fileImg, sHeight, ' ');
						std::istringstream issElement(sHeight);
						issElement >> height;
					}
					else if (counter == 3 + sWidth.length() + sHeight.length())
					{
						std::getline(fileImg, sMax, ' ');
						std::istringstream issElement(sMax);
						issElement >> max;
						isTagEnded = true;
					}
				}
				else
					fileImg.get();
				counter++;
			}

			counter = 0;
			//data = new unsigned char[width*height*bpp];

			while ( fileImg.good() )
			{
				data[counter].x = (float)fileImg.get();
				data[counter].y = (float)fileImg.get();
				data[counter].z = (float)fileImg.get();
				counter++;
			}
			return counter;
		}
		else
			return 0;

		
}

int readData(string fileNamePts, string fileNamePpm, float4* data)
{
	//Read the pts file and store x,y,z in a matrix
	cout << fileNamePts << " is loading..." << endl;
	cout << fileNamePpm << " is loading..." << endl;
	int nPts = readPtsDataFromFile(fileNamePts, data);
	//int id1 = width * 7 + 10;
	//int id2 = 0;
	//cout << data[id1].x << "," << data[id1].y << "," << data[id1].z << "," << data[id1].w << endl;
	//cout << data[id2].x << "," << data[id2].y << "," << data[id2].z << "," << data[id2].w << endl;
	readPPM(fileNamePpm, data);
	//cout << data[id1].x << "," << data[id1].y << "," << data[id1].z << "," << data[id1].w << endl;
	//cout << data[id2].x << "," << data[id2].y << "," << data[id2].z << "," << data[id2].w << endl;
	return nPts;
}

void writePPMGrayuchar(string filename, unsigned char* data, unsigned int width, unsigned int height, float scale)
{
	std::string sWidth, sHeight;
	std::ofstream fileImg;
	fileImg.open(filename.c_str(), std::ios::binary);

	std::ostringstream osstreamW, osstreamH;
	osstreamW << width;
	osstreamH << height;
	sWidth = osstreamW.str();
	sHeight = osstreamH.str();
	
	fileImg.write("P6\n", 3);
	fileImg.write(sWidth.c_str(), sWidth.length());
	fileImg.write(" ", 1);
	fileImg.write(sHeight.c_str(), sHeight.length());
	fileImg.write(" 255\n", 5);

	for (int i = 0; i < (int)(width * height); i++)
		for (int j = 0; j < 3; j++)
		{
			unsigned char asd = data[i]*scale;
			fileImg.write((char*)&asd, 1);
		}
	
	fileImg.close();
}

void writePPMGrayuint(string filename, unsigned int* data, unsigned int width, unsigned int height, float scale)
{
	std::string sWidth, sHeight;
	std::ofstream fileImg;
	fileImg.open(filename.c_str(), std::ios::binary);

	std::ostringstream osstreamW, osstreamH;
	osstreamW << width;
	osstreamH << height;
	sWidth = osstreamW.str();
	sHeight = osstreamH.str();
	
	fileImg.write("P6\n", 3);
	fileImg.write(sWidth.c_str(), sWidth.length());
	fileImg.write(" ", 1);
	fileImg.write(sHeight.c_str(), sHeight.length());
	fileImg.write(" 255\n", 5);

	for (int i = 0; i < (int)(width * height); i++)
		for (int j = 0; j < 3; j++)
		{
			unsigned char asd = data[i]*scale;
			fileImg.write((char*)&asd, 1);
		}
	
	fileImg.close();
}


void writePPMGrayuchar_upsized(string filename, unsigned char* data, unsigned int width, unsigned int height, unsigned int sizepatch, float scale)
{
	std::string sWidth, sHeight;
	std::ofstream fileImg;
	fileImg.open(filename.c_str(), std::ios::binary);

	std::ostringstream osstreamW, osstreamH;
	osstreamW << width * sizepatch;
	osstreamH << height * sizepatch;
	sWidth = osstreamW.str();
	sHeight = osstreamH.str();
	
	fileImg.write("P6\n", 3);
	fileImg.write(sWidth.c_str(), sWidth.length());
	fileImg.write(" ", 1);
	fileImg.write(sHeight.c_str(), sHeight.length());
	fileImg.write(" 255\n", 5);

	for (int i = 0; i < (int)(width * height); i++)
		for (int j = 0; j < (int)(3 * sizepatch * sizepatch); j++)
		{
			unsigned char asd = data[i]*scale;
			fileImg.write((char*)&asd, 1);
		}
	
	fileImg.close();
}

void writePPMGrayuint_upsized(string filename, unsigned int* data, unsigned int width, unsigned int height, unsigned int sizepatch, float scale)
{
	std::string sWidth, sHeight;
	std::ofstream fileImg;
	fileImg.open(filename.c_str(), std::ios::binary);

	std::ostringstream osstreamW, osstreamH;
	osstreamW << width * sizepatch;
	osstreamH << height * sizepatch;
	sWidth = osstreamW.str();
	sHeight = osstreamH.str();
	
	fileImg.write("P6\n", 3);
	fileImg.write(sWidth.c_str(), sWidth.length());
	fileImg.write(" ", 1);
	fileImg.write(sHeight.c_str(), sHeight.length());
	fileImg.write(" 255\n", 5);

	for (int i = 0; i < (int)(width * height); i++)
		for (int j = 0; j < (int)(3 * sizepatch * sizepatch); j++)
		{
			unsigned char asd = data[i]*scale;
			fileImg.write((char*)&asd, 1);
		}
	
	fileImg.close();
}


void writePPMuint(string filename, float4* data, unsigned int width, unsigned int height, float scale)
{
	std::string sWidth, sHeight;
	std::ofstream fileImg;
	fileImg.open(filename.c_str(), std::ios::binary);

	std::ostringstream osstreamW, osstreamH;
	osstreamW << width;
	osstreamH << height;
	sWidth = osstreamW.str();
	sHeight = osstreamH.str();
	
	fileImg.write("P6\n", 3);
	fileImg.write(sWidth.c_str(), sWidth.length());
	fileImg.write(" ", 1);
	fileImg.write(sHeight.c_str(), sHeight.length());
	fileImg.write(" 255\n", 5);

	for (int i = 0; i < (int)(width * height); i++)
	{
		unsigned char asdr = data[i].x*scale;
		unsigned char asdg = data[i].y*scale;
		unsigned char asdb = data[i].z*scale;
		fileImg.write((char*)&asdr, 1);
		fileImg.write((char*)&asdg, 1);
		fileImg.write((char*)&asdb, 1);
	}

	
	fileImg.close();
}
