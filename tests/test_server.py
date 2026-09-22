"""Run with MAPLE_BINARY pointing to the executable under test."""

import os
import pathlib
import select
import shutil
import signal
import socket
import ssl
import subprocess
import tempfile
import time
import unittest

CONFIGURATION_TIMEOUT_SECONDS = 0.5
CONNECTION_TIMEOUT_SECONDS = 2
DEFAULT_PORT = 1965
IDLE_CONNECTION_TIMEOUT_SECONDS = 1
PROCESS_EXIT_TIMEOUT_SECONDS = 2
QUEUED_REQUEST_TIMEOUT_SECONDS = 35
RECEIVE_BUFFER_BYTES = 65536
REQUEST_EXPIRY_TIMEOUT_SECONDS = 7
REQUEST_FRAGMENT_DELAY_SECONDS = 0.02
RESPONSE_PREFIX_BYTES = 4096
SERVER_SETTLE_SECONDS = 0.1
SHUTDOWN_SETTLE_SECONDS = 0.05
STARTUP_POLL_INTERVAL_SECONDS = 0.01
STARTUP_PROBE_TIMEOUT_SECONDS = 0.1
STARTUP_TIMEOUT_SECONDS = 3
TRANSFER_EXPIRY_TIMEOUT_SECONDS = 17

