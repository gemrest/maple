import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

class BuildTests(unittest.TestCase):
    def test_header_changes_rebuild_dependants(self):
        test_directory = pathlib.Path(__file__).resolve().parent
        repository = test_directory.parent

        with tempfile.TemporaryDirectory(prefix='maple-build-') as directory:
            workspace = pathlib.Path(directory)

            shutil.copytree(repository / 'maple', workspace / 'maple')
            shutil.copyfile(repository / 'build.ninja', workspace / 'build.ninja')
            subprocess.run(['ninja'], cwd=workspace, check=True, capture_output=True)

            build_directory = workspace / 'build'
            outputs = list(build_directory.glob('*.o')) + [build_directory / 'maple']
            modification_times_nanoseconds = [output.stat().st_mtime_ns for output in outputs]

            subprocess.run(['ninja'], cwd=workspace, check=True, capture_output=True)
            self.assertEqual([output.stat().st_mtime_ns for output in outputs], modification_times_nanoseconds)
            os.utime(workspace / 'maple/content.hh', None)

            rebuild = subprocess.check_output(['ninja'], cwd=workspace, text=True)

            self.assertIn('maple/content.cc', rebuild)
            self.assertIn('maple/gemini.cc', rebuild)
            self.assertNotIn('maple/titan.cc', rebuild)

if __name__ == '__main__':
    unittest.main()
