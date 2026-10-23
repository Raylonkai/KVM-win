#include "sharekeys/security.hpp"
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <memory>
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace sharekeys::security {
namespace {
template<class T, auto Free> using Owned = std::unique_ptr<T, decltype(Free)>;
void require(bool ok) { if (!ok) throw std::runtime_error("identity cryptography failed"); }
std::string read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot read identity");
    return {std::istreambuf_iterator<char>(stream), {}};
}
void writeNew(const std::filesystem::path& path, const std::string& data) {
#ifdef _WIN32
    const auto file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot create identity file");
    DWORD written{};
    const bool ok = WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) && written == data.size();
    CloseHandle(file);
#else
    const int file = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (file < 0) throw std::runtime_error("cannot create identity file");
    const bool ok = ::write(file, data.data(), data.size()) == static_cast<ssize_t>(data.size());
    ::close(file);
#endif
    if (!ok) throw std::runtime_error("cannot write identity file");
}
std::string protect(const std::string& bytes, bool encrypt) {
#ifdef _WIN32
    DATA_BLOB input{static_cast<DWORD>(bytes.size()), reinterpret_cast<BYTE*>(const_cast<char*>(bytes.data()))}, output{};
    const bool ok = encrypt ? CryptProtectData(&input, L"ShareKeys identity", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)
                            : CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output);
    if (!ok) throw std::runtime_error("DPAPI identity operation failed");
    std::string result(reinterpret_cast<char*>(output.pbData), output.cbData);
    SecureZeroMemory(output.pbData, output.cbData); LocalFree(output.pbData);
    return result;
#else
    (void)encrypt; return bytes;
#endif
}
std::string digest(X509* cert) {
    unsigned char bytes[EVP_MAX_MD_SIZE]{}; unsigned length{};
    require(cert && X509_digest(cert, EVP_sha256(), bytes, &length) == 1);
    std::ostringstream out;
    for (unsigned i = 0; i < length; ++i) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(bytes[i]);
    return out.str();
}
std::string bioString(BIO* bio) {
    char* buffer{}; const auto length = BIO_get_mem_data(bio, &buffer);
    require(length > 0); return {buffer, static_cast<std::size_t>(length)};
}
}
bool validPin(const std::string& pin) {
    return pin.size() == 64 && pin.find_first_not_of("0123456789abcdef") == std::string::npos;
}
bool verifyPin(X509* cert, const std::string& pin) {
    if (!cert || !validPin(pin) || X509_cmp_current_time(X509_get0_notBefore(cert)) >= 0 || X509_cmp_current_time(X509_get0_notAfter(cert)) <= 0) return false;
    const auto actual = digest(cert);
    return actual.size() == pin.size() && CRYPTO_memcmp(actual.data(), pin.data(), pin.size()) == 0;
}
void createIdentity(const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    if (std::filesystem::exists(directory / "identity.crt") || std::filesystem::exists(directory / "identity.key")) throw std::runtime_error("identity already exists; refusing overwrite");
    Owned<EVP_PKEY_CTX, EVP_PKEY_CTX_free> ctx(EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr), EVP_PKEY_CTX_free);
    require(ctx && EVP_PKEY_keygen_init(ctx.get()) == 1 && EVP_PKEY_CTX_set_ec_paramgen_curve_nid(ctx.get(), NID_X9_62_prime256v1) == 1);
    EVP_PKEY* raw{}; require(EVP_PKEY_keygen(ctx.get(), &raw) == 1);
    Owned<EVP_PKEY, EVP_PKEY_free> key(raw, EVP_PKEY_free);
    Owned<X509, X509_free> cert(X509_new(), X509_free);
    require(cert && X509_set_version(cert.get(), 2) == 1);
    require(ASN1_INTEGER_set_uint64(X509_get_serialNumber(cert.get()), randomSession()) == 1);
    require(X509_gmtime_adj(X509_getm_notBefore(cert.get()), -60) && X509_gmtime_adj(X509_getm_notAfter(cert.get()), 31536000L));
    require(X509_set_pubkey(cert.get(), key.get()) == 1);
    auto* name = X509_get_subject_name(cert.get());
    require(X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("ShareKeys local identity"), -1, -1, 0) == 1);
    require(X509_set_issuer_name(cert.get(), name) == 1 && X509_sign(cert.get(), key.get(), EVP_sha256()) > 0);
    Owned<BIO, BIO_free> certBio(BIO_new(BIO_s_mem()), BIO_free), keyBio(BIO_new(BIO_s_mem()), BIO_free);
    require(certBio && keyBio && PEM_write_bio_X509(certBio.get(), cert.get()) == 1 && PEM_write_bio_PrivateKey(keyBio.get(), key.get(), nullptr, nullptr, 0, nullptr, nullptr) == 1);
    auto privateKey = bioString(keyBio.get());
    writeNew(directory / "identity.key", protect(privateKey, true));
    OPENSSL_cleanse(privateKey.data(), privateKey.size());
    writeNew(directory / "identity.crt", bioString(certBio.get()));
}
void loadIdentity(asio::ssl::context& context, const std::filesystem::path& directory) {
    require(SSL_CTX_set_min_proto_version(context.native_handle(), TLS1_3_VERSION) == 1);
    context.set_options(asio::ssl::context::no_compression);
    const auto certificate = read(directory / "identity.crt");
    context.use_certificate_chain(asio::buffer(certificate));
    auto privateKey = protect(read(directory / "identity.key"), false);
    context.use_private_key(asio::buffer(privateKey), asio::ssl::context::pem);
    OPENSSL_cleanse(privateKey.data(), privateKey.size());
    require(SSL_CTX_check_private_key(context.native_handle()) == 1);
}
std::string fingerprint(const std::filesystem::path& path) {
    const auto bytes = read(path);
    Owned<BIO, BIO_free> bio(BIO_new_mem_buf(bytes.data(), static_cast<int>(bytes.size())), BIO_free);
    Owned<X509, X509_free> cert(PEM_read_bio_X509(bio.get(), nullptr, nullptr, nullptr), X509_free);
    return digest(cert.get());
}
std::uint64_t randomSession() {
    std::uint64_t value{};
    do { require(RAND_bytes(reinterpret_cast<unsigned char*>(&value), sizeof(value)) == 1); } while (value == 0);
    return value;
}
}
