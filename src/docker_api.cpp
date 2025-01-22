#include <string.h>
#include <curl/curl.h>
#include <format>
#include <filesystem>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <json/value.h>
#include <json/reader.h>

#include "logging.hpp"
bool docker_api_verbose = false;
std::string output_buffer;

CURLcode start_req(void* curl) {
	output_buffer.clear();
	CURLcode res = curl_easy_perform(curl);
	if (docker_api_verbose) {
		printf("%s", output_buffer.c_str());
	}
	return res;
}

int pull_image(void* curl) {
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/images/create");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "fromImage=codeberg.org/land/tempsystem:latest");
	if (docker_api_verbose) print_debug("(/var/run/docker.socket) POST: http://localhost/images/create?fromImage=codeberg.org/land/tempsystem:latest");
	CURLcode res = start_req(curl);
	if (res != CURLE_OK) {
		print_error(std::format("curl_easy_perform(): {}", curl_easy_strerror(res)));
		return 1;
	}
	return 0;
}

int create_container(void* curl, bool mount_cwd, bool ro_root, bool ro_cwd, bool network, bool privileged) {
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
	struct curl_slist *list = NULL;
	list = curl_slist_append(list, "Content-Type: application/json");
	// print_info("Creating temporary system...");
	curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/containers/create?name=tempsystem");
	std::string json = std::format(R"(
{{
	"name": "tempsystem",
	"Image": "codeberg.org/land/tempsystem:latest",
	"Tty": true,
	"Hostname": "tempsystem",
	"NetworkDisabled": {},
	"HostConfig": {{
		"Dns": ["1.1.1.1", "1.0.0.1"],
		"Privileged": {},
		"ReadonlyRootfs": {}{}
	}}
}}
	)", network ? "true" : "false", privileged ? "true" : "false", ro_root ? "true" : "false", mount_cwd ? std::format(R"(
,
"Binds": ["{}:/home/tempsystem/work{}"]
	)", std::filesystem::current_path().c_str(), ro_cwd ? ":ro" : "") : "");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
	if (docker_api_verbose) print_debug(std::format("(/var/run/docker.socket) POST: http://localhost/containers/create?name=tempsystem\nPOST data: {}", json));
	CURLcode res = start_req(curl);
	if (res != CURLE_OK) {
		print_error(std::format("curl_easy_perform(): {}", curl_easy_strerror(res)));
		return 1;
	}
	return 0;
}

int start_container(void* curl) {
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
	curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/containers/tempsystem/start");
	if (docker_api_verbose) print_debug("(/var/run/docker.socket) POST: http://localhost/containers/tempsystem/start");
	CURLcode res = start_req(curl);
	if (res != CURLE_OK) {
		print_error(std::format("curl_easy_perform(): {}", curl_easy_strerror(res)));
		return 1;
	}
	return 0;
}

