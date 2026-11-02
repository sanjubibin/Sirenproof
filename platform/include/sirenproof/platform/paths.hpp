#pragma once

#include <filesystem>

namespace sirenproof::platform {

// %ProgramData%\Sirenproof: the service's state, key and logs.
std::filesystem::path program_data_dir();

// %SystemRoot%\System32\drivers\etc\hosts.
std::filesystem::path system_hosts_file();

// The folder holding the running executable.
std::filesystem::path executable_dir();
std::filesystem::path executable_path();

}  // namespace sirenproof::platform
