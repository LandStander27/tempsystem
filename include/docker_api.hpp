#pragma once

#include <string>

int pull_image(void* curl);
int create_container(void* curl, bool mount_cwd, bool ro_root, bool ro_cwd, bool network);
int start_container(void* curl);
int exec_in_container(void* curl, std::string cmd, bool interactive, bool attach_stdout);
int kill_container(void* curl);
int delete_container(void* curl);

extern bool docker_api_verbose;
extern std::string output_buffer;