int exec_in_container(void* curl, std::string cmd, bool interactive, bool attach_stdout) {
	if (attach_stdout) {
		if (docker_api_verbose) print_debug(std::format("Executing: /bin/docker exec -it tempsystem /usr/bin/zsh -{}c \"{}; exit\"", interactive ? "i" : "", cmd));
		if (fork() == 0) {
			const std::string tmp = std::format("{}; exit", cmd);
			char* tmp2 = (char*)tmp.c_str();
			int status;
			// if (interactive) {
			// 	char* args[] = { (char*)"/bin/docker", (char*)"exec", (char*)"-it", (char*)"tempsystem", (char*)"/usr/bin/zsh", (char*)"-c", tmp2, NULL };
			// 	status = execvp("/bin/docker", args);
			// } else {
			// 	char* args[] = { (char*)"/bin/docker", (char*)"exec", (char*)"tempsystem", (char*)"/usr/bin/zsh", (char*)"-c", tmp2, NULL };
			// 	status = execvp("/bin/docker", args);
			// }
			std::string s = std::format("-{}c", interactive ? "i" : "");
			char* args[] = { (char*)"/bin/docker", (char*)"exec", (char*)"-it", (char*)"tempsystem", (char*)"/usr/bin/zsh", (char*)s.c_str(), tmp2, NULL };
			status = execvp("/bin/docker", args);
			if (status != -1) {
				print_error(std::format("execvp(): {}", strerror(errno)));
			}
		}
		
		int status = 1;
		if (wait(&status) == -1) {
			print_error(std::format("wait(): {}", strerror(errno)));
			return 1;
		}
		if (!WIFEXITED(status)) {
			print_error(std::format("execvp(): {}", strerror(errno)));
			return 1;
		}
		if (WEXITSTATUS(status) != 0) {
			print_error(std::format("execvp(): {}", strerror(errno)));
			return 1;
		}
	} else {
		std::string::size_type n = 0;
		while ((n = cmd.find("\"", n)) != std::string::npos) {
			cmd.replace(n, 1, "\\\"");
			n += 2;
		}
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
		struct curl_slist *list = NULL;
		list = curl_slist_append(list, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
		std::string json = std::format(R"(
{{
	"AttachStdin": true,
	"AttachStdout": true,
	"AttachStderr": true,
	"Cmd": ["/usr/bin/zsh", "-c", "{}"],
	"User": "tempsystem"
}}
		)", cmd);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
		curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/containers/tempsystem/exec");
		if (docker_api_verbose) print_debug(std::format("(/var/run/docker.socket) POST: http://localhost/containers/tempsystem/exec\nPOST data: {}", json));
		CURLcode res = start_req(curl);
		if (res != CURLE_OK) {
			print_error(std::format("curl_easy_perform(): {}", curl_easy_strerror(res)));
			return 1;
		}
		
		Json::CharReaderBuilder builder;
		Json::CharReader* reader = builder.newCharReader();
		Json::Value response;
		std::string errors;
		bool worked = reader->parse(
			output_buffer.c_str(),
			output_buffer.c_str() + output_buffer.size(),
			&response,
			&errors
		);
		if (!worked) {
			print_error(std::format("Json::CharReader::parse(): {}", errors));
			return 1;
		}
		
		json = std::format(R"(
{{
	"Detach": false,
	"Tty": false
}}
		)", cmd);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
		curl_easy_setopt(curl, CURLOPT_URL, std::format("http://localhost/exec/{}/start", response.get("Id", "undefined").asString()).c_str());
		if (docker_api_verbose) print_debug(std::format("(/var/run/docker.socket) POST: http://localhost/exec/{}/start\nPOST data: {}", response.get("Id", "undefined").asString(), json));
		res = start_req(curl);
		if (res != CURLE_OK) {
			print_error(std::format("curl_easy_perform(): {}", curl_easy_strerror(res)));
			return 1;
		}
		
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
		curl_easy_setopt(curl, CURLOPT_URL, std::format("http://localhost/exec/{}/json", response.get("Id", "undefined").asString()).c_str());
		if (docker_api_verbose) print_debug(std::format("(/var/run/docker.socket) GET: http://localhost/exec/{}/start", response.get("Id", "undefined").asString()));
		res = start_req(curl);
		if (res != CURLE_OK) {
			print_error(std::format("curl_easy_perform(): {}", curl_easy_strerror(res)));
			return 1;
		}
		worked = reader->parse(
			output_buffer.c_str(),
			output_buffer.c_str() + output_buffer.size(),
			&response,
			&errors
		);
		delete reader;
		if (!worked) {
			print_error(std::format("Json::CharReader::parse(): {}", errors));
			return 1;
		}
		
		return response["ExitCode"].asInt();
		// return 0;
	}

	return 0;
}

int kill_container(void* curl) {
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
	curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/containers/tempsystem/kill");
	if (docker_api_verbose) print_debug("(/var/run/docker.socket) POST: http://localhost/containers/tempsystem/kill");
	CURLcode res = start_req(curl);
	if (res != CURLE_OK) {
		print_error(std::format("curl_easy_perform(): {}", curl_easy_strerror(res)));
		return 1;
	}
	return 0;
}

int delete_container(void* curl) {
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
	curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/containers/tempsystem");
	if (docker_api_verbose) print_debug("(/var/run/docker.socket) DELETE: http://localhost/containers/tempsystem");
	CURLcode res = start_req(curl);
	if (res != CURLE_OK) {
		print_error(std::format("curl_easy_perform(): {}", curl_easy_strerror(res)));
		return 1;
	}
	return 0;
}
