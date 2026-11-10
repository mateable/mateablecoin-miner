<h1 align="center">
MateableCoin Miner
<br/><br/>
<img src="https://coin.mateable.com/assets/img/MTBC_Logo.png" alt="mateablecoin" width="200"/>
</h1>

<div align="center">

[![mateablecoinBadge](https://img.shields.io/badge/Mateable-coin-blueviolet.svg)](https://coin.mateable.com) [![mateablecoinBadge](https://img.shields.io/badge/MateableCoin-MTBC-blueviolet.svg)](https://coin.mateable.com) [![mateablecoinBadge](https://img.shields.io/badge/License-GPLv3-blueviolet.svg)](LICENSE)

[![mateablecoinBadge](https://img.shields.io/badge/Windows-blueviolet.svg)](windows/) [![mateablecoinBadge](https://img.shields.io/badge/Linux-blueviolet.svg)](linux/) [![mateablecoinBadge](https://img.shields.io/badge/CPU-blueviolet.svg)](#) [![mateablecoinBadge](https://img.shields.io/badge/GPU%20(NVIDIA)-blueviolet.svg)](#)

[![Build MateableCoin Miner](https://github.com/mateable/mateablecoin-miner/actions/workflows/build.yml/badge.svg)](https://github.com/mateable/mateablecoin-miner/actions/workflows/build.yml)

</div>

A native GUI (Windows) and script (Linux) for mining MateableCoin (MTBC).
Wraps the real mining engines -- this repo does not implement any hashing
itself:

- **CPU**: [cpuminer-opt](https://github.com/tpfuemp/cpuminer-opt) by tpfuemp (GPLv2)
- **GPU (NVIDIA)**: [ccminer-tpfuemp](https://github.com/tpfuemp/ccminer-tpfuemp) by tpfuemp (GPLv3)

Supports pool mining (zpool.ca, KriptoKyng, and our own official
[GrossCrypto Pool](https://pool.grosscrypto.com) -- plain or SSL where the
pool offers it) and solo mining straight to your own MateableCoin node (CPU
only -- the GPU engine has no payout-address flag for solo mining).

**Website:** [coin.mateable.com](https://coin.mateable.com) &nbsp;|&nbsp; **Pool:** [pool.grosscrypto.com](https://pool.grosscrypto.com)

## Usage 💻

See [doc/USAGE.md](doc/USAGE.md) for a full walkthrough of every field and
flag, on both Windows and Linux -- pool mining, solo mining, and what each
supported pool offers per algorithm.

## Repo layout 🗂

```
windows/   MateableCoinMiner.exe source (C, Win32 API) + icon + resource script
linux/     mtbc-miner.sh (zenity GUI wizard / terminal prompts / CLI flags)
           + .desktop launcher + icon
```

Neither directory contains the mining engine binaries themselves -- those
are real third-party releases, fetched fresh by CI (see below) rather than
committed here.

## Building 🛠

You don't need to build this by hand -- see **Releases** for ready-to-run
downloads, produced automatically (below). To build locally anyway:

**Windows GUI** (cross-compiled from Linux with mingw-w64):
```
cd windows
x86_64-w64-mingw32-windres mtbc_miner.rc -O coff -o mtbc_miner_res.o
x86_64-w64-mingw32-gcc -municode -mwindows -O2 \
  -o MateableCoinMiner.exe mtbc_miner_gui.c mtbc_miner_res.o -lcomctl32
```
Then grab `cpuminer-sse2.exe`/`cpuminer-avx2.exe`/`cpuminer-avx512.exe` from
cpuminer-opt's own releases and `ccminer.exe` from ccminer-tpfuemp's, and
drop them next to the exe.

**Linux**: `linux/mtbc-miner.sh` runs as-is; it expects
`cpuminer-sse2`/`cpuminer-avx2`/`cpuminer-avx512` in a `build/` folder beside
it (same binaries, Linux build).

## CI/CD -- how releases get made 🚀

`.github/workflows/build.yml` runs on every push/PR (just a fast compile
check, no downloads) and does a full package build when you push a version
tag like `v1.0`:

1. Compiles `MateableCoinMiner.exe` fresh from source.
2. Fetches the *latest* real cpuminer-opt and ccminer-tpfuemp releases
   directly from their GitHub release APIs (not pinned/vendored -- always
   current).
3. Bundles everything plus each engine's real license file.
4. Zips/tars it and attaches it to the GitHub Release for that tag.

**To cut a new release:**
```
git tag v1.1
git push origin v1.1
```
Then check the Actions tab -- when it finishes, `MateableCoinMiner-windows.zip`
and `MateableCoinMiner-linux.tar.gz` show up attached to the v1.1 release,
built from scratch, no manual steps.

You can also trigger a full build without tagging anything via the Actions
tab -> "Build MateableCoin Miner" -> "Run workflow" (useful for testing the
packaging steps before cutting a real release).

## Communities 🚀🍾

* [MateableCoin subreddit](https://www.reddit.com/r/mateable/)
* [Discord](https://discord.com/invite/9TF5QkTwFA)
* [MateableCoin Twitter](https://twitter.com/mateable)

## License ⚖️

This wrapper (the GUI source, the Linux script, build/packaging) is
licensed under the [GNU GPLv3](LICENSE) -- compatible with both bundled
engines (cpuminer-opt is GPLv2, ccminer-tpfuemp is GPLv3). Each engine
keeps its own original license, bundled alongside it in every release.
