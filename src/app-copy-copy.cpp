#include "inkview.h"
#include "inkinternal.h"
#include "curl/curl.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

struct response_data {
    char *data;
    size_t size;
};

static size_t curl_write_callback_fn(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct response_data *response = (struct response_data *)userp;

    char *ptr = (char *)realloc(response->data, response->size + realsize + 1);
    if (!ptr) {
        return 0; // Out of memory
    }

    response->data = ptr;
    memcpy(&(response->data[response->size]), contents, realsize);
    response->size += realsize;
    response->data[response->size] = 0;

    return realsize;
}

static void draw_image() {
    // iv_netinfo *netinfo = NetInfo();
	// if (netinfo->connected) {
	// 	// Already connected => nothing more to do
	// 	return;
	// }

	// const char *network_name = NULL;
	// int result = NetConnect(network_name);
	// if (result != 0) {
	// 	// Failed to connect
	// 	return;
	// }
    
    CURL *curl = curl_easy_init();
    if (!curl) {
        return;
    }

    struct response_data response = {NULL, 0};

    curl_easy_setopt(curl, CURLOPT_URL, "https://projects.xrosecky.cz/ramecek/foto.jpg");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback_fn);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || !response.data) {
        free(response.data);
        return;
    }

    char filename[256];
    snprintf(filename, sizeof(filename), "%s/Photos/foto_%d.jpg", SDCARDDIR, rand());

    FILE *f = fopen(filename, "wb+");
    if (f) {
        fwrite(response.data, 1, response.size, f);
        fclose(f);
    }
    free(response.data);
    
    // ibitmap *LoadJPEG(const char *filename, int width, int height, int br, int co, int proportional);
    ibitmap *bmp = LoadJPEG(filename, ScreenWidth(), ScreenHeight(), 100, 100, 1);

    // void StretchBitmap(int x, int y, int w, int h, const ibitmap *src, int flags);
    StretchBitmap(0, 0, ScreenWidth(), ScreenHeight(), bmp, 0);

    FullUpdate();

    // Wait for one minute and then delete the file and run again
    // int iv_unlink(const char *name);
    usleep(60000000);
}

static int main_handler(int event_type, int param_one, int param_two)
{
    switch (event_type) {
	case EVT_INIT:
        ClearScreen();

        draw_image();
		break;
    case EVT_EXIT:
        break;
    case EVT_KEYPRESS:
        if (param_one == IV_KEY_PREV) {
            CloseApp();
        }
        break;
	default:
		break;
	}

    return 0;
}


int main (int argc, char* argv[])
{
    srand( (unsigned)time(NULL) );

    InkViewMain(main_handler);

    return 0;
}