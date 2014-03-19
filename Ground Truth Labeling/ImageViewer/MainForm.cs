// Image Viewer sample application
// AForge.NET framework
// http://www.aforgenet.com/framework/
//
// Copyright © AForge.NET, 2006-2011
// contacts@aforgenet.com
//

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.IO;

using AForge.Imaging;
using AForge.Imaging.Formats;
using AForge.Imaging.Filters;

namespace ImageViewer
{
    // Main form's class
    public partial class MainForm : Form
    {
        string fileName, filePath;
        // Class constructor
        public MainForm( )
        {
            InitializeComponent( );
        }

        // Exit from application
        private void exitToolStripMenuItem_Click( object sender, EventArgs e )
        {
            Application.Exit( );
        }

        // Open image file
        private void openToolStripMenuItem_Click( object sender, EventArgs e )
        {
            openFileDialog.Filter = "PPM File | *.ppm";
            if ( openFileDialog.ShowDialog( ) == DialogResult.OK )
            {
                try
                {
                    ImageInfo imageInfo = null;
                    // create grayscale filter (BT709)
                    Grayscale filter = new Grayscale(0.2125, 0.7154, 0.0721);
                    // apply the filter
                    Bitmap grayImage = filter.Apply(ImageDecoder.DecodeFromFile( openFileDialog.FileName, out imageInfo ));
                    fileName = openFileDialog.FileName;
                    pictureBox.Image = grayImage;

                    
                    propertyGrid.SelectedObject = imageInfo;
                    //mageInfo.BitsPerPixel
                    propertyGrid.ExpandAllGridItems( );
                }
                catch ( NotSupportedException ex )
                {
                    MessageBox.Show( "Image format is not supported: " + ex.Message, "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error );
                }
                catch ( ArgumentException ex )
                {
                    MessageBox.Show( "Invalid image: " + ex.Message, "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error );
                }
                catch
                {
                    MessageBox.Show( "Failed loading the image", "Error",
                        MessageBoxButtons.OK, MessageBoxIcon.Error );
                }
            }
        }


        int drawPoly = 0;
        List<AForge.IntPoint> polyPoints;
        string resultTxt;
        List<string> resultList;
        private void button1_Click(object sender, EventArgs e)
        {
            if (drawPoly == 0)
            {
                ImageInfo imageInfo = null;
                polyPoints = new List<AForge.IntPoint>();
                drawPoly = 1;
                button1.Text = "End Polygon";
                

                Grayscale filter = new Grayscale(0.2125, 0.7154, 0.0721);
                // apply the filter
                Bitmap grayImage = filter.Apply(ImageDecoder.DecodeFromFile(fileName, out imageInfo));
                pictureBox.Image = grayImage;
            }
            else
            {
                drawPoly = 0;
                button1.Text = "Start Polygon";
                // sample 2 - converting .NET image into unmanaged
                Bitmap grayImage = (Bitmap)pictureBox.Image;
                UnmanagedImage unmanagedImage = UnmanagedImage.FromManagedImage(grayImage);

                Drawing.Polygon(unmanagedImage, polyPoints, Color.White);
                resultTxt = "";
                resultList = new List<string>();
                //pictureBox.Image = null;
                
                for (int y = 0; y < grayImage.Height; y++)
                {
                    for (int x = 0; x < grayImage.Width; x++)
                    {
                        if (IsPointInPolygon(x, y, polyPoints))
                        {
                            unmanagedImage.SetPixel(x, y, Color.White);
                            resultList.Add(y.ToString() + " " + x.ToString() + " " + "1");//resultTxt += y.ToString() + " " + x.ToString() + " " + "1" + System.Environment.NewLine;
                        }
                        else
                        {
                            //unmanagedImage.SetPixel(x, y, Color.Black);
                            resultList.Add(y.ToString() + " " + x.ToString() + " " + "0");
                            //resultTxt += y.ToString() + " " + x.ToString() + " " + "0" + System.Environment.NewLine;
                        }
                    }
                }

                Bitmap managedImage = unmanagedImage.ToManagedImage();
                pictureBox.Image = managedImage;
            }
        }

        private void pictureBox_MouseClick(object sender, MouseEventArgs e)
        {
            if (drawPoly == 1)
            {
                polyPoints.Add(new AForge.IntPoint(e.X, e.Y));
                listBox1.Items.Add((e.X).ToString() + ", " + (e.Y).ToString());
            }
        }

        private void pictureBox_MouseMove(object sender, MouseEventArgs e)
        {
            label1.Text = "X: " + e.X + ", Y: " + e.Y;
        }



        private bool IsPointInPolygon(int x, int y, List<AForge.IntPoint> polygon)
        {

            PointF point = new PointF((float)x, (float)y);
            bool isInside = false;

            for (int i = 0, j = polygon.Count - 1; i < polygon.Count; j = i++)
            {

                if (((polygon[i].Y > point.Y) != (polygon[j].Y > point.Y)) &&

                (point.X < (polygon[j].X - polygon[i].X) * (point.Y - polygon[i].Y) / (polygon[j].Y - polygon[i].Y) + polygon[i].X))
                {

                    isInside = !isInside;

                }

            }

            return isInside;

        }

        private void button2_Click(object sender, EventArgs e)
        {
            SaveFileDialog save = new SaveFileDialog();

            save.FileName = fileName + ".txt";

            save.Filter = "Text File | *.txt";

            if (save.ShowDialog() == DialogResult.OK)
            {

                StreamWriter writer = new StreamWriter(save.OpenFile());

                for (int i=0; i<resultList.Count; i++)
                    writer.WriteLine(resultList[i]);

                writer.Dispose();

                writer.Close();

            }
        }

        private void saveToolStripMenuItem_Click(object sender, EventArgs e)
        {
            button2.PerformClick();
        }

        private void polygonToolStripMenuItem_Click(object sender, EventArgs e)
        {
            button1.PerformClick();
        }

    }
}
