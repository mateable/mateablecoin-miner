#!/usr/bin/env bash
# MateableCoin Miner -- Linux launcher for the real cpuminer-opt binaries
# (tpfuemp/cpuminer-opt, GPLv2 -- see build/cpuminer-opt-LICENSE.txt /
# build/cpuminer-opt-COPYING.txt shipped next to this script).
#
# This script does not mine anything itself -- it builds the right command
# line from what you give it (via a desktop wizard if zenity is available,
# interactive prompts in a terminal, or plain flags for scripting), picks the
# best-matching bundled cpuminer-opt build for this CPU, and execs it so you
# see the real, live miner output directly.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

ALGOS=(yescryptr8 ghostrider balloon scrypt whirlpool)
POOLS=(zpool kriptokyng grosscrypto)

# Same pool/algo/port table as windows/mtbc_miner_gui.c -- keep both in sync.
# zpool.ca: all 5 algos. c=MTBC,zap=MTBC is required in the miner password to
# actually get paid in real MTBC (zpool disables payout auto-conversion for
# MTBC specifically, confirmed by zpool's own live system message).
declare -A ZPOOL_TCP=(
    [yescryptr8]="stratum+tcp://yescryptr8.mine.zpool.ca:6323"
    [ghostrider]="stratum+tcp://ghostrider.mine.zpool.ca:5354"
    [balloon]="stratum+tcp://balloon.mine.zpool.ca:5100"
    [scrypt]="stratum+tcp://scrypt.mine.zpool.ca:3433"
    [whirlpool]="stratum+tcp://whirlpool.mine.zpool.ca:4133"
)
declare -A ZPOOL_SSL=(
    [yescryptr8]="stratum+ssl://yescryptr8.mine.zpool.ca:56323"
    [ghostrider]="stratum+ssl://ghostrider.mine.zpool.ca:55354"
    [balloon]="stratum+ssl://balloon.mine.zpool.ca:55100"
    [scrypt]="stratum+ssl://scrypt.mine.zpool.ca:53433"
    [whirlpool]="stratum+ssl://whirlpool.mine.zpool.ca:54133"
)
ZPOOL_PASS="c=MTBC,zap=MTBC"

# KriptoKyng: real MiningCore pool. Only yescryptr8/ghostrider/scrypt --
# no balloon/whirlpool pool on their side. No SSL offered on any port.
declare -A KRIPTOKYNG_TCP=(
    [yescryptr8]="stratum+tcp://stratum.kriptokyng.com:2683"
    [ghostrider]="stratum+tcp://stratum.kriptokyng.com:2688"
    [scrypt]="stratum+tcp://stratum.kriptokyng.com:2677"
)
KRIPTOKYNG_PASS="x"

# GrossCrypto Pool: our own MiningCore-based pool, live and public at
# pool.grosscrypto.com. All 5 algos. No SSL on the stratum ports (plain TCP
# passthrough through HAProxy).
declare -A GROSSCRYPTO_TCP=(
    [yescryptr8]="stratum+tcp://pool.grosscrypto.com:3262"
    [ghostrider]="stratum+tcp://pool.grosscrypto.com:3263"
    [balloon]="stratum+tcp://pool.grosscrypto.com:3258"
    [scrypt]="stratum+tcp://pool.grosscrypto.com:3261"
    [whirlpool]="stratum+tcp://pool.grosscrypto.com:3259"
)
GROSSCRYPTO_PASS="x"

# ---- Pick the best-matching bundled cpuminer-opt build for this CPU ----
pick_binary() {
    local flags
    flags="$(grep -m1 '^flags' /proc/cpuinfo || true)"
    if echo "$flags" | grep -qw avx512f; then
        echo "$BUILD_DIR/cpuminer-avx512"
    elif echo "$flags" | grep -qw avx2; then
        echo "$BUILD_DIR/cpuminer-avx2"
    else
        echo "$BUILD_DIR/cpuminer-sse2"
    fi
}

