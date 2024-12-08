// #include <stdio.h>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <curl/curl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <argparse/argparse.hpp>

#include "logging.hpp"

struct memory {
	char *response;
	size_t size;
};

static size_t cb(void *data, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	return realsize;
}

int main(int argc, char* argv[]) {
	
	argparse::ArgumentParser program("tempsystem");
	program.add_argument("-v", "--verbose").help("output stream incoming from the Docker Rest API").default_value(false).implicit_value(true);
	try {
		program.parse_args(argc, argv);
	} catch (const std::exception& err) {
		std::cerr << err.what() << std::endl;
		std::cerr << program;
		exit(1);
	}
	
	void* curl = curl_easy_init();
	if (!curl) {
		print_error("curl_easy_init(): failed");
		return 1;
	}
	curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
	
	if (program["--verbose"] == false) {
		struct memory chunk = {0, 0};
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	}
	// curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1);
	
	print_info("Pulling git.kage.sj.strangled.net/land/tempsystem:latest...");
	curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/images/create");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "fromImage=git.kage.sj.strangled.net/land/tempsystem:latest");
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		print_error(std::format("curl_easy_perform(): {}", curl_easy_strerror(res)));
		return 1;
	}
	
	struct curl_slist *list = NULL;
	list = curl_slist_append(list, "Content-Type: application/json");
	print_info("Creating temporary system...");
	curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/containers/create?name=tempsystem");
	std::string json = std::format(R"({{
		"name": "tempsystem",
		"Image": "git.kage.sj.strangled.net/land/tempsystem:latest",
		"Tty": true,
		"Hostname": "tempsystem",
		"HostConfig": {{
			"Binds": ["{}:/home/tempsystem/work"]
		}}
	}})", std::filesystem::current_path().c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		print_error(std::format("curl_easy_perform(): {}", curl_easy_strerror(res)));
		return 1;
	}
	
	print_info("Starting system...");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
	curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/containers/tempsystem/start");
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		print_error(std::format("curl_easy_perform(): {}", curl_easy_strerror(res)));
		return 1;
	}
	
	print_info("Entering...");
	int status = system("docker exec -it tempsystem /usr/bin/zsh");
	if (!WIFEXITED(status)) {
		print_error(std::format("system(): failed"));
		return 1;
	}
	if (WEXITSTATUS(status) != 0) {
		print_error(std::format("docker exec failed"));
		return 1;
	}

	print_info("Killing system...");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
	curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/containers/tempsystem/kill");
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		print_error(std::format("curl_easy_perform(): {}", curl_easy_strerror(res)));
		return 1;
	}
	
	print_info("Removing system...");
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
	curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/containers/tempsystem");
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		print_error(std::format("curl_easy_perform(): {}", curl_easy_strerror(res)));
		return 1;
	}
	
	curl_easy_cleanup(curl);
	
	return 0;
	
}
