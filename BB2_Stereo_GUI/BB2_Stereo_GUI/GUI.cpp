#include "GUI.h"

int counter = 0;
SYSTEMTIME stime;


GUIWindow::GUIWindow() 
: m_timeout(100),
m_startButton("Start!"),
m_startSaveButton("Start Save Data!"),
m_setStereoParamsButton("Set Stereo Params"),
m_mainButtonTable(1,2),
m_rightButtonTable(14,1),
m_rightLabel("Stereo Parameters")
{
	//window properties
	Gtk::Window::set_title("BB2 GUI"); //set the title of the window
	Gtk::Window::set_border_width(10); //set the border width of the window
	Gtk::Window::set_resizable(false);
	//Gtk::Window::set_size_request(300,300);	//set the default size of the window

	//button properties
	m_setStereoParamsButton.set_size_request(100,60);

	//toggle button properties
	m_startButton.set_size_request(100,70);	//set the default size of the button
	m_startSaveButton.set_size_request(100,70);

	//check button properties
	m_textureValidationCheck.set_label("Texture Validation");
	m_surfaceValidationCheck.set_label("Surface Validation");
	m_backForthValidationCheck.set_label("Back and Forth Validation");
	m_backForthValidationCheck.set_active(true);
	m_subpixelInterpolationCheck.set_label("Subpixel Interpolation");
	m_subpixelInterpolationCheck.set_active(true);

	//entry properties
	m_minDispEntry.set_text("Min Disparity");
	m_minDispEntry.set_size_request(100,20);
	//m_minDispEntry.set_editable(false);
	m_maxDispEntry.set_text("Max Disparity");
	m_maxDispEntry.set_size_request(100,20);
	//m_maxDispEntry.set_editable(false);
	m_widthEntry.set_text("Image Width");
	m_widthEntry.set_size_request(100,20);
	m_heightEntry.set_text("Image Height");
	m_heightEntry.set_size_request(100,20);
	m_sizeStereoMaskEntry.set_text("Stereo Mask Size");
	m_sizeStereoMaskEntry.set_size_request(100,20);
	m_textureValidationThresholdEntry.set_text("Texture Validation Threshold");
	m_textureValidationThresholdEntry.set_size_request(100, 20);
	m_surfaceValidationSizeEntry.set_text("Surface Validation Size");
	m_surfaceValidationSizeEntry.set_size_request(100, 20);
	m_surfaceValidationDifferenceEntry.set_text("Surface Validation Difference");
	m_surfaceValidationDifferenceEntry.set_size_request(100, 20);

	//table properties
	m_mainButtonTable.set_col_spacings(10);
	m_mainButtonTable.attach(m_startButton, 0, 1, 0, 1);
	m_mainButtonTable.attach(m_startSaveButton, 1, 2, 0, 1);
	m_rightButtonTable.set_row_spacings(5);
	m_rightButtonTable.attach(m_rightLabel, 0, 1, 0, 1);
	m_rightButtonTable.attach(m_minDispEntry, 0, 1, 1, 2);
	m_rightButtonTable.attach(m_maxDispEntry, 0, 1, 2, 3);
	m_rightButtonTable.attach(m_widthEntry, 0, 1, 3, 4);
	m_rightButtonTable.attach(m_heightEntry, 0, 1, 4, 5);
	m_rightButtonTable.attach(m_sizeStereoMaskEntry, 0, 1, 5, 6);
	m_rightButtonTable.attach(m_textureValidationCheck, 0, 1, 6, 7);
	m_rightButtonTable.attach(m_textureValidationThresholdEntry, 0, 1, 7, 8);
	m_rightButtonTable.attach(m_surfaceValidationCheck, 0, 1, 8, 9);
	m_rightButtonTable.attach(m_surfaceValidationSizeEntry, 0, 1, 9, 10);
	m_rightButtonTable.attach(m_surfaceValidationDifferenceEntry, 0, 1, 10, 11);
	m_rightButtonTable.attach(m_backForthValidationCheck, 0, 1, 11, 12);
	m_rightButtonTable.attach(m_subpixelInterpolationCheck, 0, 1, 12, 13);
	m_rightButtonTable.attach(m_setStereoParamsButton, 0, 1, 13, 14);

	//drawing frame properties
	m_imageArea.set_size_request(3*BB2.getWidthImage(), BB2.getHeightImage());
	m_imageArea.add_events(Gdk::BUTTON_PRESS_MASK);

	//When the button receives the "clicked" signal, it will call the on_button_clicked() method defined below
	m_startButton.signal_clicked().connect(sigc::mem_fun(*this, &GUIWindow::on_button_start_clicked));
	m_startSaveButton.signal_clicked().connect(sigc::mem_fun(*this, &GUIWindow::on_button_startsave_clicked));
	m_setStereoParamsButton.signal_clicked().connect(sigc::mem_fun(*this, &GUIWindow::on_button_setstereoparams_clicked));
	m_imageArea.signal_button_press_event().connect(sigc::mem_fun(*this, &GUIWindow::on_mouse_clicked));

	//This packs the button into the Window (a container)
	Gtk::Window::add(m_hpaned);
	m_vpaned.add1(m_imageArea);
	m_vpaned.add2(m_mainButtonTable);
	m_hpaned.add1(m_vpaned);
	m_hpaned.add2(m_rightButtonTable);
	
	//The final step is to display all newly created widgets
	Gtk::Window::show_all_children();
}

