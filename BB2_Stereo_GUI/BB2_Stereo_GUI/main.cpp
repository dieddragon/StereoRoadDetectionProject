#include "GUI.h"

int main(int argc, char* argv[])
{
	printf("Hello world!\n");

	Gtk::Main kit(argc, argv);

	GUIWindow guiWindow;
	//Shows the window and returns when it is closed
	Gtk::Main::run(guiWindow);

	return 0;
}