#ifndef MAPLE_HH
#define MAPLE_HH

#include <openssl/ssl.h>

namespace maple {
  static int maple_socket;
  static SSL_CTX *ssl_context;

  auto exit_with[[noreturn]](const char *, bool) -> void;
}

#endif // MAPLE_HH
