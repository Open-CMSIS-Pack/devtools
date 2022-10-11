#!/usr/bin/python3
import os
import subprocess
import sys
import unittest
import xml.etree.ElementTree as ET

from argparse import ArgumentParser, Action
from datetime import datetime, timedelta
from pathlib import Path
from urllib.parse import urlparse, urljoin
from xmlrunner import XMLTestRunner
from zipfile import ZipFile

from requests import get as curl
from requests.exceptions import Timeout as CurlTimeout, HTTPError as CurlError, ConnectionError as CurlConnectionError
from semver import VersionInfo


OFFILE_MODE = False
SYNTAX_MODE = None
DELTA_MODE = None
SVDCONV_BIN = Path('./svdconv')

class TestSequence(unittest.TestCase):

    def prepare_svd(self, pdsc, svd):
        packfile = pdsc.download_pack()
        self.assertTrue(packfile.exists())

        svdpath = f".packs/.svd/{pdsc.vendor}.{pdsc.name}.{pdsc.version}"
        svdfile = Path(f"{svdpath}/{Path(svd).as_posix()}").resolve()
        if not svdfile.exists():
            with ZipFile(packfile, 'r') as archive:
                archive.extract(Path(svd).as_posix(), path=svdpath)

        self.assertTrue(svdfile.exists())

        return svdfile

    @staticmethod
    def run_cmd(cmd):
        print("\n"+" ".join(str(c) for c in cmd))
        result = subprocess.run(cmd, check=False, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        print(result.stdout.decode('utf-8'))
        print(f" => {result.returncode}")
        return result

    @staticmethod
    def generate_test_for(pdsc, svd):
        def test_header(self):
            global SVDCONV_BIN, SYNTAX_MODE, DELTA_MODE

            svdfile = self.prepare_svd(pdsc, svd)

            outdir = f"{pdsc.vendor}.{pdsc.name}.{pdsc.version}"
            svdxml = ET.parse(svdfile).getroot()
            devicename = svdxml.findtext("./name")

            outpath = Path(f"out/{outdir}/").resolve()
            outpath.mkdir(parents=True, exist_ok=True)

            result = TestSequence.run_cmd(
                [SVDCONV_BIN.as_posix(), svdfile, "--generate=header", "--fields=struct", "--fields=macro",
                "--fields=enum", "--create-folder", "-o", outpath.as_posix(), "-b",
                f"{outpath.as_posix()}/{Path(svd).stem}.log"])
            with self.subTest(msg=result.args[0]):
                self.assertLessEqual(result.returncode, 1)

            if SYNTAX_MODE:
                self.assertIsNotNone(svdxml.find("./cpu"))
                Path(f"{outpath.as_posix()}/system_{devicename}.h").touch()
                result = TestSequence.run_cmd(
                    [SYNTAX_MODE.as_posix(), "--target=arm-arm-none-eabi",
                    f"-I{os.environ.get('CMSIS_PACK_ROOT', '.packs')}/ARM/CMSIS/5.9.0/CMSIS/Core/Include",
                    "-fsyntax-only", f"{outpath.as_posix()}/{devicename}.h"])
                with self.subTest(msg=result.args[0]):
                    self.assertEqual(result.returncode, 0)

            if DELTA_MODE:
                out2path = Path(f"out2/{outdir}/").resolve()
                out2path.mkdir(parents=True, exist_ok=True)
                result = TestSequence.run_cmd(
                    [DELTA_MODE.as_posix(), svdfile, "--generate=header", "--fields=struct", "--fields=macro",
                     "--fields=enum", "--create-folder", "-o", out2path.as_posix(), "-b",
                     f"{out2path.as_posix()}/{Path(svd).stem}.log"])
                with self.subTest(msg=result.args[0]):
                    self.assertLessEqual(result.returncode, 1)

        def test_sfd(self):
            svdfile = self.prepare_svd(pdsc, svd)

            outdir = f"{pdsc.vendor}.{pdsc.name}.{pdsc.version}"
            svdxml = ET.parse(svdfile).getroot()
            devicename = svdxml.findtext("./name")

            outpath = Path(f"out/{outdir}/").resolve()
            outpath.mkdir(parents=True, exist_ok=True)

            result = TestSequence.run_cmd(
                [SVDCONV_BIN.as_posix(), svdfile.as_posix(), "--generate=sfd", "--create-folder",
                "-o", outpath.as_posix(), "-b", f"{outpath.as_posix()}/{devicename}.log"])
            with self.subTest(msg=result.args[0]):
                self.assertLessEqual(result.returncode, 1)

            if DELTA_MODE:
                out2path = Path(f"out2/{outdir}/").resolve()
                out2path.mkdir(parents=True, exist_ok=True)

                result = TestSequence.run_cmd(
                    [DELTA_MODE.as_posix(), svdfile.as_posix(), "--generate=sfd", "--create-folder",
                    "-o", out2path.as_posix(), "-b", f"{out2path.as_posix()}/{devicename}.log"])
                with self.subTest(msg=result.args[0]):
                    self.assertLessEqual(result.returncode, 1)

                result = TestSequence.run_cmd(
                    ["bash", "-c", f"diff -dwB"
                    f" <(tail -n +$(grep -n '*/' {outpath.as_posix()}/{devicename}.sfd | head -n 1 | cut -d: -f1) {outpath.as_posix()}/{devicename}.sfd)"
                    f" <(tail -n +$(grep -n '*/' {out2path.as_posix()}/{devicename}.sfd | head -n 1 | cut -d: -f1) {out2path.as_posix()}/{devicename}.sfd)"])
                with self.subTest(msg=result.args[0]):
                    self.assertEqual(result.returncode, 0)

        test_name = f"test_{pdsc.vendor.replace('-','_')}_{pdsc.name.replace('-','_')}_{Path(svd).stem.replace('-','_')}"
        setattr(TestSequence, test_name+"_header", test_header)
        setattr(TestSequence, test_name+"_sfd", test_sfd)


def download(url, file, timeout=60):
    global OFFLINE_MODE
    if OFFLINE_MODE:
        return
    print(f"Downloading '{url}' ...")
    try:
        request = curl(url, allow_redirects=True, timeout=timeout)
        request.raise_for_status()
        file.parent.mkdir(parents=True, exist_ok=True)
        with open(file, 'wb') as target:
            target.write(request.content)
    except CurlTimeout as ex:
        print(f"Timeout downloading '{url}'!")
        raise RuntimeError from ex
    except CurlError as ex:
        print(f"Failed downloading '{url}'!")
        raise RuntimeError from ex
    except CurlConnectionError as ex:
        print(f"Error connecting to '{url}'!")
        raise RuntimeError from ex


class PackDesc:
    class Release:
        def __init__(self, element):
            self._element = element

        def version(self):
            try:
                return VersionInfo.parse(self._element.get("version"))
            except ValueError:
                return VersionInfo(0)

    def __init__(self, file, url=None, index=None):
        self._file = Path(file)
        self._url = url
        self._index = index
        self._pdsc = None
        self._releases = None

    def __str__(self):
        return self._file.name

    @property
    def pdsc(self):
        if not self._pdsc:
            self._pdsc = ET.parse(self._file).getroot()
        return self._pdsc

    @property
    def vendor(self):
        return self.pdsc.findtext("./vendor")

    @property
    def name(self):
        return self.pdsc.findtext("./name")

    @property
    def version(self):
        if not self._releases:
            self._releases = sorted((PackDesc.Release(e) for e in self.pdsc.findall(".//releases/release")),
                                    key=PackDesc.Release.version, reverse=True)
        return self._releases[0].version()

    @property
    def url(self):
        if not self._url:
            self._url = urlparse(self.pdsc.findtext("./url"))
        return self._url

    @property
    def svds(self):
        debug = self.pdsc.findall(".//devices//debug[@svd]")
        return (d.get("svd") for d in debug)

    def exists(self):
        return self._file.exists()

    def update(self):
        self._pdsc = None
        self._releases = None
        if self._index:
            try:
                download(urljoin(self._index.url, self._file.name), self._file)
                return
            except RuntimeError:
                pass
        try:
            download(urljoin(self.url, self._file.name), self._file)
        except RuntimeError:
            pass

    def download_pack(self, version=None):
        version = version or self.version
        packfile = f"{self.vendor}.{self.name}.{version}.pack"
        dlfile = self._index.dlfolder.joinpath(packfile)
        if not dlfile.exists():
            download(urljoin(self.url, packfile), dlfile)
        return dlfile


class PackIndex:
    def __init__(self, url, folder=".packs"):
        self._folder = folder
        self._url = url
        self._file = Path(f"{folder}/.Web/index.pidx")
        self._skipfile = Path(f"{folder}/skiplist.pidx")
        self._pidx = None

    @property
    def folder(self):
        return self._folder

    @property
    def dlfolder(self):
        return Path(self._folder).joinpath('.Download')

    @property
    def url(self):
        return self._url

    @property
    def pidx(self):
        if not self._pidx and self._file.exists():
            self._pidx = ET.parse(self._file).getroot()
        return self._pidx

    @property
    def pdscs(self):
        skiplist = []
        if self._skipfile.exists():
            skiplist = ET.parse(self._skipfile).getroot()
            skiplist = [(s.get("vendor"), s.get("name")) for s in skiplist.findall(".//pdsc")]
        for pdsc in self.pidx.findall(".//pdsc"):
            vendor = pdsc.get("vendor")
            name = pdsc.get("name")
            version = pdsc.get("version")
            try:
                version = VersionInfo.parse(version)
            except ValueError:
                print(f"{vendor}.{name}.pdsc has invalid SemVer '{version}'")
                continue
            url = pdsc.get("url")
            cached = PackDesc(f"{self._folder}/.Web/{vendor}.{name}.pdsc", url, self)
            if not (vendor, name) in skiplist:
                yield cached, version

    def update(self, outdated=timedelta(hours=12)):
        try:
            mtime = datetime.min
            if self._file.exists():
                mtime = datetime.fromtimestamp(self._file.stat().st_mtime)
            now = datetime.now()
            if now - mtime > outdated:
                download(self._url, self._file)
        except RuntimeError:
            return

        for pdsc, latest in self.pdscs:
            if not pdsc.exists() or pdsc.version < latest:
                pdsc.update()


class AbsolutePathAction(Action):
    def __call__(self, parser, namespace, values, option_string=None):
        if values and isinstance(values, Path):
            values = values.resolve()
        setattr(namespace, self.dest, values)


def main():
    global OFFLINE_MODE, SYNTAX_MODE, DELTA_MODE, SVDCONV_BIN

    parser = ArgumentParser(add_help=False)
    parser.add_argument('--offline', action='store_true')
    parser.add_argument('--syntax', type=Path, default=None, action=AbsolutePathAction,
        help="Compiler to be used for header file syntax check")
    parser.add_argument('--delta', type=Path, default=None, action=AbsolutePathAction,
        help="Reference version of SVDConv for delta check")
    parser.add_argument('--svdconv', type=Path, default=Path('./svdconv'), action=AbsolutePathAction,
        help="SVDConv binary to be tested")
    args, argv = parser.parse_known_args()

    OFFLINE_MODE = args.offline
    SYNTAX_MODE = args.syntax
    DELTA_MODE = args.delta
    SVDCONV_BIN = args.svdconv

    pidx = PackIndex("https://www.keil.com/pack/index.pidx")
    pidx.update()

    for pdsc, _ in pidx.pdscs:
        if pdsc.exists():
            for svd in pdsc.svds:
                TestSequence.generate_test_for(pdsc, svd)

    unittest.main(
        argv=[sys.argv[0]]+argv,
        testRunner=XMLTestRunner(output='.'))


if __name__ == '__main__':
    main()
