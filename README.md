# Maple

A small static Gemini server with optional Titan uploads.

## Build and Run

Maple requires a C++20 Clang compiler, Ninja, pkg-config, and OpenSSL development
libraries. Python 3 and the OpenSSL command-line tool are needed for tests.

For a new capsule:

```sh
ninja
ninja check
bin/genkey
printf '# Welcome\n' > .maple/gmi/index.gmi
build/maple
```

Maple listens on TCP port 1965 by default. Set `MAPLE_PORT` to an unsigned decimal
integer from 1 to 65535 to use another port, for example `MAPLE_PORT=1966 build/maple`.

With Nix:

```sh
nix develop
nix build
```

The Nix package also runs the regression suite during its build.

## Content and Certificates

All paths are relative to the server's **working directory**, not its executable:

```text
.maple/
  public.pem
  private.pem
  gmi/
    index.gmi
```

`bin/genkey` creates a self-signed certificate and refuses to overwrite existing
keys. You can instead provide your own certificate and matching private key.
Keep the private key readable only by the account running Maple.

Maple serves regular `.gmi` files; directory requests use `index.gmi`. Changes
become visible without a restart. Symlinks and `.` or `..` path components are
rejected. Content directories must already exist before uploading into them.

## Titan

| Variable | Default | Meaning |
| --- | --- | --- |
| `TITAN` | Disabled | `1` or `true`, case-insensitive, enables uploads. |
| `TITAN_TOKEN` | Empty | Required upload token. An empty token permits unauthenticated uploads. |
| `TITAN_MAX_SIZE` | `1024` | Maximum upload bytes, written as an unsigned decimal integer. |

Uploads require `mime=text/gemini`, a valid `size`, and a `.gmi` destination.
Completed uploads replace files atomically; incomplete uploads preserve them.
New files are private to the server account.

Maple serves one connection at a time. The TLS handshake and request header share
a five-second absolute deadline. Uploads and responses have a 15-second
inactivity timeout.

## Docker

Prepare `.maple` as above, then run:

```sh
MAPLE_UID=$(id -u) MAPLE_GID=$(id -g) docker compose up --build -d
```

The user and group overrides let the container read your private key and write
uploads without widening file permissions. Compose passes through the three
Titan variables; uploads remain disabled by default.

## License

This project is licensed with the [GNU General Public License v3.0](./LICENSE).
