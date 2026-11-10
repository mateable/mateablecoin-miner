# Using MateableCoin Miner

This covers how to actually run the miner once you have it (or its
release zip/tar.gz) -- for building it from source instead, see the
main [README](../README.md).

## Windows

1. **Get it.** Download `MateableCoinMiner-windows.zip` from the
   [Releases page](https://github.com/mateable/mateablecoin-miner/releases),
   unzip it anywhere, and run `MateableCoinMiner.exe`. The zip already
   includes the CPU engine (`cpuminer-sse2.exe`/`cpuminer-avx2.exe`/
   `cpuminer-avx512.exe` -- the GUI auto-detects which your CPU supports)
   and the GPU engine (`ccminer.exe`), so there's nothing else to install.

2. **Pick your hardware.** Under **Mining Device**, choose **CPU** or
   **GPU (NVIDIA)**. The app detects a real NVIDIA driver on launch and
   pre-selects GPU automatically if one's present; otherwise it starts on
   CPU. GPU mining only supports Pool mode (the GPU engine has no
   payout-address flag for solo mining -- see step 4).

3. **Pick your mode**, under **Mining Mode**:
   - **Pool** -- mine through a pool, get paid out automatically as
     blocks are found by the pool as a whole.
   - **Solo (my own node)** -- mine directly against your own
     MateableCoin node via `getblocktemplate`, no pool involved. You get
     the *entire* block reward when you find one, but only when you
     find one -- no smoothing from pooled miners. CPU only.

4. **Fill in the fields** that appear for your mode:

   **Pool mode:**
   | Field | What goes here |
   |---|---|
   | MTBC Payout Address | Your MateableCoin wallet address |
   | Worker name (pool only) | Any label, e.g. `rig1` -- shows up in the pool's stats per-worker |
   | Algorithm | One of Scrypt / YescryptR8 / Whirlpool / GhostRider / Balloon |
   | Mining Pool | zpool.ca, KriptoKyng, or GrossCrypto Pool (see table below for what each supports) |
   | Reward Mode | **Shared (PROP)** (normal pooled payout) or **Solo (via pool)** (full reward via the pool's own solo-payout port, where offered) -- only enabled when the selected pool+algo actually has a solo port |
   | Use SSL/TLS | Encrypts the connection, where the pool offers it |

   **Solo mode (CPU only):**
   | Field | What goes here |
   |---|---|
   | MTBC Payout Address | Your MateableCoin wallet address -- this is where found blocks pay out |
   | Algorithm | Same 5 options as pool mode |
   | Node RPC URL | `http://host:port` of your own node, e.g. `http://127.0.0.1:6966` |
   | RPC Username / RPC Password | Your node's `rpcuser`/`rpcpassword` (from its `.conf` file) |

   Pool/algorithm support:
   | Pool | Scrypt | YescryptR8 | Whirlpool | GhostRider | Balloon |
   |---|---|---|---|---|---|
   | zpool.ca | ✅ | ✅ | ✅ | ✅ | ✅ |
   | KriptoKyng | ✅ | ✅ | ❌ | ✅ | ❌ |
   | GrossCrypto Pool | ✅ | ✅ | ✅ | ✅ | ✅ |

5. Pick **CPU Threads** (defaults to all cores), or for GPU mode,
   optionally list specific **GPU Devices** (blank = all GPUs).

6. Press **Start Mining**. The log box shows the real, live output from
   the underlying engine -- connection status, accepted shares, and (if
   you're lucky) a found block. **Stop** ends it cleanly at any time.

## Linux

1. **Get it.** Download `MateableCoinMiner-linux.tar.gz` from the
   [Releases page](https://github.com/mateable/mateablecoin-miner/releases)
   and extract it -- this gives you `mtbc-miner.sh` plus a `build/`
   folder with the real cpuminer-opt binaries for SSE2/AVX2/AVX512 (the
   script auto-detects which your CPU supports). Make sure the script is
   executable: `chmod +x mtbc-miner.sh`.

2. **Run it.** There are three ways to use it:

   **A. Interactive wizard** (easiest) -- just run it with no arguments:
   ```
   ./mtbc-miner.sh
   ```
   If you're on a desktop with `zenity` installed, you get a GUI form.
   Otherwise it falls back to plain terminal prompts. Either way it asks
   whether you want pool or solo mining, then walks you through the same
   fields as the Windows GUI above.

   **B. Pool mining from the command line** (for scripting/automation):
   ```
   ./mtbc-miner.sh -w YOUR_MTBC_ADDRESS -a yescryptr8 -P grosscrypto -n rig1
   ```
   | Flag | Meaning | Default |
   |---|---|---|
   | `-w, --wallet` | Your MTBC wallet address (required) | -- |
   | `-a, --algo` | `yescryptr8` / `ghostrider` / `balloon` / `scrypt` / `whirlpool` | `yescryptr8` |
   | `-P, --pool` | `zpool` / `kriptokyng` / `grosscrypto` | `zpool` |
   | `-n, --worker` | Worker name | `worker1` |
   | `-s, --ssl` | Use SSL/TLS (zpool only) | off |
   | `-t, --threads` | CPU threads | all cores |

   **C. Solo mining from the command line** (direct to your own node):
   ```
   ./mtbc-miner.sh --solo -w YOUR_MTBC_ADDRESS \
     --node-url http://127.0.0.1:6966 --rpc-user myrpcuser --rpc-pass myrpcpass
   ```
   `--node-url`, `--rpc-user`, and `--rpc-pass` are all required in solo
   mode; `-w`/`--wallet` is reused as the payout address.

3. The script `exec`s straight into the real miner binary, so Ctrl+C
   stops it immediately and you see its real, live output the whole
   time -- nothing is buffered or hidden.

## Notes that apply to both platforms

- **No account or sign-up needed anywhere** -- every pool here is
  address-based: your wallet address *is* your identity to the pool.
- **Solo mining** only pays out when *you personally* find a block --
  worth it if you're running serious hardware or just want 100% of
  what you find, but pool mining gives steadier, smaller payouts if
  that's what you'd rather have.
- Get a receiving address first if you don't have one: open your
  MateableCoin wallet -> **File -> Receiving Addresses -> New**.
