/*
 * libjingle
 * Copyright 2004--2008, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if HAVE_OPENSSL_SSL_H

#include "base/opensslidentity.h"

// Must be included first before openssl headers.
#include "base/win32.h"  // NOLINT

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/crypto.h>

#include "base/helpers.h"
#include "base/logging.h"
#include "base/openssldigest.h"

namespace talk_base {

// We could have exposed a myriad of parameters for the crypto stuff,
// but keeping it simple seems best.

// Strength of generated keys. Those are RSA.
static const int KEY_LENGTH = 1024;

// Random bits for certificate serial number
static const int SERIAL_RAND_BITS = 64;

// Certificate validity lifetime
static const int CERTIFICATE_LIFETIME = 60*60*24*365;  // one year, arbitrarily

// Generate a key pair. Caller is responsible for freeing the returned object.
static EVP_PKEY* MakeKey() {
  LOG(LS_INFO) << "Making key pair";
  EVP_PKEY* pkey = EVP_PKEY_new();
#if OPENSSL_VERSION_NUMBER < 0x00908000l
  // Only RSA_generate_key is available. Use that.
  RSA* rsa = RSA_generate_key(KEY_LENGTH, 0x10001, NULL, NULL);
  if (!EVP_PKEY_assign_RSA(pkey, rsa)) {
    EVP_PKEY_free(pkey);
    RSA_free(rsa);
    return NULL;
  }
#else
  // RSA_generate_key is deprecated. Use _ex version.
  BIGNUM* exponent = BN_new();
  RSA* rsa = RSA_new();
  if (!pkey || !exponent || !rsa ||
      !BN_set_word(exponent, 0x10001) ||  // 65537 RSA exponent
      !RSA_generate_key_ex(rsa, KEY_LENGTH, exponent, NULL) ||
      !EVP_PKEY_assign_RSA(pkey, rsa)) {
    EVP_PKEY_free(pkey);
    BN_free(exponent);
    RSA_free(rsa);
    return NULL;
  }
  // ownership of rsa struct was assigned, don't free it.
  BN_free(exponent);
#endif
  LOG(LS_INFO) << "Returning key pair";
  return pkey;
}

// Generate a self-signed certificate, with the public key from the
// given key pair. Caller is responsible for freeing the returned object.
static X509* MakeCertificate(EVP_PKEY* pkey, const char* common_name) {
  LOG(LS_INFO) << "Making certificate for " << common_name;
  X509* x509 = NULL;
  BIGNUM* serial_number = NULL;
  X509_NAME* name = NULL;

  if ((x509=X509_new()) == NULL)
    goto error;

  if (!X509_set_pubkey(x509, pkey))
    goto error;

  // serial number
  // temporary reference to serial number inside x509 struct
  ASN1_INTEGER* asn1_serial_number;
  if ((serial_number = BN_new()) == NULL ||
      !BN_pseudo_rand(serial_number, SERIAL_RAND_BITS, 0, 0) ||
      (asn1_serial_number = X509_get_serialNumber(x509)) == NULL ||
      !BN_to_ASN1_INTEGER(serial_number, asn1_serial_number))
    goto error;

  if (!X509_set_version(x509, 0L))  // version 1
    goto error;

  // There are a lot of possible components for the name entries. In
  // our P2P SSL mode however, the certificates are pre-exchanged
  // (through the secure XMPP channel), and so the certificate
  // identification is arbitrary. It can't be empty, so we set some
  // arbitrary common_name. Note that this certificate goes out in
  // clear during SSL negotiation, so there may be a privacy issue in
  // putting anything recognizable here.
  if ((name = X509_NAME_new()) == NULL ||
      !X509_NAME_add_entry_by_NID(name, NID_commonName, MBSTRING_UTF8,
                                     (unsigned char*)common_name, -1, -1, 0) ||
      !X509_set_subject_name(x509, name) ||
      !X509_set_issuer_name(x509, name))
    goto error;

  if (!X509_gmtime_adj(X509_get_notBefore(x509), 0) ||
      !X509_gmtime_adj(X509_get_notAfter(x509), CERTIFICATE_LIFETIME))
    goto error;

  if (!X509_sign(x509, pkey, EVP_sha1()))
    goto error;

  BN_free(serial_number);
  X509_NAME_free(name);
  LOG(LS_INFO) << "Returning certificate";
  return x509;

 error:
  BN_free(serial_number);
  X509_NAME_free(name);
  X509_free(x509);
  return NULL;
}

// This dumps the SSL error stack to the log.
static void LogSSLErrors(const std::string& prefix) {
  char error_buf[200];
  unsigned long err;

  while ((err = ERR_get_error()) != 0) {
    ERR_error_string_n(err, error_buf, sizeof(error_buf));
    LOG(LS_ERROR) << prefix << ": " << error_buf << "\n";
  }
}

OpenSSLKeyPair* OpenSSLKeyPair::Generate() {
  EVP_PKEY* pkey = MakeKey();
  if (!pkey) {
    LogSSLErrors("Generating key pair");
    return NULL;
  }
  return new OpenSSLKeyPair(pkey);
}

OpenSSLKeyPair::~OpenSSLKeyPair() {
  EVP_PKEY_free(pkey_);
}

void OpenSSLKeyPair::AddReference() {
  CRYPTO_add(&pkey_->references, 1, CRYPTO_LOCK_EVP_PKEY);
}

#ifdef _DEBUG
// Print a certificate to the log, for debugging.
static void PrintCert(X509* x509) {
  BIO* temp_memory_bio = BIO_new(BIO_s_mem());
  if (!temp_memory_bio) {
    LOG_F(LS_ERROR) << "Failed to allocate temporary memory bio";
    return;
  }
  X509_print_ex(temp_memory_bio, x509, XN_FLAG_SEP_CPLUS_SPC, 0);
  BIO_write(temp_memory_bio, "\0", 1);
  char* buffer;
  BIO_get_mem_data(temp_memory_bio, &buffer);
  LOG(LS_VERBOSE) << buffer;
  BIO_free(temp_memory_bio);
}
#endif

OpenSSLCertificate* OpenSSLCertificate::Generate(
    OpenSSLKeyPair* key_pair, const std::string& common_name) {
  std::string actual_common_name = common_name;
  if (actual_common_name.empty())
    // Use a random string, arbitrarily 8chars long.
    actual_common_name = CreateRandomString(8);
  X509* x509 = MakeCertificate(key_pair->pkey(), actual_common_name.c_str());
  if (!x509) {
    LogSSLErrors("Generating certificate");
    return NULL;
  }
#ifdef _DEBUG
  PrintCert(x509);
#endif
  return new OpenSSLCertificate(x509);
}

OpenSSLCertificate* OpenSSLCertificate::FromPEMString(
    const std::string& pem_string, int* pem_length) {
  BIO* bio = BIO_new_mem_buf(const_cast<char*>(pem_string.c_str()), -1);
  if (!bio)
    return NULL;
  (void)BIO_set_close(bio, BIO_NOCLOSE);
  BIO_set_mem_eof_return(bio, 0);
  X509 *x509 = PEM_read_bio_X509(bio, NULL, NULL,
                                 const_cast<char*>("\0"));
  char *ptr;
  int remaining_length = BIO_get_mem_data(bio, &ptr);
  BIO_free(bio);
  if (pem_length)
    *pem_length = pem_string.length() - remaining_length;
  if (x509)
    return new OpenSSLCertificate(x509);
  else
    return NULL;
}

bool OpenSSLCertificate::ComputeDigest(const std::string &algorithm,
                                       unsigned char *digest,
                                       std::size_t size,
                                       std::size_t *length) const {
  return ComputeDigest(x509_, algorithm, digest, size, length);
}

bool OpenSSLCertificate::ComputeDigest(const X509 *x509,
                                       const std::string &algorithm,
                                       unsigned char *digest,
                                       std::size_t size,
                                       std::size_t *length) {
  const EVP_MD *md;
  unsigned int n;

  if (!OpenSSLDigest::GetDigestEVP(algorithm, &md))
    return false;

  if (size < static_cast<size_t>(EVP_MD_size(md)))
    return false;

  X509_digest(x509, md, digest, &n);

  *length = n;

  return true;
}

OpenSSLCertificate::~OpenSSLCertificate() {
  X509_free(x509_);
}

std::string OpenSSLCertificate::ToPEMString() const {
  BIO* bio = BIO_new(BIO_s_mem());
  if (!bio)
    return NULL;
  if (!PEM_write_bio_X509(bio, x509_)) {
    BIO_free(bio);
    return NULL;
  }
  BIO_write(bio, "\0", 1);
  char* buffer;
  BIO_get_mem_data(bio, &buffer);
  std::string ret(buffer);
  BIO_free(bio);
  return ret;
}

void OpenSSLCertificate::AddReference() const {
  CRYPTO_add(&x509_->references, 1, CRYPTO_LOCK_X509);
}

OpenSSLIdentity* OpenSSLIdentity::Generate(const std::string& common_name) {
  OpenSSLKeyPair *key_pair = OpenSSLKeyPair::Generate();
  if (key_pair) {
    OpenSSLCertificate *certificate =
        OpenSSLCertificate::Generate(key_pair, common_name);
    if (certificate)
      return new OpenSSLIdentity(key_pair, certificate);
    delete key_pair;
  }
  LOG(LS_INFO) << "Identity generation failed";
  return NULL;
}

bool OpenSSLIdentity::ConfigureIdentity(SSL_CTX* ctx) {
  // 1 is the documented success return code.
  if (SSL_CTX_use_certificate(ctx, certificate_->x509()) != 1 ||
     SSL_CTX_use_PrivateKey(ctx, key_pair_->pkey()) != 1) {
    LogSSLErrors("Configuring key and certificate");
    return false;
  }
  return true;
}

}  // namespace talk_base

#endif  // HAVE_OPENSSL_SSL_H


