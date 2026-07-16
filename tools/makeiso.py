#!/usr/bin/env python3
import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def parse_args():
	parser = argparse.ArgumentParser(
		description="build otsos iso"
	)
	parser.add_argument("--output", required=True)
	parser.add_argument("--bios-image", required=True)
	parser.add_argument("--efi-image", required=True)
	parser.add_argument("--xorriso", default="xorriso")
	return parser.parse_args()


def require_file(path, name):
	if not path.is_file():
		print(f"makeiso: missing {name}: {path}", file=sys.stderr)
		return False
	return True


def main():
	args = parse_args()
	output = Path(args.output)
	bios_image = Path(args.bios_image)
	efi_image = Path(args.efi_image)

	if not require_file(bios_image, "BIOS image"):
		return 1
	if not require_file(efi_image, "EFI image"):
		return 1

	output.parent.mkdir(parents=True, exist_ok=True)
	with tempfile.TemporaryDirectory(prefix="otsos-iso-") as tmp:
		root = Path(tmp)
		boot_dir = root / "boot"
		efi_dir = root / "EFI"
		boot_dir.mkdir()
		efi_dir.mkdir()

		shutil.copy2(bios_image, boot_dir / "otsos-bios.img")
		shutil.copy2(efi_image, efi_dir / "efiboot.img")

		cmd = [
			args.xorriso,
			"-as",
			"mkisofs",
			"-quiet",
			"-o",
			str(output),
			"-b",
			"boot/otsos-bios.img",
			"-c",
			"boot/boot.cat",
			"-hard-disk-boot",
			"-eltorito-alt-boot",
			"-e",
			"EFI/efiboot.img",
			"-no-emul-boot",
			str(root),
		]
		subprocess.run(cmd, check=True)
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
