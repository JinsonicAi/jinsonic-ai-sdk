#include <curl/curl.h>

#include <iostream>
#include <string>
#include <thread>

class AlarmPusher {
public:
	AlarmPusher(const std::string& serverBaseUrl, long timeoutSeconds = 5)
		: base_url(serverBaseUrl), timeout(timeoutSeconds) {
		curl_global_init(CURL_GLOBAL_ALL);
	}

	~AlarmPusher() {
		curl_global_cleanup();
	}

	void sendAsync(const std::string& apiSuffix, const std::string& jsonPayload) {
		std::thread([=]() {
			CURL* curl = curl_easy_init();
			if (!curl) {
				std::cerr << "CURL init failed\n";
				return;
			}

			std::string		   full_url = base_url + apiSuffix;
			struct curl_slist* headers	= nullptr;
			headers						= curl_slist_append(headers, "Content-Type: application/json");

			curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

			CURLcode res = curl_easy_perform(curl);
			if (res != CURLE_OK) {
				std::cerr << "[Async] Push failed: " << curl_easy_strerror(res) << "\n";
			}

			curl_slist_free_all(headers);
			curl_easy_cleanup(curl);
		}).detach();
	}

private:
	std::string base_url;
	long		timeout;
};
