#include <format>
#include <curl/curl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>

#include <argparse/argparse.hpp>

#include "logging.hpp"
#include "docker_api.hpp"

static size_t cb(void* data, size_t size, size_t nmemb, void *userp) {
	// print_debug(std::format("{}", size * nmemb));
	((std::string*)userp)->append((char*)data, size * nmemb);
	return size * nmemb;
}

void revert(void* curl) {
	print_info("Killing system...");
	int status = kill_container(curl);
	if (status != 0) {
		print_error(std::format("kill_container(): {}", status));
		revert(curl);
		exit(status);
	}

	print_info("Removing system...");
	status = delete_container(curl);
	if (status != 0) {
		print_error(std::format("delete_container(): {}", status));
		exit(status);
	}
}

int main(int argc, char* argv[]) {
	
	argparse::ArgumentParser program("tempsystem");
	program.add_argument("-v", "--verbose").help("increase output verbosity").default_value(false).implicit_value(true);
	program.add_argument("-r", "--ro-root").help("mount system root as read only").default_value(false).implicit_value(true);
	program.add_argument("-c", "--ro-cwd").help("mount current directory as read only").default_value(false).implicit_value(true);
	program.add_argument("-m", "--disable-cwd-mount").help("do not mount current directory to ~/work").default_value(false).implicit_value(true);
	program.add_argument("-n", "--no-network").help("disable network capabilities for the system").default_value(false).implicit_value(true);
	program.add_argument("-p", "--extra-packages").help("extra packages to install in the system, space deliminated");
	// program.add_argument("-R", "--read-only").help("quickhand for -r -c").default_value(false).implicit_value(true);
	
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
		// curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
		// curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output_buffer);
		docker_api_verbose = false;
	} else {
		docker_api_verbose = true;
	}
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output_buffer);
	
	int status;
	print_info("Pulling codeberg.org/land/tempsystem:latest...");
	status = pull_image(curl);
	if (status != 0) {
		print_error(std::format("pull_image(): {}", status));
		return status;
	}

	print_info("Creating temporary system...");
	status = create_container(curl,
		program["--disable-cwd-mount"] == false,
		program["--ro-root"] == true,
		program["--ro-cwd"] == true,
		program["--no-network"] == true
	);
	if (status != 0) {
		print_error(std::format("create_container(): {}", status));
		return status;
	}

	print_info("Starting system...");
	status = start_container(curl);
	if (status != 0) {
		print_error(std::format("start_container(): {}", status));
		revert(curl);
		return status;
	}
	
	if (program.is_used("--extra-packages")) {
		std::string pkgs = program.get<std::string>("--extra-packages");
		wordexp_t p;
		wordexp(pkgs.c_str(), &p, WRDE_NOCMD);
		char** w = p.we_wordv;
		for (long unsigned int i = 0; i < p.we_wordc; i++) {
			print_info(std::format("Installing package {}", w[i]));
			status = exec_in_container(curl, std::format("/bin/pacman -Ssq \"^{}$\" | grep -x \"{}\"", w[i], w[i]), false, true);
			if (status != 0) {
				print_error(std::format("{} package does not exist.", w[i]));
				wordfree(&p);
				revert(curl);
				return status;
			}
			status = exec_in_container(curl, std::format("/bin/sudo /bin/pacman -S --needed --noconfirm {}", w[i]), false, true);
			if (status != 0) {
				print_error(std::format("/bin/sudo /bin/pacman -S --needed --noconfirm {}: {}", w[i], status));
				wordfree(&p);
				revert(curl);
				return status;
			}
		}
		wordfree(&p);
	}

	print_info("Entering...");
	status = exec_in_container(curl, "/usr/bin/zsh", true, true);
	if (status != 0) {
		print_error(std::format("exec_in_container(\"/usr/bin/zsh\"): {}", status));
		revert(curl);
		return status;
	}

	revert(curl);
	
	curl_easy_cleanup(curl);
	
	return 0;
}
