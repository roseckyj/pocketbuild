#include "inkview.h"
#include "inkinternal.h"
#include "curl/curl.h"
#include <exception>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

struct response_data {
    char *data;
    size_t size;
};

static size_t curl_write_file_callback_fn(void *contents, size_t size, size_t nmemb, void *userp) {
    FILE *f = (FILE *)userp;
    if (!f) {
        return 0;
    }
    size_t items_written = fwrite(contents, size, nmemb, f);
    return items_written * size;
}

static bool file_exists(const char *path) {
    return path && path[0] != '\0' && access(path, F_OK) == 0;
}

static bool hash_file_fnv1a64(const char *path, uint64_t *out_hash) {
    if (!out_hash) {
        return false;
    }
    *out_hash = 0;
    if (!path || path[0] == '\0') {
        return false;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        return false;
    }

    uint64_t hash = 14695981039346656037ULL; // FNV offset basis
    const uint64_t prime = 1099511628211ULL;
    unsigned char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i < n; i++) {
            hash ^= (uint64_t)buf[i];
            hash *= prime;
        }
    }

    bool ok = (ferror(f) == 0);
    fclose(f);
    if (!ok) {
        return false;
    }

    *out_hash = hash;
    return true;
}

static int download_url_to_file(const char *url, const char *output_path) {
    if (!url || url[0] == '\0' || !output_path || output_path[0] == '\0') {
        return -1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        return -2;
    }

    FILE *out = fopen(output_path, "wb");
    if (!out) {
        curl_easy_cleanup(curl);
        return -3;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_file_callback_fn);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "PocketBook/app-copy");
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    // Ask intermediaries and servers not to cache the response.
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Cache-Control: no-cache");
    headers = curl_slist_append(headers, "Pragma: no-cache");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    fflush(out);
    fclose(out);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK || http_code < 200 || http_code >= 300) {
        (void)unlink(output_path);
        return (int)res != 0 ? (int)res : (int)http_code;
    }

    return 0;
}

static void show_error(const char *msg) {
    if (!msg) {
        msg = "(null)";
    }
    Message(ICON_ERROR, "app-copy", msg, 5 * 1000);
}

static int ensure_wifi_connected() {
    iv_netinfo *netinfo = NetInfo();
    if (netinfo && netinfo->connected) {
        return 0;
    }

    // On some firmwares calling NetConnect/NetConnect2 from apps can crash.
    // Require Wi-Fi to be connected by the user in system UI.
    return 1;
}

static const char *get_photo_dir() {
    // This directory is known to exist on many PocketBook devices (as used in app.cpp).
    if (access("/mnt/ext2", F_OK) == 0) {
        return "/mnt/ext2/Photo";
    }

    // Fallback: use SDCARDDIR if available.
    static char fallback[256];
    if (SDCARDDIR && SDCARDDIR[0] != '\0') {
        snprintf(fallback, sizeof(fallback), "%s/Photo", SDCARDDIR);
        return fallback;
    }

    return "/tmp";
}

static bool ensure_dir_exists(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }
    if (access(path, F_OK) == 0) {
        return true;
    }
    if (mkdir(path, 0775) == 0) {
        return true;
    }
    return errno == EEXIST;
}

static const int kRefreshMs = 10 * 1000;
static const char *kRefreshTimerName = "pocketbuild-refresh";
static void refresh_timer();

static void keep_awake_tick() {
    // Prevent the device from going to sleep / timed poweroff while we are actively running.
    // BanSleep expects seconds.
    BanSleep(60);
    (void)PostponeTimedPoweroff();

    // Keep Wi-Fi radio powered while the app is active.
    // Note: This does not force-connect to an AP (NetConnect2 can crash on some firmwares),
    // it only asks the system to keep Wi-Fi powered.
    (void)WiFiPower(1);
}

static void draw_image() {
    char filename[256];
    char tmpname[256];
    const char *photo_dir = get_photo_dir();
    if (!ensure_dir_exists(photo_dir)) {
        show_error("Cannot create/find Photo directory");
        return;
    }

    int wifi_res = ensure_wifi_connected();
    if (wifi_res != 0) {
        show_error("Wi-Fi is not connected. Please connect in Settings first, then re-open the app.");
        return;
    }

    // Persist a stable "current" file so we can hash/compare across updates.
    snprintf(filename, sizeof(filename), "%s/current.jpg", photo_dir);
    snprintf(tmpname, sizeof(tmpname), "%s/current.tmp", photo_dir);

    const char *base_url = "http://192.168.0.10:8888/photo.jpg";
    char url[512];
    unsigned long t = (unsigned long)time(NULL);
    // Cache-busting: make URL unique each update attempt.
    if (strchr(base_url, '?')) {
        snprintf(url, sizeof(url), "%s&t=%lu", base_url, t);
    } else {
        snprintf(url, sizeof(url), "%s?t=%lu", base_url, t);
    }

    int dl = download_url_to_file(url, tmpname);
    if (dl != 0) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Download failed (code %d)", dl);
        show_error(buf);
        return;
    }

    uint64_t new_hash = 0;
    if (!hash_file_fnv1a64(tmpname, &new_hash)) {
        (void)unlink(tmpname);
        show_error("Failed to hash downloaded file");
        return;
    }

    if (file_exists(filename)) {
        uint64_t old_hash = 0;
        if (hash_file_fnv1a64(filename, &old_hash) && old_hash == new_hash) {
            // No change => keep current screen.
            (void)unlink(tmpname);
            return;
        }
    }

    // Load from tmp first so a bad download doesn't replace the last good image.
    ibitmap *bmp = LoadJPEG(tmpname, ScreenWidth(), ScreenHeight(), 100, 100, 1);

    if (!bmp) {
        (void)unlink(tmpname);
        show_error("LoadJPEG() failed (file missing or invalid)");
        return;
    }

    // Replace current on disk (atomic on POSIX when same filesystem).
    if (rename(tmpname, filename) != 0) {
        // If rename fails, keep tmp around only briefly.
        (void)unlink(tmpname);
    }

    // void StretchBitmap(int x, int y, int w, int h, const ibitmap *src, int flags);
    StretchBitmap(0, 0, ScreenWidth(), ScreenHeight(), bmp, 0);

    FullUpdate();
}

static void refresh_timer() {
    try {
        keep_awake_tick();
        draw_image();
    } catch (const std::exception &e) {
        show_error(e.what());
    } catch (...) {
        show_error("Unknown exception");
    }

    // Re-arm timer after the work completes to avoid overlapping executions.
    SetWeakTimer(kRefreshTimerName, refresh_timer, kRefreshMs);
}

static int main_handler(int event_type, int param_one, int param_two)
{

    try {
    switch (event_type) {
	case EVT_INIT:
        ClearScreen();
        FullUpdate();

    keep_awake_tick();

    // Kick off periodic refresh.
    refresh_timer();
		break;
    case EVT_EXIT:
    ClearTimerByName(kRefreshTimerName);
        break;
    case EVT_KEYPRESS:
        if (param_one == IV_KEY_PREV) {
            CloseApp();
        }
        break;
	default:
		break;
	}

    } catch (const std::exception &e) {
        show_error(e.what());
    } catch (...) {
        show_error("Unknown exception");
    }

    return 0;
}


int main (int argc, char* argv[])
{
    srand( (unsigned)time(NULL) );

    curl_global_init(CURL_GLOBAL_DEFAULT);

    InkViewMain(main_handler);

    curl_global_cleanup();

    return 0;
}