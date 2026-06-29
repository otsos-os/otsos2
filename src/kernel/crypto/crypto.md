# Kernel Cryptography Subsystem

The `crypto/` directory implements the kernel's cryptographic primitives used
for password hashing, random number generation, and future security features.

## Structure

```
crypto/
  crypto.h          — umbrella header (includes all sub-modules)
  crypto.md         — this documentation
  util/
    crypto_util.h   — hex encode/decode, constant-time compare, secure wipe
    crypto_util.c
  hash/
    sha256.h        — SHA-256 context API + one-shot hash
    sha256.c        — FIPS 180-4 compliant SHA-256
  hmac/
    hmac_sha256.h   — HMAC-SHA256 context API + one-shot MAC
    hmac_sha256.c   — RFC 2104 HMAC using SHA-256
  kdf/
    pbkdf2.h        — PBKDF2-HMAC-SHA256 + constant-time verify
    pbkdf2.c        — RFC 8018 PBKDF2 with SHA-256 PRF
  rng/
    rng.h           — CSPRNG API
    rng.c           — RDRAND/RDSEED + SHA-256 entropy pool (Fortuna-like)
  cipher/
    chacha20.h      — ChaCha20 stream cipher API
    chacha20.c      — RFC 8439 ChaCha20 (256-bit key, 96-bit nonce)
```

## Usage

```c
#include <kernel/crypto/crypto.h>
```

### SHA-256

```c
u8 digest[32];
sha256_hash(data, len, digest);
```

### HMAC-SHA256

```c
u8 mac[32];
hmac_sha256(key, key_len, data, data_len, mac);
```

### PBKDF2 (password hashing)

```c
u8 derived[32];
pbkdf2_hmac_sha256(password, pass_len,
                   salt, salt_len,
                   10000,
                   derived, 32);

int ok = pbkdf2_verify(password, pass_len,
                       salt, salt_len,
                       10000,
                       expected, 32);
```

### CSPRNG

```c
crypto_rng_init();
u8 salt[16];
crypto_rng_bytes(salt, 16);
u64 rnd = crypto_rng_u64();
crypto_rng_add_entropy(extra_data, extra_len);
```

### ChaCha20 (stream cipher)

```c
/* one-shot encrypt / decrypt (same operation: XOR with keystream) */
u8 ciphertext[len];
chacha20_encrypt(key, nonce, plaintext, len, ciphertext);

/* streaming usage */
chacha20_ctx_t ctx;
chacha20_init(&ctx, key, nonce);
chacha20_xor(&ctx, chunk1, out1, len1);
chacha20_xor(&ctx, chunk2, out2, len2);
chacha20_wipe(&ctx);
```

## Kusr Integration

The kusr authentication system uses PBKDF2-HMAC-SHA256:

- On first boot, `kusr_init()` generates a 16-byte random salt via the CSPRNG
  and derives a 32-byte key from the user's password with 10,000 iterations.
- The derived key, salt, and iteration count are stored in `/conf/kernel.toml`
  as hex strings under the `[kusr]` section (`password_hash`, `password_salt`,
  `password_iterations`).
- `api_kusr_auth()` reads the stored parameters, re-derives the key from the
  supplied password, and compares in constant time.

The previous FNV-1a 64-bit hash (which was trivially reversible and had no
salt) has been completely replaced.
