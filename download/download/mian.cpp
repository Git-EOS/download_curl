#include <string>
#include <thread>
#include <iostream>
#include <functional>
#include <list>

#include "src/download/DownLoader.h"
#pragma comment(lib, "src/lib/curl/libcurl_imp.lib")

#include "src/lib/curl/include/win32/curl/curl.h"


using namespace std;

int main()
{
	list <std::function<void()>> *eventList = new list <std::function<void()>>;
	DownLoader::getInstance()->downloadFile("http://127.0.0.1:47/download/te.7z", "dl_te.7z", [=](const string& file) {
		eventList->push_back([=]() {
			printf("file download over 111 file:%s\n", file.c_str());
		});
	});
	DownLoader::getInstance()->downloadFile("http://127.0.0.1:47/download/tex.txt", "dl_te.txt", [=](const string& file) {
		eventList->push_back([=]() {
			printf("file download over 111 file:%s\n", file.c_str());
		});
	});
	while (true)
	{
		Sleep(1);
		if (!eventList->empty())
		{
			//防止多线程产生的数据冲突（简陋版）
			for(auto var : *eventList)
			{
				var();
			}
			eventList->clear();
		}
	}
	
	return 0;
}