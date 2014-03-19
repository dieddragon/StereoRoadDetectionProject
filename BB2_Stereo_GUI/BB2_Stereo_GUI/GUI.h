#ifndef BB2_STEREO_GUI_GUI_H__
#define BB2_STEREO_GUI_GUI_H__

#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <iostream>
#include <gtkmm.h>
#include <string.h>
#include <windows.h>
#include <sstream>

#include "camera.h"
#include "Bumblebee2.h"


class GUIWindow : public Gtk::Window
{

public:
	GUIWindow();
	virtual ~GUIWindow();

	int xImageArea;
	int yImageArea;

protected:
	//Signal Handlers
	void on_button_start_clicked();
	void on_button_startsave_clicked();
	void on_button_setstereoparams_clicked();
	bool on_mouse_clicked(GdkEventButton *event);
	bool on_timeout();

	//Member Widgets
	Gtk::Button m_setStereoParamsButton;
	Gtk::ToggleButton m_startButton;
	Gtk::ToggleButton m_startSaveButton;
	Gtk::Table m_mainButtonTable;
	Gtk::Table m_rightButtonTable;
	Gtk::VPaned m_vpaned;
	Gtk::HPaned m_hpaned;
	Gtk::Label m_rightLabel;
	Gtk::Entry m_minDispEntry;
	Gtk::Entry m_maxDispEntry;
	Gtk::Entry m_widthEntry;
	Gtk::Entry m_heightEntry;
	Gtk::Entry m_sizeStereoMaskEntry;
	Gtk::CheckButton m_textureValidationCheck;
	Gtk::Entry m_textureValidationThresholdEntry;	//0.0 - 128.0, good values are between 0.0 - 2.0
	Gtk::CheckButton m_surfaceValidationCheck;
	Gtk::Entry m_surfaceValidationSizeEntry;	//Common parameter values range from 100 to 500, depending on image resolution.
	Gtk::Entry m_surfaceValidationDifferenceEntry;	//???
	Gtk::CheckButton m_backForthValidationCheck;
	Gtk::CheckButton m_subpixelInterpolationCheck;
	Gtk::DrawingArea m_imageArea;

	sigc::connection m_timer;
	
	int m_timeout;
	Bumblebee2 BB2;
	
};




#endif BB2_STEREO_GUI_GUI_H