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
#include "info.hpp"

static bool quiet = false;

static size_t cb(void* data, size_t size, size_t nmemb, void *userp) {
	((std::string*)userp)->append((char*)data, size * nmemb);
	return size * nmemb;
}

void revert(void* curl) {
	if (!quiet) print_info("Killing system...");
	int status = kill_container(curl);
	if (status != 0) {
		print_error(std::format("kill_container(): {}", status));
		revert(curl);
		exit(status);
	}

	if (!quiet) print_info("Removing system...");
	status = delete_container(curl);
	if (status != 0) {
		print_error(std::format("delete_container(): {}", status));
		exit(status);
	}
}

int main(int argc, char* argv[]) {
	
	argparse::ArgumentParser program("tempsystem", version);
	program.add_argument("-q", "--quiet").help("disable logging").default_value(false).implicit_value(true);
	program.add_argument("-v", "--verbose").help("increase output verbosity").default_value(false).implicit_value(true);
	program.add_argument("-u", "--update-system").help("run a system update before entering").default_value(false).implicit_value(true);
	program.add_argument("-r", "--ro-root").help("mount system root as read only (cannot be used with --extra-packages)").default_value(false).implicit_value(true);
	program.add_argument("-c", "--ro-cwd").help("mount current directory as read only").default_value(false).implicit_value(true);
	program.add_argument("-m", "--disable-cwd-mount").help("do not mount current directory to ~/work").default_value(false).implicit_value(true);
	program.add_argument("-n", "--no-network").help("disable network capabilities for the system (cannot be used with --extra-packages)").default_value(false).implicit_value(true);
	program.add_argument("-p", "--extra-packages").help("extra packages to install in the system, space deliminated (cannot be used with --no-network or --ro-root)");
	program.add_argument("-ap", "--extra-aur-packages").help("same as --extra-packages, but fetches the packages from the AUR");
	program.add_argument("-x", "--privileged").help("give extended privileges to the system").default_value(false).implicit_value(true);
	program.add_argument("-i", "--interactive").help("execute COMMAND as interactive (only used when COMMAND is supplied)").default_value(false).implicit_value(true);
	program.add_argument("COMMAND").help("command to execute in container, then exit").default_value("/usr/bin/zsh");
	
	try {
		program.parse_args(argc, argv);
	} catch (const std::exception& err) {
		std::cerr << err.what() << std::endl;
		std::cerr << program;
		exit(1);
	}

	quiet = program["--quiet"] == true;
	void* curl = curl_easy_init();
	if (!curl) {
		print_error("curl_easy_init(): failed");
		return 1;
	}
	curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, "/var/run/docker.sock");
	
	if (program["--verbose"] == false) {
		docker_api_verbose = false;
	} else {
		docker_api_verbose = true;
	}
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output_buffer);
	
	int status;
	if (!quiet) print_info("Pulling codeberg.org/land/tempsystem:latest...");
	status = pull_image(curl);
	if (status != 0) {
		print_error(std::format("pull_image(): {}", status));
		return status;
	}

	if (!quiet) print_info("Creating temporary system...");
	status = create_container(curl,
		program["--disable-cwd-mount"] == false,
		program["--ro-root"] == true,
		program["--ro-cwd"] == true,
		program["--no-network"] == true,
		program["--privileged"] == true
	);
	if (status != 0) {
		print_error(std::format("create_container(): {}", status));
		return status;
	}

	if (!quiet) print_info("Starting system...");
	status = start_container(curl);
	if (status != 0) {
		print_error(std::format("start_container(): {}", status));
		revert(curl);
		return status;
	}
	
	if (program["--no-network"] == false) {
		if (program["--update-system"] == true) {
			if (!quiet) print_info("Updating system...");
			status = exec_in_container(curl, "/bin/sudo /bin/pacman -Syu --noconfirm", false, false);
			if (status != 0) {
				print_error(std::format("exec_in_container(\"/bin/pacman -Syu --noconfirm\"): {}", status));
				revert(curl);
				return status;
			}
		}
	} else {
		if (!quiet) print_info("No network, skipping system ugrade");
	}
	
	if (program.is_used("--extra-packages")) {
		std::string pkgs = program.get<std::string>("--extra-packages");
		wordexp_t p;
		wordexp(pkgs.c_str(), &p, WRDE_NOCMD);
		char** w = p.we_wordv;
		for (long unsigned int i = 0; i < p.we_wordc; i++) {
			if (!quiet) print_info(std::format("Installing package {}", w[i]));
			status = exec_in_container(curl, std::format("/bin/pacman -Ssq \"^{}$\" | grep -x \"{}\"", w[i], w[i]), false, false);
			if (status != 0) {
				print_error(std::format("{} package does not exist.", w[i]));
				wordfree(&p);
				revert(curl);
				return status;
			}
			status = exec_in_container(curl, std::format("/bin/sudo /bin/pacman -S --needed --noconfirm {}", w[i]), false, false);
			if (status != 0) {
				print_error(std::format("/bin/sudo /bin/pacman -S --needed --noconfirm {}: {}", w[i], status));
				wordfree(&p);
				revert(curl);
				return status;
			}
		}
		wordfree(&p);
	}
	
	if (program.is_used("--extra-aur-packages")) {
		std::string pkgs = program.get<std::string>("--extra-aur-packages");
		wordexp_t p;
		wordexp(pkgs.c_str(), &p, WRDE_NOCMD);
		char** w = p.we_wordv;
		for (long unsigned int i = 0; i < p.we_wordc; i++) {
			if (!quiet) print_info(std::format("Installing package {} from AUR", w[i]));
			status = exec_in_container(curl, std::format("/bin/yay --aur -Ssq \"{}\" | grep -x \"{}\"", w[i], w[i]), false, false);
			if (status != 0) {
				print_error(std::format("{} package does not exist.", w[i]));
				wordfree(&p);
				revert(curl);
				return status;
			}
			status = exec_in_container(curl, std::format("/bin/yay --sync --needed --noconfirm --noprogressbar {}", w[i]), false, false);
			if (status != 0) {
				print_error(std::format("/bin/yay --sync --needed --noconfirm --noprogressbar {}: {}", w[i], status));
				wordfree(&p);
				revert(curl);
				return status;
			}
		}
		wordfree(&p);
	}

	if (!quiet) print_info("Executing command...");
	if (program.is_used("COMMAND")) {
		status = exec_in_container(curl, program.get("COMMAND"), program["--interactive"] == true, true);
		if (status != 0) {
			print_error(std::format("COMMAND returned non-zero exitcode: {}", status));
		}
	} else {
		status = exec_in_container(curl, std::format("export SHOW_WELCOME=true; {}", program.get("COMMAND")), true, true);
		if (status != 0) {
			print_error(std::format("COMMAND returned non-zero exitcode: {}", status));
		}
	}

	revert(curl);
	
	curl_easy_cleanup(curl);
	
	return 0;
}
