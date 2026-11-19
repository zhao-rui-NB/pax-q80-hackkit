import tempfile
import stat
import unittest
from pathlib import Path
from unittest.mock import patch
import zipfile

from q80 import aip, app_manifest


class AppPackageTest(unittest.TestCase):
    def test_manifest_creates_appinfo_and_pack(self):
        with tempfile.TemporaryDirectory() as temp, patch.object(app_manifest, "ROOT", Path(temp)):
            root = Path(temp)
            app_manifest.create("hello")
            app = app_manifest.load("hello")
            app.output.parent.mkdir(parents=True)
            app.output.write_bytes(b"ELF test")
            package = aip.pack_app(app, root / "hello.aip")
            with zipfile.ZipFile(package) as archive:
                self.assertEqual(archive.namelist(), ["appinfo", "libhello.so"])
                self.assertIn(b"id=hello", archive.read("appinfo"))
                self.assertEqual(archive.read("libhello.so"), b"ELF test")

    def test_invalid_app_id(self):
        with tempfile.TemporaryDirectory() as temp, patch.object(app_manifest, "ROOT", Path(temp)):
            app_manifest.create("hello")
            (Path(temp) / "device/apps/hello/app.toml").write_text('id = "MAINAPP"')
            with self.assertRaises(ValueError):
                app_manifest.load("hello")

    def test_root_loader_aip_symlink_precedes_payload(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            loader = root / "loader.so"
            loader.write_bytes(b"test loader")
            path = aip.pack_root_loader(loader, root / "loader.aip", "q80_test")
            with zipfile.ZipFile(path) as archive:
                self.assertEqual(archive.namelist(), [
                    "appinfo", "manifest.xml", "manifest.xml/libosal.so"
                ])
                link = archive.getinfo("manifest.xml")
                self.assertTrue(stat.S_ISLNK(link.external_attr >> 16))
                self.assertEqual(archive.read("manifest.xml"), b"/usr/lib")

    def test_busybox_aip_links_bin_before_binary(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            binary = root / "busybox"
            binary.write_bytes(b"test busybox")
            path = aip.pack_busybox(binary, root / "busybox.aip", "q80_bb_test")
            with zipfile.ZipFile(path) as archive:
                self.assertEqual(archive.namelist(), [
                    "appinfo", "probe", "probe/busybox", "probe/sh"
                ])
                self.assertTrue(stat.S_ISLNK(archive.getinfo("probe").external_attr >> 16))
                self.assertEqual(archive.read("probe"), b"/bin")
