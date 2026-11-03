#pragma once

#include <filesystem>
#include <vector>

namespace sirenproof::platform {

// %ProgramData%\Sirenproof: the service's state, key and logs. Known folders come from Windows itself,
// never from environment variables.
std::filesystem::path program_data_dir();

// Program Files and Program Files (x86).
std::vector<std::filesystem::path> program_files_dirs();

// %SystemRoot%\System32\drivers\etc\hosts.
std::filesystem::path system_hosts_file();

// The folder holding the running executable.
std::filesystem::path executable_dir();
std::filesystem::path executable_path();

}  // namespace sirenproof::platform
