#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import shutil
import sys
import urllib.request
from pathlib import Path


DATASETS = {
    "mola-463m": {
        "url": "https://planetarymaps.usgs.gov/mosaic/Mars_MGS_MOLA_DEM_mosaic_global_463m.tif",
        "product_page": "https://astrogeology.usgs.gov/search/map/mars_mgs_mola_dem_463m",
        "file_name": "mars_mgs_mola_dem_463m.tif",
    },
    "hrsc-mola-200m": {
        "url": "https://planetarymaps.usgs.gov/mosaic/Mars/HRSC_MOLA_Blend/Mars_HRSC_MOLA_BlendDEM_Global_200mp_v2.tif",
        "product_page": "https://planetarymaps.usgs.gov/mosaic/Mars/HRSC_MOLA_Blend/",
        "file_name": "mars_hrsc_mola_blend_dem_200m_v2.tif",
    },
}


def parse_args() -> argparse.Namespace:
    script_dir = Path(__file__).resolve().parent
    default_data_root = (script_dir / ".." / ".." / "grpcMMO-data").resolve()

    parser = argparse.ArgumentParser(
        description="Download an official Mars DEM into the sibling grpcMMO-data repository."
    )
    parser.add_argument(
        "--dataset",
        choices=sorted(DATASETS.keys()),
        default="mola-463m",
        help="Dataset profile to download.",
    )
    parser.add_argument(
        "--data-root",
        type=Path,
        default=default_data_root,
        help="Path to the grpcMMO-data repository root.",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Redownload the file even if it already exists.",
    )
    return parser.parse_args()


def compute_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download_file(url: str, destination: Path) -> None:
    request = urllib.request.Request(
        url,
        headers={
            "User-Agent": "grpcMMO terrain bootstrap",
        },
    )
    with urllib.request.urlopen(request) as response:
        destination.parent.mkdir(parents=True, exist_ok=True)
        with destination.open("wb") as output:
            shutil.copyfileobj(response, output, length=1024 * 1024)


def main() -> int:
    args = parse_args()
    dataset = DATASETS[args.dataset]

    data_root = args.data_root.expanduser().resolve()
    source_dir = data_root / "sources" / "mars"
    destination = source_dir / dataset["file_name"]
    hash_file = destination.with_suffix(destination.suffix + ".sha256")

    if destination.exists() and not args.force:
        print(f"Mars source already present: {destination}")
        print("Use --force to download it again.")
        return 0

    if args.force and destination.exists():
        destination.unlink()

    print(f"Downloading {args.dataset}")
    print(f"  URL: {dataset['url']}")
    print(f"  Product page: {dataset['product_page']}")
    print(f"  Destination: {destination}")

    try:
        download_file(dataset["url"], destination)
    except Exception as error:  # pragma: no cover - surface actionable error text
        print(f"Download failed: {error}", file=sys.stderr)
        return 1

    sha256 = compute_sha256(destination)
    hash_file.write_text(f"{sha256} *{dataset['file_name']}\n", encoding="utf-8")

    print("Download complete.")
    print(f"  SHA256: {sha256}")
    print(f"  Hash file: {hash_file}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
