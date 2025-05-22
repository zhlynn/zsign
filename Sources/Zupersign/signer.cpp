#include <Zupersign.h>
#include <openssl/err.h>
#include "openssl.h"
#include "macho.h"
#include "bundle.h"
#include "signing.h"

static void print_ssl_err(char **exception, const char *reason) {
    unsigned long last_error = ERR_peek_last_error();
    if (!last_error) {
        *exception = strdup(reason);
        return;
    }
    asprintf(exception,
             "%s (error:%lx:%s:%s)",
             reason,
             last_error,
             ERR_lib_error_string(last_error),
             ERR_reason_error_string(last_error));
}

int zsign_sign(
    const char *app_directory,
    const void *cert_data,
    size_t cert_len,
    const void *private_key_data,
    size_t private_key_len,
    const entitlements_data_t *entitlements,
    size_t num_entitlements,
    void (*progress)(const void *, double),
    const void *progressContext,
    char **exception
) {
    bool success;

    progress(progressContext, -1);

    ERR_clear_error();
    BIO *private_key_bio = BIO_new_mem_buf(private_key_data, (int)private_key_len);
    if (!private_key_bio) {
        print_ssl_err(exception, "Missing private key");
        return -1;
    }

    EVP_PKEY *pkey = PEM_read_bio_PrivateKey(private_key_bio, NULL, NULL, NULL);
    BIO_free(private_key_bio);
    if (!pkey) {
        print_ssl_err(exception, "Invalid private key");
        return -1;
    }

    X509 *cert = d2i_X509(NULL, (const unsigned char **)&cert_data, cert_len);
    if (!cert) {
        print_ssl_err(exception, "Invalid developer certificate");
        EVP_PKEY_free(pkey);
        return -1;
    }

    ZSignAsset asset;
    // TODO: Use diff ents for diff products
    success = asset.Init(
        cert, pkey, "",
        string(static_cast<const char *>(entitlements[0].data), entitlements[0].len),
        false, false, false
    );
    if (!success) {
        *exception = strdup("Invalid developer certificate/private key");
        EVP_PKEY_free(pkey);
        X509_free(cert);
        return -1;
    }

    ZBundle bundle;
    success = bundle.SignFolder(
        &asset,
        app_directory,
        "",
        "",
        "",
        std::vector<string>(),
        std::vector<string>(),
        true,
        false,
        false
    );

    EVP_PKEY_free(pkey);
    X509_free(cert);

    if (!success) {
        *exception = strdup("Signing failed");
        return -1;
    }

    return 0;
}

void *zsign_analyze(const char *path, size_t *out_len, char **exception) {
    ZMachO macho;
    if (!macho.Init(path)) {
        *exception = strdup("ZMachO::Init failed");
        return NULL;
    }

    auto &archs = macho.m_arrArchOes;
    if (archs.empty()) {
        *exception = strdup("ZMachO::m_arrArchOes is empty");
        return NULL;
    }

    auto &arch = *archs.front();
    string entitlements;
    if (!ZSign::ParseCodeSignatureEntitlements(arch.m_pSignBase, entitlements)) {
        *exception = strdup("ParseCodeSignatureEntitlements failed");
        return NULL;
    }

    size_t ents_size = entitlements.size();
    *out_len = ents_size;
    void *out = malloc(ents_size);
    memcpy(out, entitlements.data(), ents_size);
    return out;
}

__attribute__((constructor)) static void ctor() {
    ZLog::SetLogLever(ZLog::eLogType::E_ERROR);
    add_signer("zsign", zsign_sign, zsign_analyze);
}
