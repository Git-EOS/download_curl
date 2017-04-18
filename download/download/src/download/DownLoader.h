#pragma once
#ifndef __DOWNLOADER__
#define __DOWNLOADER__

#include <string>
#include <functional>
#include <queue>
using namespace std;

#include "../lib/curl/include/win32/curl/curl.h"
#pragma comment(lib, "src/lib/curl/libcurl_imp.lib")

typedef std::function<void(const string&)> DL_Callback;
struct DL_Info
{
	string url;
	string file;
	DL_Callback callback;
};


class DownLoader
{
public:
	DownLoader();
	~DownLoader();
	static DownLoader* getInstance();
	void downloadFile(const char* url, const char* file, const DL_Callback& func = nullptr);

	string format(const double size);
	CURL* getCurrentCurl() { return currentCurl; };
	curl_off_t getResumeOffset() { return resumeOffset; };
	double getFileLen() { return fileLen; };
	void onProgress(double progress, double speed, const char* leftTime);
	bool calcFileLen(const char* url);
private:
	bool _downloadFile(const string& url, const string& file);
	string _url;
	string _file;
	queue<DL_Info> _queue;

	long getFileSize(const string& file);
	bool initCurl(CURL *&curl, const string& url, const string& file, string& error);
	CURL* currentCurl;
	FILE* currentFp;
	double fileLen;
	curl_off_t resumeOffset;
	DL_Callback overCallback;
};
#endif