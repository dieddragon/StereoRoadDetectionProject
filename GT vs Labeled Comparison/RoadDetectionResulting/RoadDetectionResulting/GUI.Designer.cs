namespace RoadDetectionResulting
{
    partial class GUI
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.pbGT = new System.Windows.Forms.PictureBox();
            this.pbResult = new System.Windows.Forms.PictureBox();
            this.btnStart = new System.Windows.Forms.Button();
            this.rtbResults = new System.Windows.Forms.RichTextBox();
            this.openFileDialog1 = new System.Windows.Forms.OpenFileDialog();
            ((System.ComponentModel.ISupportInitialize)(this.pbGT)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.pbResult)).BeginInit();
            this.SuspendLayout();
            // 
            // pbGT
            // 
            this.pbGT.Location = new System.Drawing.Point(12, 12);
            this.pbGT.Name = "pbGT";
            this.pbGT.Size = new System.Drawing.Size(320, 240);
            this.pbGT.TabIndex = 0;
            this.pbGT.TabStop = false;
            // 
            // pbResult
            // 
            this.pbResult.Location = new System.Drawing.Point(12, 258);
            this.pbResult.Name = "pbResult";
            this.pbResult.Size = new System.Drawing.Size(320, 240);
            this.pbResult.TabIndex = 1;
            this.pbResult.TabStop = false;
            // 
            // btnStart
            // 
            this.btnStart.Location = new System.Drawing.Point(338, 475);
            this.btnStart.Name = "btnStart";
            this.btnStart.Size = new System.Drawing.Size(75, 23);
            this.btnStart.TabIndex = 2;
            this.btnStart.Text = "Start";
            this.btnStart.UseVisualStyleBackColor = true;
            this.btnStart.Click += new System.EventHandler(this.btnStart_Click);
            // 
            // rtbResults
            // 
            this.rtbResults.Location = new System.Drawing.Point(338, 12);
            this.rtbResults.Name = "rtbResults";
            this.rtbResults.Size = new System.Drawing.Size(409, 457);
            this.rtbResults.TabIndex = 3;
            this.rtbResults.Text = "";
            // 
            // openFileDialog1
            // 
            this.openFileDialog1.Filter = "\"txt files (*.txt)|*.txt|All files (*.*)|*.*\"";
            // 
            // GUI
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(852, 514);
            this.Controls.Add(this.rtbResults);
            this.Controls.Add(this.btnStart);
            this.Controls.Add(this.pbResult);
            this.Controls.Add(this.pbGT);
            this.Name = "GUI";
            this.Text = "Road Detection Result Comparison Program";
            ((System.ComponentModel.ISupportInitialize)(this.pbGT)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.pbResult)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.PictureBox pbGT;
        private System.Windows.Forms.PictureBox pbResult;
        private System.Windows.Forms.Button btnStart;
        private System.Windows.Forms.RichTextBox rtbResults;
        private System.Windows.Forms.OpenFileDialog openFileDialog1;
    }
}