GUIWindow::~GUIWindow() { }


bool GUIWindow::on_timeout()
{		
	BB2.allocBuffers();

	FlyCaptureImage	   fImage;
	TriclopsImage16  tDepthImage16;
	TriclopsImage       tMonoImage = {0}; 
	TriclopsColorImage  tColorImageRight;
	TriclopsColorImage  tColorImageLeft;
	
	//grabColorAndStereo(fImage, tDepthImage16, tMonoImage, tColorImageRight, tColorImageLeft);
	//BB2.grabColorAndStereo(fImage, tDepthImage16, tMonoImage, tColorImageRight, tColorImageLeft);
	BB2.grabColorAndStereo();
	tDepthImage16 = BB2.getDepthImage16();
	tColorImageRight = BB2.getColorImageRight();
	tColorImageLeft = BB2.getColorImageRight();

	unsigned char * dataExpandedRight;
	unsigned char * dataExpandedLeft;
	unsigned char * dataExpandedDisp;

	expandColor(tColorImageLeft, dataExpandedLeft);
	expandColor(tColorImageRight, dataExpandedRight);
	expandDepth(tDepthImage16, dataExpandedDisp);

	Glib::RefPtr<Gdk::Window> win = m_imageArea.get_window();
	Glib::RefPtr<Gdk::Pixbuf> imageDisp = Gdk::Pixbuf::create_from_data( dataExpandedDisp, Gdk::COLORSPACE_RGB, false, 8, tDepthImage16.ncols, tDepthImage16.nrows, tDepthImage16.rowinc/2*3);
	Glib::RefPtr<Gdk::Pixbuf> imageRight = Gdk::Pixbuf::create_from_data(dataExpandedRight, Gdk::COLORSPACE_RGB, false, 8, tColorImageRight.ncols, tColorImageRight.nrows, tColorImageRight.rowinc*3);
	Glib::RefPtr<Gdk::Pixbuf> imageLeft = Gdk::Pixbuf::create_from_data(dataExpandedLeft, Gdk::COLORSPACE_RGB, false, 8, tColorImageLeft.ncols, tColorImageLeft.nrows, tColorImageLeft.rowinc*3);

	imageLeft->render_to_drawable(win, m_imageArea.get_style()->get_black_gc(), 0, 0, 0, 0, imageLeft->get_width(), imageLeft->get_height(), Gdk::RGB_DITHER_NONE, 0, 0);
	imageRight->render_to_drawable(win, m_imageArea.get_style()->get_black_gc(), 0, 0, BB2.getWidthImage(), 0, imageRight->get_width(), imageRight->get_height(), Gdk::RGB_DITHER_NONE, 0, 0);
	imageDisp->render_to_drawable(win, m_imageArea.get_style()->get_black_gc(), 0, 0, 2*BB2.getWidthImage(), 0, imageDisp->get_width(), imageDisp->get_height(), Gdk::RGB_DITHER_NONE, 0, 0);

	if(m_startSaveButton.get_active())
	{
		counter++;
		GetLocalTime(&stime);
		char buf[40] = {0};
			
		sprintf(buf,"%02d_%02d_%04d__%02d_%02d__%04d", stime.wDay, stime.wMonth, stime.wYear, stime.wHour, stime.wMinute, counter);
		std::string dt = buf;
		BB2.saveData(dt);
	}

	BB2.releaseBuffers();

	free(dataExpandedLeft);
	free(dataExpandedRight);
	free(dataExpandedDisp);

    return true;
}