usage() {
    cat <<EOF
MateableCoin Miner (Linux)

Usage:
  $0                                   Interactive wizard (zenity GUI if available, else terminal prompts)
  $0 -w WALLET [options]                Non-interactive pool mining, for scripting
  $0 --solo -w ADDR --node-url URL ...  Non-interactive solo mining against your own node

Pool mining options:
  -w, --wallet WALLET    Your MTBC wallet address (required for non-interactive use)
  -n, --worker NAME      Worker name (default: worker1)
  -a, --algo ALGO        One of: ${ALGOS[*]} (default: yescryptr8)
  -P, --pool POOL        One of: ${POOLS[*]} (default: zpool)
  -s, --ssl              Use SSL/TLS (stratum+ssl://) instead of plain stratum+tcp:// (zpool only)

Solo mining options (direct getblocktemplate RPC to your own node, no pool, full block reward):
  --solo                 Enable solo mode instead of pool mining
  -w, --wallet ADDR      Your MTBC payout address (same flag as pool mode)
  --node-url URL         Node RPC URL, e.g. http://127.0.0.1:6966 (required for --solo)
  --rpc-user USER        Node RPC username (required for --solo)
  --rpc-pass PASS        Node RPC password (required for --solo)

  -t, --threads N        Number of CPU threads (default: all cores)
  -h, --help             Show this help
EOF
}

WALLET=""
WORKER="worker1"
ALGO="yescryptr8"
POOL="zpool"
USE_SSL=0
SOLO=0
NODE_URL=""
RPC_USER=""
RPC_PASS=""
THREADS=""
ARGS_GIVEN=0

while [ $# -gt 0 ]; do
    ARGS_GIVEN=1
    case "$1" in
        -w|--wallet) WALLET="$2"; shift 2 ;;
        -n|--worker) WORKER="$2"; shift 2 ;;
        -a|--algo) ALGO="$2"; shift 2 ;;
        -P|--pool) POOL="$2"; shift 2 ;;
        -s|--ssl) USE_SSL=1; shift ;;
        -t|--threads) THREADS="$2"; shift 2 ;;
        --solo) SOLO=1; shift ;;
        --node-url) NODE_URL="$2"; shift 2 ;;
        --rpc-user) RPC_USER="$2"; shift 2 ;;
        --rpc-pass) RPC_PASS="$2"; shift 2 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

is_valid_algo() {
    local a="$1"
    for x in "${ALGOS[@]}"; do [ "$x" = "$a" ] && return 0; done
    return 1
}

is_valid_pool() {
    local p="$1"
    for x in "${POOLS[@]}"; do [ "$x" = "$p" ] && return 0; done
    return 1
}

# ---- Collect missing input ----
if [ $ARGS_GIVEN -eq 0 ]; then
    if command -v zenity >/dev/null 2>&1 && [ -n "${DISPLAY:-}" ]; then
        MODE_CHOICE="$(zenity --list --radiolist --title="MateableCoin Miner" \
            --text="Pool mining or solo mining?" --column="" --column="Mode" \
            TRUE "Pool mining" FALSE "Solo mining (my own node)" 2>/dev/null)" \
            || { echo "Cancelled."; exit 1; }
        [ "$MODE_CHOICE" = "Solo mining (my own node)" ] && SOLO=1

        if [ "$SOLO" = "1" ]; then
            FORM_OUT="$(zenity --forms \
                --title="MateableCoin Miner -- Solo" \
                --text="Enter your node details" \
                --add-entry="MTBC Payout Address" \
                --add-entry="Node RPC URL (e.g. http://127.0.0.1:6966)" \
                --add-entry="RPC Username" \
                --add-entry="RPC Password" \
                --add-combo="Algorithm" --combo-values="${ALGOS[*]// /|}" \
                --separator="|" 2>/dev/null)" || { echo "Cancelled."; exit 1; }
            IFS='|' read -r WALLET NODE_URL RPC_USER RPC_PASS ALGO <<< "$FORM_OUT"
            if [ -z "$WALLET" ] || [ -z "$NODE_URL" ]; then
                zenity --error --text="Payout address and node RPC URL are required." 2>/dev/null
                exit 1
            fi
        else
            FORM_OUT="$(zenity --forms \
                --title="MateableCoin Miner" \
                --text="Enter your mining details" \
                --add-entry="MTBC Wallet Address" \
                --add-entry="Worker name (optional)" \
                --add-combo="Algorithm" --combo-values="${ALGOS[*]// /|}" \
                --add-combo="Pool" --combo-values="${POOLS[*]// /|}" \
                --add-combo="Use SSL/TLS?" --combo-values="No|Yes" \
                --separator="|" 2>/dev/null)" || { echo "Cancelled."; exit 1; }
            IFS='|' read -r WALLET WORKER ALGO POOL SSL_CHOICE <<< "$FORM_OUT"
            [ -z "$WORKER" ] && WORKER="worker1"
            [ "$SSL_CHOICE" = "Yes" ] && USE_SSL=1
            if [ -z "$WALLET" ]; then
                zenity --error --text="Enter your MTBC wallet address first." 2>/dev/null
                exit 1
            fi
        fi
    else
        echo "=== MateableCoin Miner ==="
        read -rp "Solo mine against your own node instead of a pool? [y/N]: " SOLO_ANSWER
        [[ "$SOLO_ANSWER" =~ ^[Yy] ]] && SOLO=1

        if [ "$SOLO" = "1" ]; then
            read -rp "MTBC payout address: " WALLET
            read -rp "Node RPC URL [http://127.0.0.1:6966]: " NODE_URL
            [ -z "$NODE_URL" ] && NODE_URL="http://127.0.0.1:6966"
            read -rp "RPC username: " RPC_USER
            read -rsp "RPC password: " RPC_PASS; echo
            echo "Algorithm options: ${ALGOS[*]}"
            read -rp "Algorithm [yescryptr8]: " ALGO
            [ -z "$ALGO" ] && ALGO="yescryptr8"
            if [ -z "$WALLET" ]; then
                echo "Enter your MTBC payout address first." >&2
                exit 1
            fi
        else
            read -rp "MTBC wallet address: " WALLET
            if [ -z "$WALLET" ]; then
                echo "Enter your MTBC wallet address first." >&2
                exit 1
            fi
            read -rp "Worker name [worker1]: " WORKER
            [ -z "$WORKER" ] && WORKER="worker1"
            echo "Algorithm options: ${ALGOS[*]}"
            read -rp "Algorithm [yescryptr8]: " ALGO
            [ -z "$ALGO" ] && ALGO="yescryptr8"
            echo "Pool options: ${POOLS[*]}"
            read -rp "Pool [zpool]: " POOL
            [ -z "$POOL" ] && POOL="zpool"
            read -rp "Use SSL/TLS? (zpool only) [y/N]: " SSL_ANSWER
            [[ "$SSL_ANSWER" =~ ^[Yy] ]] && USE_SSL=1
        fi
    fi