class ServerTests(unittest.TestCase):
    @classmethod
    def setUpClass(test_class):
        test_class.binary = pathlib.Path(os.environ.get('MAPLE_BINARY', 'build/maple')).resolve()
        test_class.certificates = tempfile.TemporaryDirectory(prefix='maple-certificates-')
        test_class.certificate_directory = pathlib.Path(test_class.certificates.name)

        subprocess.run(
            ['openssl', 'req', '-x509', '-newkey', 'ec', '-pkeyopt',
             'ec_paramgen_curve:prime256v1', '-nodes', '-days', '1',
             '-subj', '/CN=localhost', '-keyout', str(test_class.certificate_directory / 'private.pem'),
             '-out', str(test_class.certificate_directory / 'public.pem')],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )

        test_class.context = ssl._create_unverified_context()

    @classmethod
    def tearDownClass(test_class):
        test_class.certificates.cleanup()

    def setUp(self):
        self.workspace = tempfile.TemporaryDirectory(prefix='maple-server-')

        self.addCleanup(self.workspace.cleanup)

        self.directory = pathlib.Path(self.workspace.name)
        self.content = self.directory / '.maple/gmi'

        self.content.mkdir(parents=True)
        (self.content / 'index.gmi').write_bytes(b'INDEX')
        (self.content / 'existing.gmi').write_bytes(b'ORIGINAL')
        (self.directory / 'outside').mkdir()
        (self.directory / 'outside/index.gmi').write_bytes(b'OUTSIDE')

        for name in ('public.pem', 'private.pem'):
            shutil.copyfile(self.certificate_directory / name, self.directory / '.maple' / name)

        self.process = None

        self.addCleanup(self.stop)

    def start(self, **settings):
        with socket.socket() as reservation:
            reservation.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            reservation.bind(('127.0.0.1', DEFAULT_PORT))

        environment = {key: value for key, value in os.environ.items() if not key.startswith('TITAN')}

        environment.update(TITAN='1', TITAN_MAX_SIZE='8192')
        environment.update(settings)

        self.process = subprocess.Popen(
            [str(self.binary)], cwd=self.directory, env=environment,
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        startup_deadline_seconds = time.monotonic() + STARTUP_TIMEOUT_SECONDS

        while time.monotonic() < startup_deadline_seconds:
            if self.process.poll() is not None:
                self.fail(f'Server exited at startup: {self.process.returncode}')

            try:
                with socket.create_connection(('127.0.0.1', DEFAULT_PORT), timeout=STARTUP_PROBE_TIMEOUT_SECONDS):
                    return
            except ConnectionRefusedError:
                time.sleep(STARTUP_POLL_INTERVAL_SECONDS)

        self.fail('Server did not start')

    def stop(self):
        if self.process is not None:
            if self.process.poll() is None:
                self.process.terminate()

            try:
                self.process.wait(timeout=PROCESS_EXIT_TIMEOUT_SECONDS)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=PROCESS_EXIT_TIMEOUT_SECONDS)

    def connect(self, timeout_seconds=CONNECTION_TIMEOUT_SECONDS):
        connection = socket.create_connection(('127.0.0.1', DEFAULT_PORT), timeout=timeout_seconds)

        try:
            return self.context.wrap_socket(connection, server_hostname='localhost')
        except BaseException:
            connection.close()

            raise

    def request(self, *parts):
        with self.connect() as connection:
            for part in parts:
                connection.sendall(part)
                time.sleep(REQUEST_FRAGMENT_DELAY_SECONDS)

            return self.read_response(connection)

    @staticmethod
    def read_response(connection):
        with connection.makefile('rb') as stream:
            return stream.read()

    def upload(self, path, body, declared_size_bytes=None, token=''):
        declared_size_bytes = len(body) if declared_size_bytes is None else declared_size_bytes
        header = f'titan://localhost{path};mime=text/gemini;size={declared_size_bytes}'

        if token:
            header += ';token=' + token

        return self.request(header.encode() + b'\r\n' + body)

    def test_existing_file(self):
        self.start()
        self.assertEqual(self.request(b'gemini://localhost/existing.gmi\r\n'),
                         b'20 text/gemini\r\nORIGINAL')

    def test_upload_traversal(self):
        self.start()

        response = self.upload('/../../outside/index.gmi', b'CHANGED')

        self.assertEqual((self.directory / 'outside/index.gmi').read_bytes(), b'OUTSIDE')
        self.assertTrue(response.startswith(b'50'))

    def test_index_traversal(self):
        self.start()
        self.assertNotIn(b'OUTSIDE', self.request(b'gemini://localhost/../../outside/\r\n'))

    def test_symlinks(self):
        (self.content / 'linked.gmi').symlink_to(self.directory / 'outside/index.gmi')
        (self.content / 'linked').symlink_to(self.directory / 'outside', target_is_directory=True)
        self.start()
        self.assertNotIn(b'OUTSIDE', self.request(b'gemini://localhost/linked.gmi\r\n'))
        self.assertNotIn(b'OUTSIDE', self.request(b'gemini://localhost/linked/\r\n'))
        self.upload('/linked.gmi', b'CHANGED')
        self.upload('/linked/index.gmi', b'CHANGED')
        self.assertEqual((self.directory / 'outside/index.gmi').read_bytes(), b'OUTSIDE')

    def test_new_upload_is_visible(self):
        self.start()
        self.upload('/new.gmi', b'NEW')
        self.assertEqual(self.request(b'gemini://localhost/new.gmi\r\n'), b'20 text/gemini\r\nNEW')

    def test_missing_index(self):
        self.start()
        self.assertTrue(self.request(b'gemini://localhost/missing/\r\n').startswith(b'51'))

    def test_malformed_titan_keeps_server_alive(self):
        self.start()

        with self.connect() as connection:
            connection.sendall(b'titan://localhost/existing.gmi;size=3')

        time.sleep(SERVER_SETTLE_SECONDS)
        self.assertIsNone(self.process.poll())
        self.assertEqual((self.content / 'existing.gmi').read_bytes(), b'ORIGINAL')
        self.assertTrue(self.request(b'gemini://localhost/\r\n').startswith(b'20'))

    def test_strict_upload_size(self):
        self.start(TITAN_MAX_SIZE='4')

        for declared_size_bytes in ('5', '1junk', '-1', '99999999999999999999999999'):
            with self.subTest(size=declared_size_bytes):
                response = self.upload('/existing.gmi', b'OVERSIZED', declared_size_bytes=declared_size_bytes)

                self.assertTrue(response.startswith(b'59'))
                self.assertEqual((self.content / 'existing.gmi').read_bytes(), b'ORIGINAL')

    def test_upload_uses_declared_size_across_tls_records(self):
        self.start(TITAN_MAX_SIZE='4')

        header = b'titan://localhost/existing.gmi;mime=text/gemini;size=4\r\n'
        payload = b'DATAEXTRA'

        for records in ((header + payload,), (header + payload[:4], payload[4:]),
                        (header + payload[:2], payload[2:])):
            with self.subTest(records=len(records), first_record_bytes=len(records[0])):
                (self.content / 'existing.gmi').write_bytes(b'ORIGINAL')

                incoming = ssl.MemoryBIO()
                outgoing = ssl.MemoryBIO()
                session = self.context.wrap_bio(incoming, outgoing, server_hostname='localhost')

                with socket.create_connection(('127.0.0.1', DEFAULT_PORT), timeout=CONNECTION_TIMEOUT_SECONDS) as transport:
                    while True:
                        try:
                            session.do_handshake()

                            break
                        except ssl.SSLWantReadError:
                            transport.sendall(outgoing.read())

                            encrypted_bytes = transport.recv(RECEIVE_BUFFER_BYTES)

                            self.assertTrue(encrypted_bytes)
                            incoming.write(encrypted_bytes)

                    # Queue every TLS record before sending to avoid a race with the response.
                    for record in records:
                        session.write(record)

                    transport.sendall(outgoing.read())

                    response = bytearray()

                    while b'\r\n' not in response:
                        try:
                            chunk = session.read(RESPONSE_PREFIX_BYTES)
                        except ssl.SSLWantReadError:
                            try:
                                encrypted_bytes = transport.recv(RECEIVE_BUFFER_BYTES)
                            except ConnectionResetError:
                                # Unread surplus ciphertext can reset TCP when the server closes.
                                break

                            if not encrypted_bytes:
                                break

                            incoming.write(encrypted_bytes)

                            continue

                        if not chunk:
                            break

                        response.extend(chunk)

                    if response:
                        self.assertTrue(response.startswith(b'20'), response)

                self.assertEqual((self.content / 'existing.gmi').read_bytes(), b'DATA')
                self.assertIsNone(self.process.poll())

    def test_empty_upload_ignores_surplus_bytes(self):
        self.start(TITAN_MAX_SIZE='0')

        response = self.upload('/existing.gmi', b'EXTRA', declared_size_bytes=0)

        self.assertTrue(response.startswith(b'20'))
        self.assertEqual((self.content / 'existing.gmi').read_bytes(), b'')

    def test_negative_configured_size(self):
        environment = {**os.environ, 'TITAN': '1', 'TITAN_MAX_SIZE': '-1'}
        result = subprocess.run([str(self.binary)], cwd=self.directory, env=environment,
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                timeout=CONFIGURATION_TIMEOUT_SECONDS)

        self.assertNotEqual(result.returncode, 0)

    def test_fragmented_request_and_body(self):
        self.start()
        self.assertEqual(self.request(b'gemini://local', b'host/existing.gmi\r', b'\n'),
                         b'20 text/gemini\r\nORIGINAL')

        response = self.request(b'titan://localhost/new.gmi;mime=text/gemini;size=4\r\n', b'DA', b'TA')

        self.assertTrue(response.startswith(b'20'))
        self.assertEqual((self.content / 'new.gmi').read_bytes(), b'DATA')

    def test_incomplete_upload_preserves_file(self):
        self.start()

        with self.connect() as connection:
            connection.sendall(b'titan://localhost/existing.gmi;mime=text/gemini;size=4\r\na')
            time.sleep(SERVER_SETTLE_SECONDS)

        self.assertEqual((self.content / 'existing.gmi').read_bytes(), b'ORIGINAL')

    def test_large_upload(self):
        body_size_bytes = 64 * 1024
        body = b'X' * body_size_bytes

        self.start(TITAN_MAX_SIZE=str(body_size_bytes))
        self.assertTrue(self.upload('/large.gmi', body).startswith(b'20'))
        self.assertEqual((self.content / 'large.gmi').read_bytes(), body)

    def test_wrong_token(self):
        self.start(TITAN_TOKEN='expected')
        self.assertTrue(self.upload('/existing.gmi', b'BAD', token='wrong').startswith(b'59'))
        self.assertEqual((self.content / 'existing.gmi').read_bytes(), b'ORIGINAL')

    def test_failed_write(self):
        self.start()
        self.assertTrue(self.upload('/missing/file.gmi', b'DATA').startswith(b'50'))

    def test_upload_replaces_without_following_hard_links(self):
        os.link(self.directory / 'outside/index.gmi', self.content / 'hardlink.gmi')
        self.start()
        self.assertTrue(self.upload('/hardlink.gmi', b'NEW').startswith(b'20'))
        self.assertEqual((self.content / 'hardlink.gmi').read_bytes(), b'NEW')
        self.assertEqual((self.directory / 'outside/index.gmi').read_bytes(), b'OUTSIDE')
        self.assertEqual(list(self.content.glob('.maple-upload-*')), [])

    def test_parameter_order_and_empty_upload(self):
        self.start(TITAN_TOKEN='expected')

        response = self.request(b'titan://localhost/empty.gmi;token=expected;size=0;mime=text/gemini\r\n')

        self.assertTrue(response.startswith(b'20'))
        self.assertEqual((self.content / 'empty.gmi').read_bytes(), b'')

    def test_duplicate_and_missing_parameters(self):
        self.start()

        for parameters in (b'size=1;size=1;mime=text/gemini', b'size=1',
                           b'mime=text/gemini', b'size=1;mime=text/plain',
                           b'size=1;mime=text/gemini;broken'):
            with self.subTest(parameters=parameters):
                response = self.request(b'titan://localhost/existing.gmi;' + parameters + b'\r\nx')

                self.assertTrue(response.startswith(b'59'))
                self.assertEqual((self.content / 'existing.gmi').read_bytes(), b'ORIGINAL')

    def test_invalid_headers(self):
        self.start()

        for header in (b'gemini://localhost/\n', b'gemini://localhost/\x00.gmi\r\n',
                       b'gemini://localhost/' + b'x' * 1024 + b'\r\n',
                       b'other://localhost/\r\n'):
            with self.subTest(header=header[:40]):
                self.assertTrue(self.request(header).startswith(b'59'))

    def test_nonregular_and_non_gemini_files(self):
        (self.content / 'directory.gmi').mkdir()
        os.mkfifo(self.content / 'pipe.gmi')
        (self.content / 'unsupported.txt').write_bytes(b'UNSUPPORTED')
        self.start()

        for path in ('/directory.gmi', '/pipe.gmi', '/unsupported.txt'):
            with self.subTest(path=path):
                self.assertTrue(self.request(f'gemini://localhost{path}\r\n'.encode()).startswith(b'51'))
                self.assertTrue(self.upload(path, b'BAD').startswith(b'50'))

    def test_idle_handshake_expires(self):
        self.start()

        with socket.create_connection(('127.0.0.1', DEFAULT_PORT), timeout=IDLE_CONNECTION_TIMEOUT_SECONDS):
            with self.connect(timeout_seconds=REQUEST_EXPIRY_TIMEOUT_SECONDS) as connection:
                connection.sendall(b'gemini://localhost/\r\n')
                self.assertIn(b'INDEX', self.read_response(connection))

    def test_slow_header_has_absolute_deadline(self):
        maximum_header_fragments = 14
        header_poll_interval_seconds = 0.5

        self.start()

        start_time_seconds = time.monotonic()
        closed = False

        with self.connect(timeout_seconds=REQUEST_EXPIRY_TIMEOUT_SECONDS) as connection:
            for _ in range(maximum_header_fragments):
                try:
                    connection.sendall(b'g')

                    if select.select([connection], [], [], header_poll_interval_seconds)[0]:
                        closed = connection.recv(RESPONSE_PREFIX_BYTES) == b''

                        break
                except (OSError, ssl.SSLError):
                    closed = True

                    break

        self.assertTrue(closed)
        self.assertLess(time.monotonic() - start_time_seconds, REQUEST_EXPIRY_TIMEOUT_SECONDS)
        self.assertTrue(self.request(b'gemini://localhost/\r\n').startswith(b'20'))

    def test_stalled_upload_expires_without_truncation(self):
        self.start()

        with self.connect(timeout_seconds=TRANSFER_EXPIRY_TIMEOUT_SECONDS) as connection:
            connection.sendall(b'titan://localhost/existing.gmi;mime=text/gemini;size=4\r\na')
            self.assertEqual(connection.recv(RESPONSE_PREFIX_BYTES), b'')

        self.assertEqual((self.content / 'existing.gmi').read_bytes(), b'ORIGINAL')
        self.assertTrue(self.request(b'gemini://localhost/\r\n').startswith(b'20'))

    def test_progressing_upload_outlives_request_deadline(self):
        header_pause_seconds = 3
        body_pause_seconds = 9

        self.start()

        with self.connect(timeout_seconds=REQUEST_EXPIRY_TIMEOUT_SECONDS) as connection:
            connection.sendall(b'titan://localhost/existing.gmi;mime=text/gemini;size=4')
            time.sleep(header_pause_seconds)
            connection.sendall(b'\r\n')
            time.sleep(body_pause_seconds)
            connection.sendall(b'D')
            time.sleep(body_pause_seconds)
            connection.sendall(b'ATA')
            self.assertTrue(self.read_response(connection).startswith(b'20'))

        self.assertEqual((self.content / 'existing.gmi').read_bytes(), b'DATA')

    def test_shutdown_during_handshake(self):
        self.start()

        with socket.create_connection(('127.0.0.1', DEFAULT_PORT), timeout=IDLE_CONNECTION_TIMEOUT_SECONDS):
            time.sleep(SHUTDOWN_SETTLE_SECONDS)
            self.process.send_signal(signal.SIGTERM)
            self.assertEqual(self.process.wait(timeout=PROCESS_EXIT_TIMEOUT_SECONDS), 0)

    def test_shutdown_during_upload(self):
        self.start()

        with self.connect() as connection:
            connection.sendall(b'titan://localhost/existing.gmi;mime=text/gemini;size=4\r\na')
            time.sleep(SHUTDOWN_SETTLE_SECONDS)
            self.process.send_signal(signal.SIGINT)
            self.assertEqual(self.process.wait(timeout=PROCESS_EXIT_TIMEOUT_SECONDS), 0)

        self.assertEqual((self.content / 'existing.gmi').read_bytes(), b'ORIGINAL')

    def test_large_download(self):
        body = bytes(range(256)) * 8192

        (self.content / 'large.gmi').write_bytes(body)
        self.start()
        self.assertEqual(self.request(b'gemini://localhost/large.gmi\r\n'),
                         b'20 text/gemini\r\n' + body)

    def test_stalled_download_expires(self):
        with (self.content / 'large.gmi').open('wb') as output:
            output.truncate(32 * 1024 * 1024)

        self.start()

        with self.connect(timeout_seconds=TRANSFER_EXPIRY_TIMEOUT_SECONDS) as connection:
            connection.sendall(b'gemini://localhost/large.gmi\r\n')
            time.sleep(SERVER_SETTLE_SECONDS)

            # Allow initial ciphertext buffering before the inactivity window begins.
            with self.connect(timeout_seconds=QUEUED_REQUEST_TIMEOUT_SECONDS) as second:
                second.sendall(b'gemini://localhost/\r\n')
                self.assertIn(b'INDEX', self.read_response(second))

    def test_progressing_download_outlives_request_deadline(self):
        slow_reading_duration_seconds = 16
        read_chunk_bytes = 8192
        read_interval_seconds = 0.1

        # Exceed socket buffers so paced reads exercise the server's deadline.
        body = b'X' * (32 * 1024 * 1024)
        header = b'20 text/gemini\r\n'

        (self.content / 'large.gmi').write_bytes(body)
        self.start()

        with self.connect(timeout_seconds=REQUEST_EXPIRY_TIMEOUT_SECONDS) as connection:
            connection.sendall(b'gemini://localhost/large.gmi\r\n')

            response = bytearray()
            slow_reading_deadline_seconds = time.monotonic() + slow_reading_duration_seconds

            while time.monotonic() < slow_reading_deadline_seconds:
                chunk = connection.recv(read_chunk_bytes)

                self.assertTrue(chunk)
                response.extend(chunk)
                time.sleep(read_interval_seconds)

            response.extend(self.read_response(connection))

        self.assertEqual(len(response), len(header) + len(body))
        self.assertEqual(bytes(response), header + body)

    def test_clean_shutdown(self):
        self.start()
        self.process.send_signal(signal.SIGINT)
        self.assertEqual(self.process.wait(timeout=PROCESS_EXIT_TIMEOUT_SECONDS), 0)

if __name__ == '__main__':
    unittest.main()
