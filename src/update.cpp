#include "pch.h"
#include "update.h"
#include "config.h"
#include "gui.h"

#include <WinInet.h>
#pragma comment(lib, "wininet.lib")

enum CHECK_STATUS {
  UPDATE_IDLE,
  UPDATE_CHECKING,
  UPDATE_CHECKED,
};

static HINTERNET internet = NULL;
static CHECK_STATUS status = UPDATE_IDLE;

// from config.cpp
extern bool match_version(char **str, int *value);

// newest version
static int newest_version = 0;

// status callback
static void CALLBACK internet_status_callback(
  _In_  HINTERNET hInternet,
  _In_  DWORD_PTR dwContext,
  _In_  DWORD dwInternetStatus,
  _In_  LPVOID lpvStatusInformation,
  _In_  DWORD dwStatusInformationLength
  )
{
  if (dwInternetStatus == INTERNET_STATUS_REQUEST_COMPLETE) {
    INTERNET_ASYNC_RESULT *result = (INTERNET_ASYNC_RESULT*)lpvStatusInformation;

    if (result->dwError == 0) {
      HINTERNET handle = (HINTERNET)result->dwResult;

      DWORD size;
      char temp[1024];


      if (InternetReadFile(handle, temp, sizeof(temp), &size)) {
        if (size != 0) {
          temp[size] = 0;
          char *s = temp;
          int version = 0;
          match_version(&s, &newest_version);
          gui_notify_update(newest_version);
        }
      }

      InternetCloseHandle(handle);
    }

    InternetCloseHandle(internet);
    status = UPDATE_CHECKED;
  }
}

// check update
void update_check_async() {
  if (status != UPDATE_IDLE)
    return;

  status = UPDATE_CHECKING;

  internet = InternetOpen("FreePiano 1.8", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, INTERNET_FLAG_ASYNC);
  InternetSetStatusCallback(internet, &internet_status_callback);

  InternetOpenUrl(internet, "http://api.freepiano.tiwb.com/check_update", NULL, NULL, NULL, 1);
}