#pragma once
#include <string>
#include <vector>

namespace tyr { namespace profiles {
std::vector<std::string> list();
bool create(const std::string& name, const std::string& contents, std::string& error);
bool write(const std::string& name, const std::string& contents, std::string& error);
bool read(const std::string& name, std::string& contents, std::string& error);
bool remove(const std::string& name, std::string& error);
bool exportCopy(const std::string& name, std::string& path, std::string& error);
} }
