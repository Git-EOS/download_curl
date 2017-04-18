#include "DownLoader.h"
#include <thread>
#include <iostream>
#include <io.h>


static DownLoader* sp_DownLoader = NULL;
DownLoader::DownLoader()
{
}

DownLoader::~DownLoader()
{
}

DownLoader* DownLoader::getInstance()
{
	if (sp_DownLoader == NULL)
	{
		sp_DownLoader = new DownLoader;
	}
	return sp_DownLoader;
}

long DownLoader::getFileSize(const string& file)
{
	FILE *fp = fopen(file.c_str(), "r");
	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fclose(fp);
	return size;
}

#define UNIT_G 1073741824
#define UNIT_M 1048576
#define UNIT_K 1024
string DownLoader::format(const double size)
{
	char ret[10] = "";
	string unit = "B";
	double newSize = 0.f;
	if (size > UNIT_G)
	{
		unit = "G";
		newSize = size / UNIT_G;
	}
	else if (size > UNIT_M)
	{
		unit = "M";
		newSize = size / UNIT_M;
	}
	else if (size > UNIT_K)
	{
		unit = "KB";
		newSize = size / UNIT_K;
	}
	sprintf(ret, "%.2f%s", newSize, unit.c_str());
	return ret;
}

void DownLoader::onProgress(double progress, double speed, const char* leftTime)
{
	if (currentFp == nullptr) return;

	printf("progress: %.2f\n", progress);
	printf("speed: %s/s\n", format(speed).c_str());
	printf("left time: %s\n ", leftTime);
	if ((int)progress == 100)
	{
		if (overCallback)
		{
			fclose(currentFp);
			currentFp = nullptr;
			rename((_file + ".dl").c_str(), _file.c_str());

			overCallback(_file);
		}
	}
}

static size_t dl_get_file_len(void *ptr, size_t size, size_t nmemb, void *data)
{
	return size * nmemb;
}

size_t dl_write_func(char *buffer, size_t size, size_t nitems, void *outstream)
{
	int written = fwrite(buffer, size, nitems, (FILE*)outstream);
	return written;
}

static int dl_progress_func(void *ptr, double dltotal, double dlnow, double ultotal, double ulnow) {
	if (dltotal == 0) return -1;

	DownLoader* downloader = (DownLoader*)ptr;
	CURL* curl = downloader->getCurrentCurl();
	char timeFormat[12] = "";
	double speed; //下载速度
	curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD, &speed);

	//string seepStr = downloader->format(speed);
	//printf("speed: %.2f%s/s", seepStr.c_str());

	double progress = 0;
	double leftTime = 0;

	curl_off_t resumeOffset = downloader->getResumeOffset();
	double fileLen = downloader->getFileLen();
	{
		progress = (dlnow + resumeOffset) / fileLen * 100;
		leftTime = (fileLen - dlnow - resumeOffset) / speed;
		//printf("progress: %.2f", progress);

		char timeFormat[12] = "";
		int hours = leftTime / 3600;
		int minutes = (leftTime - hours * 3600) / 60;
		int seconds = leftTime - hours * 3600 - minutes * 60;
		sprintf(timeFormat, "%02d:%02d:%02d", hours, minutes, seconds);
		//printf("left time: %s\n ", timeFormat);

		downloader->onProgress(progress, speed, timeFormat);
	}
	
	return 0;
}

