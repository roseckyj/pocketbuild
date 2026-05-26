
#include "inkview.h"
#include "inkinternal.h"

#include <math.h>

static int g_argc;
static char **g_argv;


static ifont *font;
static const int kFontSize = 40;
static int y_log;


int wifi_activate()
{
	iv_netinfo *netinfo = NetInfo();
	if (netinfo->connected) {
		// Already connected => nothing more to do
		return 0;
	}

	const char *network_name = NULL;
	int result = NetConnect2(network_name, 1);
	if (result != 0) {
		// Failed to connect
		return 1;
	}

	// Just to be sure: check if we are, now, connected
	netinfo = NetInfo();
	if (netinfo->connected) {
		return 0;
	}

	// Connection failed, I don't know why
	return 2;
}


static void splash_screen()
{
	font = OpenFont("LiberationSans", kFontSize, 0);
	SetFont(font, BLACK);

	ClearScreen();
    DrawTextRect(0, 0, ScreenWidth(), ScreenHeight(), "Waiting for an app...", ALIGN_CENTER | VALIGN_MIDDLE);
	InvertArea(0, 0, ScreenWidth(), ScreenHeight());
	FullUpdate();
	
	CloseFont(font);
}

static int main_handler(int event_type, int param_one, int param_two)
{
	if (EVT_INIT == event_type) {
        splash_screen();
		wifi_activate();
		CloseApp();
    }

    return 0;
}


int main (int argc, char* argv[])
{
	g_argc = argc;
	g_argv = argv;

    InkViewMain(main_handler);
    return 0;
}