#pragma once
#include <asio/ssl.hpp>
#include <filesystem>
#include <string>

namespace sharekeys::security {
// Generates a local P-256 identity only if both files are absent. Windows private
// keys are DPAPI-protected for the current interactive user.
void createIdentity(const std::filesystem::path& directory);
void loadIdentity(asio::ssl::context& context, const std::filesystem::path& directory);
std::string fingerprint(const std::filesystem::path& certificate);
bool validPin(const std::string& pin);
bool verifyPin(X509* certificate, const std::string& pin);
std::uint64_t randomSession();
}