bool DownLoader::initCurl(CURL *&curl, const string& url, const string& file, string& error)
{
	CURLcode code;
	string file_dl = file + ".dl";
	FILE *fp = fopen(file_dl.c_str(), "r");
	long fsize = 0;
	if (fp == NULL)
	{
		fp = fopen(file_dl.c_str(), "wb");
		currentFp = fp;
	}
	else
	{
		fseek(fp, 0, SEEK_END);
		fsize = ftell(fp);
		fclose(fp);
		if (fsize == (int)fileLen)
		{
			printf("downloaded the complete file \n");
			if (overCallback)
			{
				overCallback(_file);
			}
			return false;
		}
		fp = fopen(file_dl.c_str(), "ab+");//追加
		currentFp = fp;
	}

	curl = curl_easy_init();
	if (curl == NULL)
	{
		printf("Failed to create CURL connection\n");
		return false;
	}
	code = curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error); //error回传
	if (code != CURLE_OK)
	{
		printf("Failed to set error buffer [%d]\n", code);
		return false;
	}
	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
	code = curl_easy_setopt(curl, CURLOPT_URL, url.c_str()); //设置url
	if (code != CURLE_OK)
	{
		printf("Failed to set URL [%s]\n", error);
		return false;
	}
	code = curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1); //自动进行跳转抓取,比如点击弹出新窗口
	if (code != CURLE_OK)
	{
		printf("Failed to set redirect option [%s]\n", error);
		return false;
	}

	code = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, dl_write_func); //写入回调
	if (code != CURLE_OK)
	{
		printf("Failed to set write func [%s]\n", error);
		return false;
	}
	code = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)fp); //要存储的文件指针
	if (code != CURLE_OK)
	{
		printf("Failed to set write data [%s]\n", error);
		return false;
	}

	code = curl_easy_setopt(curl, CURLOPT_RESUME_FROM, fsize);  //断点续传
	printf("downloaded_file_offset: %ld\n", fsize);
	resumeOffset = fsize;
	if (code != CURLE_OK)
	{
		printf("Failed to set resume [%s]\n", error);
		return false;
	}

	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, dl_progress_func); //下载进度回调方法
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this); // 传入本类对象

	currentCurl = curl;

	return true;
}

void DownLoader::downloadFile(const char* url, const char* file, const DL_Callback& func)
{
	if (_access(file, 0) != -1)
	{
		//文件已经存在,无需下载
		printf("file is exist [%s]\n", file);
		return;
	}
	if (_file.size() > 0)
	{
		//下载中,加入下载队列
		_queue.push(DL_Info{string(url), string(file), func});
		return;
	}
	overCallback = func;
	_url = string(url);
	_file = string(file);

	std::thread([this]()
	{
		do 
		{
			printf("start download  url = %s\n", _url.c_str());

			bool dl_succ = _downloadFile(_url, _file);
			if (dl_succ)
			{
				curl_easy_cleanup(currentCurl);
			}

			if (_queue.empty())
			{
				break;
			}
			else
			{
				DL_Info info = _queue.front();
				overCallback = info.callback;
				_url = string(info.url);
				_file = string(info.file);
				_queue.pop();
			}
		} while (true);
	}).detach();
}

bool DownLoader::_downloadFile(const string& url, const string& file)
{
	if (!calcFileLen(url.c_str()))
	{
		return false;
	}

	CURL *curl = NULL;
	CURLcode code;
	string error;

	code = curl_global_init(CURL_GLOBAL_DEFAULT);
	if (code != CURLE_OK)
	{
		printf("Failed to global init default [%d]\n", code);
		curl_easy_cleanup(currentCurl);
		return false;
	}

	if (!initCurl(curl, url, file, error))
	{
		printf("Failed to init curl \n");
		curl_easy_cleanup(currentCurl);
		return false;
	}

	code = curl_easy_perform(curl);
	if (code != CURLE_OK)
	{
		printf("Failed to get '%s' [%s]\n", url.c_str(), error);
		curl_easy_cleanup(currentCurl);
		return false;
	}

	long retcode = 0;
	code = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &retcode);
	printf("curl_ret_code: %d\n", retcode);
	if ((code == CURLE_OK) && retcode == 200)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool DownLoader::calcFileLen(const char* url)
{
	bool ret = false;
	CURL *handle = curl_easy_init();
	curl_easy_setopt(handle, CURLOPT_URL, url);
	curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "GET"); //设置为get方式获取
	curl_easy_setopt(handle, CURLOPT_NOBODY, 1);
	CURLcode code = curl_easy_perform(handle);
	//查看是否有出错信息  
	if (code == CURLE_OK) 
	{
		long retcode = 0;
		code = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &retcode);
		if ((code == CURLE_OK) && retcode == 200)
		{
			curl_easy_getinfo(handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &fileLen);
			printf("calcFileLen: len = %d\n", (int)fileLen);
			ret = true;
		}
		else
		{
			printf("calcFileLen: not find 404! \n");
			fileLen = -1;
		}
	}
	else {
		fileLen = -1;
	}
	curl_easy_cleanup(handle);
	return ret;
}