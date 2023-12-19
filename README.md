# Maple

A very simple static Gemini server, now with Titan support!

### Lines-of-code
This codebase is now 351 lines of lines-of-code!

The statement "... written within a single file and liberally
spanning 159 lines-of-code." has been removed.

If you would still like the minimal 159-LOC experience, checkout
[49ce0f8](https://github.com/gemrest/maple/tree/49ce0f83b8abd1af4760e56c1673c6997ef8a2c4)!
(Docker tag `0.1.4`) Do note, that version of this project is outdated and only
has Gemini support.

## Usage

### Docker Compose

A Docker Compose file is already setup [here](./docker-compose.yaml). The only
field which should be modified is the `ports`, if need be.

```shell
docker-compose up -d
```

### Docker

```shell
docker run fuwn/maple:latest
```

### Executable

1. Build: `ninja` (requires [Ninja](https://ninja-build.org/))
2. Run: `out/maple`, or
   `TITAN=1 TITAN_TOKEN=secret TITAN_MAX_SIZE=2048 out/maple`

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

### Environment Variables

Maple can be configured with three optional environment variables:

#### `TITAN`

Enable Titan support.

Takes a value of either `true` or `1`, case-insensitive.

Defaults to off.

#### `TITAN_TOKEN`

Set a token to restrict Titan access.

Takes a string value, case-sensitive.

Defaults to no token, **anyone can upload!**

#### `TITAN_MAX_SIZE`

The maximum size in bytes which a Titan upload is permitted to perform.

Takes an integer.

Default to `1024`.

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