void GUIWindow::on_button_start_clicked()
{
	if (m_startButton.get_active())
	{
		std::cout << "BB2 is initializing!" << std::endl;
		m_startButton.set_label("Stop");
		m_timer = Glib::signal_timeout().connect(sigc::mem_fun(*this, &GUIWindow::on_timeout), m_timeout);
		BB2.initCamera();
		//initCamera();
	}
	else
	{
		std::cout << "BB2 is stopped!" << std::endl;
		m_startButton.set_label("Start");
		m_timer.disconnect();
		BB2.destroyCamera();
		//destroyCamera();
	}
}

void GUIWindow::on_button_startsave_clicked()
{
	if (m_startButton.get_active())
	{
		if (m_startSaveButton.get_active())
		{
			m_startSaveButton.set_label("Stop Save Data");
		}
		else
		{
			m_startSaveButton.set_label("Start Save Data");
		}
	}
	else
		m_startSaveButton.set_active(false);
}

void GUIWindow::on_button_setstereoparams_clicked()
{
	if(m_startButton.get_active())
	{
		int dummyInt;
		float dummyFloat;
		std::stringstream converter(m_minDispEntry.get_text());
		if(!(converter >> dummyInt))
			dummyInt = 0;
		BB2.setMinDisparity(dummyInt);
		converter.clear();
		converter.str(m_maxDispEntry.get_text());
		if(!(converter >> dummyInt))
			dummyInt = 200;
		BB2.setMaxDisparity(dummyInt);
		converter.clear();
		converter.str(m_widthEntry.get_text());
		if(!(converter >> dummyInt))
			dummyInt = 320;
		BB2.setWidthImage(dummyInt);
		converter.clear();
		converter.str(m_heightEntry.get_text());
		if(!(converter >> dummyInt))
			dummyInt = 240;
		BB2.setHeightImage(dummyInt);
		converter.clear();
		converter.str(m_sizeStereoMaskEntry.get_text());
		if(!(converter >> dummyInt))
			dummyInt = 15;
		BB2.setMaskSizeStereo(dummyInt);
		BB2.setIsTextureValidation(m_textureValidationCheck.get_active());
		converter.clear();
		converter.str(m_textureValidationThresholdEntry.get_text());
		if(!(converter >> dummyFloat))
			dummyFloat = 0.0;
		BB2.setTextureValidationThreshold(dummyFloat);
		BB2.setIsSurfaceValidation(m_surfaceValidationCheck.get_active());
		converter.clear();
		converter.str(m_surfaceValidationSizeEntry.get_text());
		if(!(converter >> dummyInt))
			dummyInt = 0;
		BB2.setSurfaceValidationSize(dummyInt);
		converter.clear();
		converter.str(m_surfaceValidationDifferenceEntry.get_text());
		if(!(converter >> dummyFloat))
			dummyFloat = 0.0;
		BB2.setSurfaceValidationDifference(dummyFloat);
		BB2.setIsBackForthValidation(m_backForthValidationCheck.get_active());
		BB2.setIsSubpixelInterpolation(m_subpixelInterpolationCheck.get_active());
	
		BB2.setStereoParams();
		m_imageArea.set_size_request(3*BB2.getWidthImage(), BB2.getHeightImage());
		Gtk::Window::show_all_children();
	}

}


bool GUIWindow::on_mouse_clicked(GdkEventButton *event)
{
	std::stringstream ssx, ssy, ssz;
	std::string str;
	double x, y;
	float distx, disty, distz;

	x = event->x;
	xImageArea = x;
	y = event->y;
	yImageArea = y;
	BB2.RCDToXYZ(yImageArea, xImageArea - 2*BB2.getWidthImage(), distx, disty, distz);

	ssx << distx;
	ssy << disty;
	ssz << distz;
	str = ssx.str() + "," + ssy.str() + "," + ssz.str();

	Gtk::Window::set_title(str);
	return true;
}