fi

if [ -z "$WALLET" ]; then
    echo "ERROR: no wallet/payout address given. Use -w ADDR or run with no args for the wizard." >&2
    usage
    exit 1
fi

if ! is_valid_algo "$ALGO"; then
    echo "ERROR: unknown algorithm '$ALGO'. Valid: ${ALGOS[*]}" >&2
    exit 1
fi

MINER="$(pick_binary)"
if [ ! -x "$MINER" ]; then
    echo "ERROR: expected miner binary not found or not executable: $MINER" >&2
    exit 1
fi

THREADS_FLAG=()
[ -n "$THREADS" ] && THREADS_FLAG=(-t "$THREADS")

if [ "$SOLO" = "1" ]; then
    if [ -z "$NODE_URL" ] || [ -z "$RPC_USER" ] || [ -z "$RPC_PASS" ]; then
        echo "ERROR: --solo requires --node-url, --rpc-user, and --rpc-pass." >&2
        usage
        exit 1
    fi
    echo "--- Starting SOLO ($ALGO, CPU build: $(basename "$MINER")) ---"
    echo "\"$MINER\" -a $ALGO -o $NODE_URL -u $RPC_USER -p <hidden> --coinbase-addr=$WALLET ${THREADS_FLAG[*]:-}"
    echo
    exec "$MINER" -a "$ALGO" -o "$NODE_URL" -u "$RPC_USER" -p "$RPC_PASS" --coinbase-addr="$WALLET" "${THREADS_FLAG[@]}"
fi

if ! is_valid_pool "$POOL"; then
    echo "ERROR: unknown pool '$POOL'. Valid: ${POOLS[*]}" >&2
    exit 1
fi

case "$POOL" in
    zpool)
        if [ "$USE_SSL" = "1" ]; then POOL_URL="${ZPOOL_SSL[$ALGO]:-}"; else POOL_URL="${ZPOOL_TCP[$ALGO]:-}"; fi
        POOL_PASS="$ZPOOL_PASS"
        ;;
    kriptokyng)
        [ "$USE_SSL" = "1" ] && echo "Note: KriptoKyng has no SSL ports, using plain TCP." >&2
        POOL_URL="${KRIPTOKYNG_TCP[$ALGO]:-}"
        POOL_PASS="$KRIPTOKYNG_PASS"
        ;;
    grosscrypto)
        [ "$USE_SSL" = "1" ] && echo "Note: GrossCrypto Pool has no SSL ports, using plain TCP." >&2
        POOL_URL="${GROSSCRYPTO_TCP[$ALGO]:-}"
        POOL_PASS="$GROSSCRYPTO_PASS"
        ;;
esac

if [ -z "$POOL_URL" ]; then
    echo "ERROR: $POOL doesn't offer a pool for algorithm '$ALGO'. Try a different pool or algorithm." >&2
    exit 1
fi

echo "--- Starting ($ALGO on $POOL, CPU build: $(basename "$MINER")) ---"
echo "\"$MINER\" -a $ALGO -o $POOL_URL -u $WALLET.$WORKER -p $POOL_PASS ${THREADS_FLAG[*]:-}"
echo

# exec replaces this shell with the miner -- Ctrl+C stops it directly and its
# real stdout/stderr streams straight to this terminal, unbuffered.
exec "$MINER" -a "$ALGO" -o "$POOL_URL" -u "$WALLET.$WORKER" -p "$POOL_PASS" "${THREADS_FLAG[@]}"
