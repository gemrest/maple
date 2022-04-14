# Maple

A very simple static Gemini server; written within a single file and liberally
spanning 152 lines-of-code.

## Usage

### Certificates

Before any possible method of usage; you must generate or use a preexisting
server certificate.

If you do not have an existing certificate, generate one using the provided
certificate generations script: [`bin/genkey`](bin/genkey) (requires
[OpenSSL](https://www.openssl.org/)).

The server certificate must be placed in a directory -- directly adjacent to the
Docker Compose file or executable -- named [`.maple`](.maple). The public and
private keys must be named `public.pem` and `private.pem`, respectively.

### Gemini Content

Gemini content must be placed in a directory -- directly adjacent to the
Docker Compose file or executable -- named [`.maple/gmi`](.maple/gmi) and
ending with the file extension `.gmi`.

### Docker Compose

A Docker Compose file is already setup [here](./docker-compose.yaml). The only
field which should be modified is the `ports`, if you have to.

`$ docker-compose up -d` (requires [Docker](https://www.docker.com/) and/ or
[Docker Compose](https://docs.docker.com/compose/))

### Executable

1. Build: `ninja` (requires [Ninja](https://ninja-build.org/))
2. Run: `out/maple`

### Hacking

If you decide to tamper with Maple, you should give your commits a test. As
there isn't a test suite yet, feel free to use any of these examples:

```bash
# Using OpenSSL directly to debug different requests:
$ openssl s_client -ign_eof -quiet -connect localhost:1965 <<< \
> "gemini://localhost:1965"

# Using Bollux to debug proper requests... or any client!
$ bollux localhost
```

## Capsules using Maple

[Add yours!](https://github.com/gemrest/maple/edit/main/README.md)

- <gemini://gem.rest/> (<https://gem.rest/>)

## License

This project is licensed with the [GNU General Public License v3.0](./LICENSE).
