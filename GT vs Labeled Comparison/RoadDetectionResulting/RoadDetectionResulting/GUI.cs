using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;

namespace RoadDetectionResulting
{
    public partial class GUI : Form
    {
        String extFile = ".txt";
        String nameGTFile;
        String nameGTFileOld;

        const int N_DATA = 359;

        public GUI()
        {
            InitializeComponent();
        }

        private void btnStart_Click(object sender, EventArgs e)
        {
            DialogResult resultGT = System.Windows.Forms.DialogResult.Abort, resultRes = System.Windows.Forms.DialogResult.Abort;
            if (nameGTFile == null)
            {
                openFileDialog1.Title = "Select the first ground truth file";
                resultGT = openFileDialog1.ShowDialog();
                nameGTFile = openFileDialog1.FileName;
                nameGTFileOld = openFileDialog1.FileName;
            }
            nameGTFile = nameGTFileOld;
            openFileDialog1.Title = "Select the first result file";
            resultRes = openFileDialog1.ShowDialog();
            String nameResultFile = openFileDialog1.FileName;
            
            if (resultRes == System.Windows.Forms.DialogResult.OK)
            {
                rtbResults.Text += nameResultFile + "\n";

                for (int i = 0; i < N_DATA; i++)
                {
                    String[] dummyGT = nameGTFile.Split('(');
                    String[] dummyResult = nameResultFile.Split('(');

                    nameGTFile = dummyGT[0] + "(" + (i + 1).ToString() + ")" + extFile;
                    nameResultFile = dummyResult[0] + "(" + (i + 1).ToString() + ")" + extFile;

                    // Read the file line by line.
                    System.IO.StreamReader fileGT = new System.IO.StreamReader(nameGTFile);
                    System.IO.StreamReader fileResult = new System.IO.StreamReader(nameResultFile);

                    String lineGT, lineResult;
                    char separator = ' ';
                    int truePos = 0, trueNeg = 0, falsePos = 0, falseNeg = 0;
                    while ((lineGT = fileGT.ReadLine()) != null && (lineResult = fileResult.ReadLine()) != null)
                    {
                        String[] wordsGT = lineGT.Split(separator);
                        String[] wordsResult = lineResult.Split(separator);

                        int classGT, classResult;
                        int rowGT, rowResult, colGT, colResult;

                        int.TryParse(wordsGT[0], out rowGT);
                        int.TryParse(wordsResult[0], out rowResult);

                        int.TryParse(wordsGT[1], out colGT);
                        int.TryParse(wordsResult[1], out colResult);

                        int.TryParse(wordsGT[2], out classGT);
                        int.TryParse(wordsResult[2], out classResult);

                        //akifin yedigi boku duzeltmece
                        if (colGT == 640)
                        {
                            lineGT = fileGT.ReadLine();
                            wordsGT = lineGT.Split(separator);

                            int.TryParse(wordsGT[0], out rowGT);
                            int.TryParse(wordsGT[1], out colGT);
                            int.TryParse(wordsGT[2], out classGT);
                        }

                        if (classGT == 1)
                        {
                            if (classResult == 1)
                                truePos++;
                            else
                                falseNeg++;
                        }
                        else
                        {
                            if (classResult == 1)
                                falsePos++;
                            else
                                trueNeg++;
                        }
                    }
                    fileGT.Close();
                    fileResult.Close();

                    //The order of writing is: TP, TN, FP, FN
                    rtbResults.Text += truePos.ToString() + " ";
                    rtbResults.Text += trueNeg.ToString() + " ";
                    rtbResults.Text += falsePos.ToString() + " ";
                    rtbResults.Text += falseNeg.ToString() + "\n";
                }
            }
        }
    }
}
