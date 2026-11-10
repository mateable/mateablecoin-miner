// MateableCoin Miner -- a simple native Windows GUI wrapper around the real
// cpuminer-opt binaries (tpfuemp/cpuminer-opt, GPLv2 -- see
// cpuminer-opt-LICENSE.txt / cpuminer-opt-COPYING.txt shipped alongside this
// program). This program does not mine anything itself -- it just builds the
// right command line from what the user fills in, auto-picks the best-
// matching bundled cpuminer-opt build for the current CPU, and launches it as
// a child process, streaming its output into the window.

#define _WIN32_WINNT 0x0601
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <intrin.h>

#pragma comment(lib, "comctl32.lib")

#define IDI_APPICON      101

#define ID_WALLET_EDIT   101
#define ID_ALGO_COMBO    102
#define ID_POOL_EDIT     103
#define ID_START_BTN     104
#define ID_STOP_BTN      105
#define ID_OUTPUT_EDIT   106
#define ID_WORKER_EDIT   107
#define ID_POOL_COMBO    108
#define ID_SSL_CHECK     109
#define ID_MODE_POOL     110
#define ID_MODE_SOLO     111
#define ID_NODE_URL_EDIT 112
#define ID_RPCUSER_EDIT  113
#define ID_RPCPASS_EDIT  114
#define ID_DEV_CPU       115
#define ID_DEV_GPU       116
#define ID_THREADS_COMBO 117
#define ID_GPU_DEV_EDIT  118
#define ID_REWARD_PROP   119
#define ID_REWARD_SOLO   120
#define ID_MENU_ABOUT    201
#define ID_ABOUT_CLOSE   202
#define WM_APPEND_OUTPUT (WM_USER + 1)

#define HEADER_H 46

// Background color sampled directly from the real MateableCoin wallet's own
// splash screen (src/qt/res/icons/splash.png, flat RGB 73,72,72) so the miner's
// loading screen, About box, and main window header all match the wallet's
// look instead of guessing.
#define MTBC_DARK RGB(73, 72, 72)
// Accent color sampled directly from the wallet's own app icon
// (src/qt/res/icons/bitcoin.png -- the dominant circle color), used for the
// primary action button and the header's divider line.
#define MTBC_ACCENT RGB(146, 41, 144)
#define MTBC_ACCENT_PRESSED RGB(110, 28, 108)

// Pool x Algo address table. zpool.ca is the only verified-working entry
// right now (every port here was confirmed against zpool's own API and
// live-tested with a real miner). Adding another pool later just means
// adding another row here and another dropdown entry -- no redesign.
// 3rd entry ("GrossCrypto Pool") is our own MiningCore-based pool,
// now live and public at pool.grosscrypto.com (was LAN-only at 10.1.10.222
// during development -- swapped to the real public hostname once HAProxy
// routing/UFW/DNS were all confirmed working end to end).
#define NUM_POOLS 3
#define NUM_ALGOS 5
// Algo column order everywhere in this file: yescryptr8, ghostrider,
// balloon, scrypt, whirlpool.
static const wchar_t* POOL_NAMES[NUM_POOLS] = { L"zpool.ca", L"KriptoKyng", L"GrossCrypto Pool" };
// Mining-Dutch was tried and pulled back out: their one officially-documented
// MTBC port (3407, same on both the scrypt and ghostrider pages) refuses/
// times out connections from three independent real networks. Their generic
// "multiport" ports (9996 default, 9997) DO connect, but live testing proved
// they mine a completely different coin (net diff 8697.68 PETA on 9996, a
// mismatched block height on 9997 -- nowhere near MTBC's real ~40-100 net
// diff) since which coin a shared port serves depends on the mining-dutch.nl
// account's dashboard setting, which this app can't read or control. A
// working connection to the wrong coin is worse than no pool at all, so this
// was removed rather than shipped half-right.
//
// KriptoKyng runs real MiningCore (standard, well-documented pool software --
// api.kriptokyng.com/api/help lists its full real API). Pulled connection
// details straight from api.kriptokyng.com/api/pools/ (ports, algorithm,
// payout scheme) rather than scraping their site, then live-tested all three
// MTBC algos it offers against the real chain: every one connected, reported
// the real current block height, and (yescryptr8/ghostrider) accepted real
// shares. Address-based like zpool, not account-based like Mining-Dutch --
// same payout label works for both. ghostrider specifically has real history
// behind it (2189 blocks found, 2123 MTBC paid out per the API at the time
// this was added), the strongest live evidence of any pool tried this
// session. Only the PROP (regular pool) ports are wired in; KriptoKyng also
// runs separate SOLO-payout ports (stratum-based solo mining through their
// relay, not the same thing as this app's own Solo/getblocktemplate mode) --
// not added here to keep the pool picker simple, but real and available
// (mtbcscryptsolo:2680, mtbcyesscryptsolo:2685, mtbcghostridersolo:2690) if
// ever wanted.
#define POOL_URL_UNSUPPORTED L""
static const wchar_t* POOL_URLS[NUM_POOLS][NUM_ALGOS] = {
    { // zpool.ca
        L"stratum+tcp://yescryptr8.mine.zpool.ca:6323",
        L"stratum+tcp://ghostrider.mine.zpool.ca:5354",
        L"stratum+tcp://balloon.mine.zpool.ca:5100",
        L"stratum+tcp://scrypt.mine.zpool.ca:3433",
        L"stratum+tcp://whirlpool.mine.zpool.ca:4133",
    },
    { // KriptoKyng -- yescryptr8/ghostrider/scrypt only (no balloon/whirlpool
      // pool on their side)
        L"stratum+tcp://stratum.kriptokyng.com:2683",
        L"stratum+tcp://stratum.kriptokyng.com:2688",
        POOL_URL_UNSUPPORTED,
        L"stratum+tcp://stratum.kriptokyng.com:2677",
        POOL_URL_UNSUPPORTED,
    },
    { // GrossCrypto Pool -- our own MiningCore-based pool, live and public
      // at pool.grosscrypto.com (see NUM_POOLS comment above). All 5 algos
      // live and tested: yescryptr8 (confirmed against our own coins.json --
      // this pool's "yescrypt" entry's actual headerHasher IS yescryptr8,
      // same algo as the column above, not a different one), ghostrider,
      // balloon, scrypt, whirlpool.
        L"stratum+tcp://pool.grosscrypto.com:3262",
        L"stratum+tcp://pool.grosscrypto.com:3263",
        L"stratum+tcp://pool.grosscrypto.com:3258",
        L"stratum+tcp://pool.grosscrypto.com:3261",
        L"stratum+tcp://pool.grosscrypto.com:3259",
    },
};
// SOLO reward-mode ports -- real, separate stratum ports KriptoKyng runs
// alongside their regular PROP ones (same API, payoutScheme:"SOLO" instead
// of "PROP"). Still goes through their server, NOT the same thing as this
// app's own "Solo (my own node)" mining mode (which is direct
// getblocktemplate RPC to a node you run yourself, no pool at all) -- this
// is "solo payout via a pool you don't have to run anything for": you get
// the full block reward (minus their ~2.1% fee) instead of a proportional
// share, no node setup required. Live-tested on ghostrider:2690 -- real
// connection, real accepted shares, same as the PROP port. zpool has no
// comparable solo concept in its own model, so its row is all unsupported.
static const wchar_t* POOL_URLS_SOLO[NUM_POOLS][NUM_ALGOS] = {
    { // zpool.ca -- no solo-payout ports
        POOL_URL_UNSUPPORTED, POOL_URL_UNSUPPORTED, POOL_URL_UNSUPPORTED,
        POOL_URL_UNSUPPORTED, POOL_URL_UNSUPPORTED,
    },
    { // KriptoKyng
        L"stratum+tcp://stratum.kriptokyng.com:2685",
        L"stratum+tcp://stratum.kriptokyng.com:2690",
        POOL_URL_UNSUPPORTED,
        L"stratum+tcp://stratum.kriptokyng.com:2680",
        POOL_URL_UNSUPPORTED,
    },
    { // GrossCrypto Pool -- payment processing/solo-payout ports aren't
      // configured on this pool right now, no real solo-payout ports exist
      // to wire in
        POOL_URL_UNSUPPORTED, POOL_URL_UNSUPPORTED, POOL_URL_UNSUPPORTED,
        POOL_URL_UNSUPPORTED, POOL_URL_UNSUPPORTED,
    },
};
// Pool password syntax differs per pool -- zpool needs c=/zap= to force a
// native MTBC payout (confirmed live earlier). KriptoKyng runs one dedicated
// stratum port per coin+algo (no multiport ambiguity), so a plain password
// is enough -- confirmed via real accepted shares in live testing.
static const wchar_t* POOL_PASSWORDS[NUM_POOLS] = { L"c=MTBC,zap=MTBC", L"x", L"x" };
// Both pools are address-based (raw MTBC wallet address as username) --
// same label works for both, unlike Mining-Dutch's account-based model.
static const wchar_t* POOL_PAYOUT_LABELS[NUM_POOLS] = {
    L"MTBC Payout Address:", L"MTBC Payout Address:", L"MTBC Payout Address:"
};
// SSL/TLS variants -- ssl_port values pulled live from zpool's own API
// (https://zpool.ca/api/status) and live-tested: cpuminer-opt supports the
// stratum+ssl:// scheme directly (confirmed in its own README), no extra
// flags needed.
// Short, factual descriptions -- not marketing copy. yescryptr8/balloon are
// the two algos this chain is realistically CPU-competitive on right now;
// scrypt's real network difficulty is ASIC/GPU territory (confirmed earlier
// via live testing -- not mineable at any useful rate on CPU).
static const wchar_t* ALGO_DESCRIPTIONS[NUM_ALGOS] = {
    L"Memory-hard, CPU-friendly (BitZeny-derived).\r\nGood CPU hashrate, low GPU/ASIC advantage.",
    L"Composite of multiple hash algorithms (Raptoreum's\r\nalgo). Runs well on both CPU and GPU.",
    L"MateableCoin's own memory-hard algorithm. CPU-focused;\r\ncurrently very low real network hashrate.",
    L"Classic Litecoin/Dogecoin algorithm. Real difficulty\r\nfavors ASIC/GPU -- CPU isn't competitive.",
    L"Cryptographic hash-based algorithm, usually GPU-favorable.\r\ncpuminer-opt mines it fine on CPU too -- live-tested against\r\nour own pool: thousands of accepted shares, no rejects.",
};

static const wchar_t* POOL_URLS_SSL[NUM_POOLS][NUM_ALGOS] = {
    { // zpool.ca
        L"stratum+ssl://yescryptr8.mine.zpool.ca:56323",
        L"stratum+ssl://ghostrider.mine.zpool.ca:55354",
        L"stratum+ssl://balloon.mine.zpool.ca:55100",
        L"stratum+ssl://scrypt.mine.zpool.ca:53433",
        L"stratum+ssl://whirlpool.mine.zpool.ca:54133",
    },
    { // KriptoKyng -- every port's API entry explicitly reports "tls":false;
      // no SSL offered, so don't invent one. SSL checkbox disables for this
      // pool automatically (same mechanism Mining-Dutch used).
        POOL_URL_UNSUPPORTED, POOL_URL_UNSUPPORTED, POOL_URL_UNSUPPORTED,
        POOL_URL_UNSUPPORTED, POOL_URL_UNSUPPORTED,
    },
    { // GrossCrypto Pool -- no TLS configured on any stratum port (plain
      // TCP passthrough through HAProxy, same as every other pool's
      // non-SSL ports)
        POOL_URL_UNSUPPORTED, POOL_URL_UNSUPPORTED, POOL_URL_UNSUPPORTED,
        POOL_URL_UNSUPPORTED, POOL_URL_UNSUPPORTED,
    },
};

static HWND g_hWallet, g_hAlgo, g_hPool, g_hPoolCombo, g_hWorker, g_hOutput, g_hStart, g_hStop, g_hSslCheck;
static HWND g_hStatusLabel = NULL;
static HWND g_hModePool, g_hModeSolo;
// Pool-mode-only controls (shown/hidden as a group when switching modes).
static HWND g_hPoolLabel, g_hPoolAddrLabel;
// Solo-mode-only controls -- a direct getblocktemplate RPC connection to a
// MateableCoin node the user runs themselves, no pool involved. cpuminer-opt
// supports this natively (confirmed via its own --help: --coinbase-addr,
// --no-gbt, --no-stratum are real flags, and RPC getblocktemplate over
// http://https:// is listed as a supported connection type in its README).
static HWND g_hNodeUrlLabel, g_hNodeUrl, g_hRpcUserLabel, g_hRpcUser, g_hRpcPassLabel, g_hRpcPass;
static HWND g_hWorkerLabel;
static HWND g_hPayoutLabel; // text swaps per pool: wallet address vs account username
// Mining device: CPU (cpuminer-opt, GPLv2) or GPU (ccminer-tpfuemp, GPLv3 --
// a completely separate program, NVIDIA/CUDA only, no coinbase-addr flag so
// it can't solo-mine; Solo gets force-disabled whenever GPU is selected).
static HWND g_hDevCpu, g_hDevGpu;
static HWND g_hThreadsLabel, g_hThreadsCombo;
static HWND g_hGpuDevLabel, g_hGpuDevEdit;
static HWND g_hAlgoDesc;
// Reward mode within Pool mode: Shared (PROP, the normal pooled payout) vs
// Solo (full block reward via the pool's own solo-payout port -- only shown/
// enabled when the selected pool+algo actually has one; see POOL_URLS_SOLO).
static HWND g_hRewardLabel, g_hRewardProp, g_hRewardSolo;
// Set once the child miner's own output shows it actually started hashing
// (not just launched) -- reset at the top of every StartMining() call.
static BOOL g_connectedAnnounced = FALSE;
static wchar_t g_connectTail[128] = L"";
// cpuminer-opt genuinely tracks this itself (solved_block_count in its own
// source) and prints "BLOCK SOLVED %d" in its one-liner result whenever an
// accepted share's diff also satisfies the full network difficulty -- a
// real signal, not inferred. Session-only (resets each Start), separate
// tail buffer since this needs to keep scanning for the whole run, not
// just until the first match like g_connectedAnnounced does.
static int g_blocksFound = 0;
static wchar_t g_blockTail[64] = L"";
static HANDLE g_hProcess = NULL;
static HANDLE g_hChildStdoutRd = NULL;
static HANDLE g_hReaderThread = NULL;
static volatile BOOL g_stopRequested = FALSE;
static volatile BOOL g_isMining = FALSE;
static HICON g_hAppIcon = NULL;      // 256x256 -- for DrawIconEx (downscales cleanly)
static HICON g_hAppIconSmall = NULL; // 48x48 -- for SS_ICON controls (draw at native size, no scaling)
// Set once at startup, before the main window is created -- see
// DetectNvidiaGpu(). Drives the Mining Device default (GPU pre-selected
// when a real one is usable, CPU otherwise).
static BOOL g_hasGpu = FALSE;

// Real presence check, not a guess: nvcuda.dll is exactly what
// ccminer-tpfuemp itself needs to even start (confirmed live earlier --
// it's the first thing that fails to load when there's no NVIDIA driver).
// If it loads here, a real CUDA-capable driver is actually installed, the
// same dependency the GPU engine itself has. Doesn't call into it or
// enumerate devices -- just the presence check, since there's no real
// NVIDIA GPU anywhere in this project's test environment to verify actual
// CUDA Driver API calls against, and a wrong calling-convention guess
// there could crash on a real user's machine. LoadLibrary/FreeLibrary are
// simple, well-defined Win32 calls with no such risk.
static BOOL DetectNvidiaGpu(void) {
    HMODULE h = LoadLibraryW(L"nvcuda.dll");
    if (h) { FreeLibrary(h); return TRUE; }
    return FALSE;
}
static wchar_t g_splashStatus[128] = L"Detecting CPU capabilities...";
static HFONT g_hBtnFont = NULL;
static HFONT g_hMonoFont = NULL;

// ---- Pick the best-matching bundled cpuminer-opt build for this CPU ----
// Checked from newest/fastest to oldest/safest; falls back to sse2 (every
// x86-64 CPU since ~2003 has this) if nothing newer is detected.
static const wchar_t* PickMinerBinary(void) {
    int cpuInfo[4];
    __cpuid(cpuInfo, 0);
    int maxLeaf = cpuInfo[0];

    BOOL hasAVX512 = FALSE, hasAVX2 = FALSE;
    if (maxLeaf >= 7) {
        __cpuidex(cpuInfo, 7, 0);
        hasAVX2   = (cpuInfo[1] & (1 << 5))  != 0; // EBX bit 5
        hasAVX512 = (cpuInfo[1] & (1 << 16)) != 0; // EBX bit 16 (AVX512F)
    }

    if (hasAVX512) return L"cpuminer-avx512.exe";
    if (hasAVX2)   return L"cpuminer-avx2.exe";
    return L"cpuminer-sse2.exe";
}

static void AppendOutput(const wchar_t* text) {
    int len = GetWindowTextLengthW(g_hOutput);
    SendMessageW(g_hOutput, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageW(g_hOutput, EM_REPLACESEL, FALSE, (LPARAM)text);
}

// Reads the child miner process's combined stdout/stderr pipe and forwards
// each chunk to the main window to display, until the pipe closes.
static DWORD WINAPI ReaderThreadProc(LPVOID param) {
    char buf[2048];
    DWORD bytesRead;
    for (;;) {
        BOOL ok = ReadFile(g_hChildStdoutRd, buf, sizeof(buf) - 1, &bytesRead, NULL);
        if (!ok || bytesRead == 0) break;
        buf[bytesRead] = '\0';

        int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, -1, NULL, 0);
        wchar_t* wbuf = (wchar_t*)malloc(wlen * sizeof(wchar_t));
        if (wbuf) {
            MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, wlen);
            wchar_t* copy = _wcsdup(wbuf);
            PostMessageW((HWND)param, WM_APPEND_OUTPUT, 0, (LPARAM)copy);
            free(wbuf);
        }
    }
    return 0;
}

// Composes the two-line status label ("● Idle"/"● Mining..." + a running
// Blocks Found count) and repaints it. Centralized so the count shown here
// can never drift from g_blocksFound regardless of which of the three call
// sites (Start, Stop, a block actually being found) last touched it.
static void UpdateStatusText(void) {
    if (!g_hStatusLabel) return;
    wchar_t text[64];
    swprintf(text, 64, L"%ls\r\nBlocks Found: %d",
        g_isMining ? L"● Mining..." : L"● Idle", g_blocksFound);
    SetWindowTextW(g_hStatusLabel, text);
    InvalidateRect(g_hStatusLabel, NULL, TRUE);
}

static void StopMining(void) {
    if (g_hProcess) {
        TerminateProcess(g_hProcess, 0);
        CloseHandle(g_hProcess);
        g_hProcess = NULL;
    }
    // Real bug this fixes: TerminateProcess kills the child, but the reader
    // thread is a SEPARATE thread still blocked in ReadFile() on the global
    // g_hChildStdoutRd handle. Without waiting for it here, the very next
    // StartMining() call immediately overwrites that same global with a
    // brand new pipe -- and the OLD thread, still looping on the global
    // (not a local copy), would start reading from the NEW run's pipe
    // instead of its own. Two threads racing ReadFile on one pipe produces
    // garbled/interleaved output, which is exactly what looked like "status
    // doesn't reset" when switching algo and restarting quickly.
    //
    // Terminating the child closes its end of the pipe, which unblocks the
    // reader thread's ReadFile on its own (broken-pipe/0-bytes return), so
    // this wait is normally fast -- the timeout is just a safety net so
    // Stop can never hang the UI thread.
    if (g_hReaderThread) {
        WaitForSingleObject(g_hReaderThread, 2000);
        CloseHandle(g_hReaderThread);
        g_hReaderThread = NULL;
    }
    if (g_hChildStdoutRd) {
        CloseHandle(g_hChildStdoutRd);
        g_hChildStdoutRd = NULL;
    }
    EnableWindow(g_hStart, TRUE);
    EnableWindow(g_hStop, FALSE);
    g_isMining = FALSE;
    UpdateStatusText();
}

static void StartMining(HWND hwnd) {
    BOOL soloMode = SendMessageW(g_hModeSolo, BM_GETCHECK, 0, 0) == BST_CHECKED;
    BOOL gpuMode = SendMessageW(g_hDevGpu, BM_GETCHECK, 0, 0) == BST_CHECKED;

    g_connectedAnnounced = FALSE;
    g_connectTail[0] = L'\0';
    g_blocksFound = 0;
    g_blockTail[0] = L'\0';
    // Fresh run, fresh log -- otherwise the previous run's output (possibly
    // a different algorithm/pool entirely) just sits above the new output
    // with nothing separating them, which reads as if nothing reset.
    SetWindowTextW(g_hOutput, L"");

    wchar_t wallet[256] = {0}, pool[512] = {0}, worker[128] = {0};
    wchar_t nodeUrl[256] = {0}, rpcUser[128] = {0}, rpcPass[128] = {0};
    wchar_t gpuDevices[64] = {0};
    GetWindowTextW(g_hWallet, wallet, 256);
    GetWindowTextW(g_hPool, pool, 512);
    GetWindowTextW(g_hWorker, worker, 128);
    GetWindowTextW(g_hNodeUrl, nodeUrl, 256);
    GetWindowTextW(g_hRpcUser, rpcUser, 128);
    GetWindowTextW(g_hRpcPass, rpcPass, 128);
    GetWindowTextW(g_hGpuDevEdit, gpuDevices, 64);

    if (wcslen(wallet) == 0) {
        MessageBoxW(hwnd, L"Enter your MTBC payout address first.", L"MateableCoin Miner", MB_ICONWARNING);
        return;
    }
    if (soloMode) {
        if (gpuMode) {
            // Shouldn't be reachable via the UI (Solo is force-disabled when
            // GPU is selected), but guard anyway: ccminer-tpfuemp's own
            // --help has no coinbase-address-equivalent flag, so there is no
            // honest way to direct a solo block reward to an arbitrary
            // address with it.
            MessageBoxW(hwnd, L"GPU mining doesn't support Solo mode (the GPU engine has no payout-address flag for solo mining). Switch to Pool, or back to CPU.",
                L"MateableCoin Miner", MB_ICONWARNING);
            return;
        }
        if (wcslen(nodeUrl) == 0 || wcslen(rpcUser) == 0 || wcslen(rpcPass) == 0) {
            MessageBoxW(hwnd, L"Solo mining needs your node's RPC URL, username, and password first.",
                L"MateableCoin Miner", MB_ICONWARNING);
            return;
        }
    }
    if (wcslen(worker) == 0) wcscpy(worker, L"worker1");

    int algoIdx = (int)SendMessageW(g_hAlgo, CB_GETCURSEL, 0, 0);
    const wchar_t* algos[] = { L"yescryptr8", L"ghostrider", L"balloon", L"scrypt", L"whirlpool" };
    const wchar_t* algo = algos[algoIdx >= 0 && algoIdx < 5 ? algoIdx : 0];

    int poolIdx = (int)SendMessageW(g_hPoolCombo, CB_GETCURSEL, 0, 0);
    if (poolIdx < 0 || poolIdx >= NUM_POOLS) poolIdx = 0;
    if (!soloMode && (wcslen(pool) == 0 || pool[0] == L'(')) {
        // UpdatePoolSelection() fills the field with a parenthesized message
        // instead of a URL whenever this pool/algo combo has no verified
        // endpoint (e.g. Mining-Dutch only supports scrypt/ghostrider for
        // MTBC) -- refuse to launch on that placeholder instead of handing
        // cpuminer-opt a bogus -o argument.
        MessageBoxW(hwnd, L"This pool doesn't support the selected algorithm for MTBC. Pick a different algorithm, or switch pools.",
            L"MateableCoin Miner", MB_ICONWARNING);
        return;
    }

    const wchar_t* minerExe = gpuMode ? L"ccminer.exe" : PickMinerBinary();

    wchar_t exePath[MAX_PATH], exeDir[MAX_PATH], fullMinerPath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wcscpy(exeDir, exePath);
    wchar_t* lastSlash = wcsrchr(exeDir, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    swprintf(fullMinerPath, MAX_PATH, L"%ls%ls", exeDir, minerExe);

    // CPU thread count: "Auto" (index 0) omits -t entirely, which both
    // engines document as defaulting to all detected processors/GPUs anyway.
    int threadsIdx = (int)SendMessageW(g_hThreadsCombo, CB_GETCURSEL, 0, 0);
    wchar_t threadsFlag[32] = L"";
    if (!gpuMode && threadsIdx > 0)
        swprintf(threadsFlag, 32, L" -t %d", threadsIdx);

    // GPU device list: blank means omit -d, which ccminer documents as using
    // every detected CUDA device. "0,1" etc. restricts to specific cards.
    wchar_t gpuDevFlag[96] = L"";
    if (gpuMode && wcslen(gpuDevices) > 0)
        swprintf(gpuDevFlag, 96, L" -d %ls", gpuDevices);

    wchar_t cmdLine[1024];
    if (soloMode) {
        // Direct getblocktemplate RPC to a node the user runs themselves --
        // no pool, no stratum. --coinbase-addr is a real cpuminer-opt flag
        // (confirmed via its own --help: "payout address for solo mining"),
        // so the block reward goes straight to this address when a block is
        // actually found. -u/-p here are the node's RPC credentials, not a
        // pool worker login. (CPU-only -- GPU+Solo is blocked above.)
        swprintf(cmdLine, 1024,
            L"\"%ls\" -a %ls -o %ls -u %ls -p %ls --coinbase-addr=%ls%ls",
            fullMinerPath, algo, nodeUrl, rpcUser, rpcPass, wallet, threadsFlag);
    } else {
        // NOTE: deliberately NOT using --stratum-keepalive. Its "no shares
        // submitted" watchdog is hardcoded to 4 minutes (not configurable via
        // -T/--timeout) and fires a self-reset of the stratum connection --
        // confirmed in cpuminer-opt's own source. yescryptr8/ghostrider see
        // shares often enough it never matters, but balloon's real share gaps
        // routinely exceed 4 minutes right now, so the flag was actively
        // *causing* disconnects there, not preventing them. Found this live
        // during testing -- plain stratum (no keepalive) is the correct default.
        swprintf(cmdLine, 1024,
            L"\"%ls\" -a %ls -o %ls -u %ls.%ls -p %ls%ls%ls",
            fullMinerPath, algo, pool, wallet, worker, POOL_PASSWORDS[poolIdx], threadsFlag, gpuDevFlag);
    }

    wchar_t banner[768];
    swprintf(banner, 768, L"\r\n--- Starting (%ls, engine: %ls) ---\r\n%ls\r\n\r\n",
        algo, minerExe, cmdLine);
    AppendOutput(banner);

    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hWr = NULL;
    if (!CreatePipe(&g_hChildStdoutRd, &hWr, &sa, 0)) {
        AppendOutput(L"ERROR: could not create output pipe.\r\n");
        return;
    }
    SetHandleInformation(g_hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWr;
    si.hStdError = hWr;
    si.hStdInput = NULL;

    PROCESS_INFORMATION pi = {0};
    BOOL ok = CreateProcessW(NULL, cmdLine, NULL, NULL, TRUE,
        CREATE_NO_WINDOW, NULL, exeDir, &si, &pi);
    CloseHandle(hWr);

    if (!ok) {
        wchar_t err[256];
        swprintf(err, 256, L"ERROR: failed to launch miner (code %lu). Is %ls present next to this program?\r\n",
            GetLastError(), minerExe);
        AppendOutput(err);
        CloseHandle(g_hChildStdoutRd);
        g_hChildStdoutRd = NULL;
        return;
    }

    g_hProcess = pi.hProcess;
    CloseHandle(pi.hThread);

    g_hReaderThread = CreateThread(NULL, 0, ReaderThreadProc, hwnd, 0, NULL);

    EnableWindow(g_hStart, FALSE);
    EnableWindow(g_hStop, TRUE);
    g_isMining = TRUE;
    UpdateStatusText();
}

// ---- About box -- a plain hand-rolled modal popup (no dialog template) ----
// Content mirrors what the real MateableCoin wallet's own About dialog shows
// (version + the mining engine's real license), just for this miner tool
// instead of the wallet.
static LRESULT CALLBACK AboutWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wp) == ID_ABOUT_CLOSE) DestroyWindow(hwnd);
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void ShowAboutDialog(HWND owner, HINSTANCE hInst) {
    static BOOL classRegistered = FALSE;
    if (!classRegistered) {
        WNDCLASSW ac = {0};
        ac.lpfnWndProc = AboutWndProc;
        ac.hInstance = hInst;
        ac.lpszClassName = L"MTBCAboutDlg";
        ac.hCursor = LoadCursorW(NULL, IDC_ARROW);
        ac.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        ac.hIcon = g_hAppIcon;
        RegisterClassW(&ac);
        classRegistered = TRUE;
    }

    int w = 440, h = 500;
    RECT ownerRect;
    GetWindowRect(owner, &ownerRect);
    int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - w) / 2;
    int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - h) / 2;

    HWND hDlg = CreateWindowW(L"MTBCAboutDlg", L"About MateableCoin Miner",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, w, h, owner, NULL, hInst, NULL);

    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    HFONT hBold = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    int yy = 20;
    if (g_hAppIconSmall) {
        HWND hIconStatic = CreateWindowW(L"STATIC", NULL, WS_VISIBLE | WS_CHILD | SS_ICON,
            20, yy, 48, 48, hDlg, NULL, hInst, NULL);
        SendMessageW(hIconStatic, STM_SETICON, (WPARAM)g_hAppIconSmall, 0);
    }
    HWND hTitle = CreateWindowW(L"STATIC", L"MateableCoin Miner", WS_VISIBLE | WS_CHILD,
        80, yy + 8, 320, 28, hDlg, NULL, hInst, NULL);
    SendMessageW(hTitle, WM_SETFONT, (WPARAM)hBold, TRUE);
    yy += 64;

    HWND hVer = CreateWindowW(L"STATIC", L"Version 1.0", WS_VISIBLE | WS_CHILD,
        20, yy, 300, 20, hDlg, NULL, hInst, NULL);
    SendMessageW(hVer, WM_SETFONT, (WPARAM)hFont, TRUE);
    yy += 28;

    const wchar_t* body =
        L"A native Windows GUI for mining MateableCoin (MTBC).\r\n\r\n"
        L"Supports all 5 MTBC algorithms: yescryptr8, ghostrider,\r\n"
        L"balloon, scrypt, and whirlpool. Mine through a pool, or\r\n"
        L"solo straight to a MateableCoin node you run yourself.\r\n"
        L"CPU and GPU (NVIDIA) mining both supported.\r\n\r\n"
        L"CPU engine: cpuminer-opt by tpfuemp (GPLv2)\r\n"
        L"https://github.com/tpfuemp/cpuminer-opt\r\n"
        L"GPU engine: ccminer-tpfuemp by tpfuemp (GPLv3)\r\n"
        L"https://github.com/tpfuemp/ccminer-tpfuemp\r\n"
        L"Full license text for both is shipped alongside this\r\n"
        L"program (cpuminer-opt-* and ccminer-tpfuemp-* files).\r\n\r\n"
        L"MTBC donation address:\r\n"
        L"MXdVfBXaB8uTgAC81wpuPZXQ5oDrehqLNj\r\n\r\n"
        L"© 2022-2026 MateableCoin\r\n"
        L"coin.mateable.com";

    HWND hBody = CreateWindowW(L"STATIC", body, WS_VISIBLE | WS_CHILD,
        20, yy, 400, 320, hDlg, NULL, hInst, NULL);
    SendMessageW(hBody, WM_SETFONT, (WPARAM)hFont, TRUE);
    yy += 330;

    HWND hClose = CreateWindowW(L"BUTTON", L"Close", WS_VISIBLE | WS_CHILD,
        (w - 100) / 2 - 8, yy, 100, 30, hDlg, (HMENU)ID_ABOUT_CLOSE, hInst, NULL);
    SendMessageW(hClose, WM_SETFONT, (WPARAM)hFont, TRUE);

    EnableWindow(owner, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    MSG msg;
    while (IsWindow(hDlg)) {
        if (GetMessageW(&msg, NULL, 0, 0) <= 0) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    DeleteObject(hBold);
}

// Refreshes the pool-address field, algo description, and payout-field
// label together from the current algo/pool/SSL/mode selection. Shared by
// the combo-change handler and SetModeVisibility (switching Pool<->Solo
// changes what the payout label should say even if nothing else changed).
static void UpdatePoolSelection(void) {
    int algoI = (int)SendMessageW(g_hAlgo, CB_GETCURSEL, 0, 0);
    int poolI = (int)SendMessageW(g_hPoolCombo, CB_GETCURSEL, 0, 0);
    if (algoI < 0 || algoI >= NUM_ALGOS) algoI = 0;
    if (poolI < 0 || poolI >= NUM_POOLS) poolI = 0;

    // Only offer the Solo reward mode where this pool has at least one real
    // solo-payout port published (currently just KriptoKyng). Force back to
    // Shared if it was on Solo and the pool/algo changed out from under it.
    BOOL poolHasAnySolo = FALSE;
    for (int a = 0; a < NUM_ALGOS; a++)
        if (wcslen(POOL_URLS_SOLO[poolI][a]) > 0) { poolHasAnySolo = TRUE; break; }
    EnableWindow(g_hRewardSolo, poolHasAnySolo);
    if (!poolHasAnySolo) SendMessageW(g_hRewardProp, BM_SETCHECK, BST_CHECKED, 0);
    BOOL soloReward = poolHasAnySolo && SendMessageW(g_hRewardSolo, BM_GETCHECK, 0, 0) == BST_CHECKED;

    // SSL only applies to the regular (Shared/PROP) ports -- none of the
    // solo-payout ports found so far have a TLS variant, so Solo reward mode
    // always disables SSL rather than imply a port that doesn't exist.
    BOOL poolHasAnySsl = FALSE;
    for (int a = 0; a < NUM_ALGOS; a++)
        if (wcslen(POOL_URLS_SSL[poolI][a]) > 0) { poolHasAnySsl = TRUE; break; }
    EnableWindow(g_hSslCheck, poolHasAnySsl && !soloReward);
    if (!poolHasAnySsl || soloReward) SendMessageW(g_hSslCheck, BM_SETCHECK, BST_UNCHECKED, 0);

    BOOL useSsl = !soloReward && poolHasAnySsl && SendMessageW(g_hSslCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
    const wchar_t* url = soloReward ? POOL_URLS_SOLO[poolI][algoI]
                        : useSsl    ? POOL_URLS_SSL[poolI][algoI]
                                    : POOL_URLS[poolI][algoI];
    SetWindowTextW(g_hPool, wcslen(url) > 0 ? url : L"(this pool doesn't support this algorithm for MTBC)");
    SetWindowTextW(g_hAlgoDesc, ALGO_DESCRIPTIONS[algoI]);

    // Solo (my own node) mode's wallet field is always a raw MTBC coinbase
    // address, regardless of which pool happens to be selected in the hidden
    // Pool-mode controls -- only show the pool-specific label while Pool
    // mode is active.
    BOOL soloNodeMode = SendMessageW(g_hModeSolo, BM_GETCHECK, 0, 0) == BST_CHECKED;
    SetWindowTextW(g_hPayoutLabel, soloNodeMode ? L"MTBC Payout Address:" : POOL_PAYOUT_LABELS[poolI]);
}

static void SetModeVisibility(BOOL solo) {
    ShowWindow(g_hPoolLabel, solo ? SW_HIDE : SW_SHOW);
    ShowWindow(g_hPoolCombo, solo ? SW_HIDE : SW_SHOW);
    ShowWindow(g_hRewardLabel, solo ? SW_HIDE : SW_SHOW);
    ShowWindow(g_hRewardProp, solo ? SW_HIDE : SW_SHOW);
    ShowWindow(g_hRewardSolo, solo ? SW_HIDE : SW_SHOW);
    ShowWindow(g_hSslCheck, solo ? SW_HIDE : SW_SHOW);
    ShowWindow(g_hPoolAddrLabel, solo ? SW_HIDE : SW_SHOW);
    ShowWindow(g_hPool, solo ? SW_HIDE : SW_SHOW);
    ShowWindow(g_hNodeUrlLabel, solo ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hNodeUrl, solo ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hRpcUserLabel, solo ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hRpcUser, solo ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hRpcPassLabel, solo ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hRpcPass, solo ? SW_SHOW : SW_HIDE);
    // Worker name only means anything for pool mode (-u WALLET.WORKER);
    // solo mode's -u is the node's RPC username instead.
    EnableWindow(g_hWorker, !solo);
    EnableWindow(g_hWorkerLabel, !solo);
    UpdatePoolSelection();
}

static void SetDeviceVisibility(BOOL gpu) {
    ShowWindow(g_hThreadsLabel, gpu ? SW_HIDE : SW_SHOW);
    ShowWindow(g_hThreadsCombo, gpu ? SW_HIDE : SW_SHOW);
    ShowWindow(g_hGpuDevLabel, gpu ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hGpuDevEdit, gpu ? SW_SHOW : SW_HIDE);
    // ccminer-tpfuemp has no coinbase-address-equivalent flag (confirmed via
    // its own --help), so it genuinely can't solo-mine to an arbitrary
    // payout address -- force back to Pool mode and lock Solo out while GPU
    // is selected, rather than let the GUI offer a combination that can't
    // honestly work.
    EnableWindow(g_hModeSolo, !gpu);
    if (gpu && SendMessageW(g_hModeSolo, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        SendMessageW(g_hModePool, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(g_hModeSolo, BM_SETCHECK, BST_UNCHECKED, 0);
        SetModeVisibility(FALSE);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        g_hBtnFont = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        g_hMonoFont = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");

        int groupTop = HEADER_H + 8;
        int y = groupTop + 22;

        CreateWindowW(L"STATIC", L"Mining Device:", WS_VISIBLE | WS_CHILD,
            24, y, 190, 20, hwnd, NULL, NULL, NULL);
        g_hDevCpu = CreateWindowW(L"BUTTON", L"CPU",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
            220, y, 90, 22, hwnd, (HMENU)ID_DEV_CPU, NULL, NULL);
        g_hDevGpu = CreateWindowW(L"BUTTON", L"GPU (NVIDIA)",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
            315, y, 200, 22, hwnd, (HMENU)ID_DEV_GPU, NULL, NULL);
        // Default to whichever is actually usable -- g_hasGpu was set during
        // the splash sequence via a real nvcuda.dll presence check, not a
        // guess. GPU mining is generally more competitive than CPU for these
        // algorithms when a real NVIDIA card is actually there to use.
        if (g_hasGpu) SendMessageW(g_hDevGpu, BM_SETCHECK, BST_CHECKED, 0);
        else SendMessageW(g_hDevCpu, BM_SETCHECK, BST_CHECKED, 0);
        y += 22;

        {
            HWND hDetectLabel = CreateWindowW(L"STATIC",
                g_hasGpu ? L"Detected: NVIDIA GPU found, GPU mining selected"
                         : L"Detected: no NVIDIA GPU found, CPU mining selected",
                WS_VISIBLE | WS_CHILD, 220, y, 370, 18, hwnd, NULL, NULL, NULL);
            HFONT hSmallFont = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            SendMessageW(hDetectLabel, WM_SETFONT, (WPARAM)hSmallFont, TRUE);
        }
        y += 24;

        CreateWindowW(L"STATIC", L"Mining Mode:", WS_VISIBLE | WS_CHILD,
            24, y, 190, 20, hwnd, NULL, NULL, NULL);
        g_hModePool = CreateWindowW(L"BUTTON", L"Pool",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
            220, y, 90, 22, hwnd, (HMENU)ID_MODE_POOL, NULL, NULL);
        g_hModeSolo = CreateWindowW(L"BUTTON", L"Solo (my own node)",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
            315, y, 200, 22, hwnd, (HMENU)ID_MODE_SOLO, NULL, NULL);
        SendMessageW(g_hModePool, BM_SETCHECK, BST_CHECKED, 0);
        y += 30;

        g_hPayoutLabel = CreateWindowW(L"STATIC", POOL_PAYOUT_LABELS[0], WS_VISIBLE | WS_CHILD,
            24, y, 190, 20, hwnd, NULL, NULL, NULL);
        g_hWallet = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER,
            220, y - 3, 345, 24, hwnd, (HMENU)ID_WALLET_EDIT, NULL, NULL);
        y += 30;

        g_hWorkerLabel = CreateWindowW(L"STATIC", L"Worker name (pool only):", WS_VISIBLE | WS_CHILD,
            24, y, 190, 20, hwnd, NULL, NULL, NULL);
        g_hWorker = CreateWindowW(L"EDIT", L"worker1", WS_VISIBLE | WS_CHILD | WS_BORDER,
            220, y - 3, 345, 24, hwnd, (HMENU)ID_WORKER_EDIT, NULL, NULL);
        y += 30;

        CreateWindowW(L"STATIC", L"Algorithm:", WS_VISIBLE | WS_CHILD,
            24, y, 190, 20, hwnd, NULL, NULL, NULL);
        g_hAlgo = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
            220, y - 3, 345, 200, hwnd, (HMENU)ID_ALGO_COMBO, NULL, NULL);
        SendMessageW(g_hAlgo, CB_ADDSTRING, 0, (LPARAM)L"yescryptr8 (CPU)");
        SendMessageW(g_hAlgo, CB_ADDSTRING, 0, (LPARAM)L"ghostrider (CPU/GPU)");
        SendMessageW(g_hAlgo, CB_ADDSTRING, 0, (LPARAM)L"balloon (CPU)");
        SendMessageW(g_hAlgo, CB_ADDSTRING, 0, (LPARAM)L"scrypt (ASIC/GPU)");
        SendMessageW(g_hAlgo, CB_ADDSTRING, 0, (LPARAM)L"whirlpool (CPU/GPU)");
        SendMessageW(g_hAlgo, CB_SETCURSEL, 0, 0);
        y += 22;

        g_hAlgoDesc = CreateWindowW(L"STATIC", ALGO_DESCRIPTIONS[0], WS_VISIBLE | WS_CHILD,
            220, y, 345, 30, hwnd, NULL, NULL, NULL);
        y += 36;

        // --- CPU row: thread count / GPU row: device list (toggled by device) ---
        g_hThreadsLabel = CreateWindowW(L"STATIC", L"CPU Threads:", WS_VISIBLE | WS_CHILD,
            24, y, 190, 20, hwnd, NULL, NULL, NULL);
        g_hThreadsCombo = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
            220, y - 3, 345, 300, hwnd, (HMENU)ID_THREADS_COMBO, NULL, NULL);
        {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            DWORD numCores = si.dwNumberOfProcessors;
            SendMessageW(g_hThreadsCombo, CB_ADDSTRING, 0, (LPARAM)L"Auto (all cores)");
            for (DWORD n = 1; n <= numCores; n++) {
                wchar_t label[32];
                swprintf(label, 32, L"%lu", n);
                SendMessageW(g_hThreadsCombo, CB_ADDSTRING, 0, (LPARAM)label);
            }
            SendMessageW(g_hThreadsCombo, CB_SETCURSEL, 0, 0);
        }

        g_hGpuDevLabel = CreateWindowW(L"STATIC", L"GPU Devices (blank = all):", WS_VISIBLE | WS_CHILD,
            24, y, 190, 20, hwnd, NULL, NULL, NULL);
        // ccminer-tpfuemp's own --help: "-d, --devices  comma separated list
        // of CUDA device IDs... Device IDs start counting from 0!" Omitting
        // -d entirely (blank field) mines on every detected card.
        g_hGpuDevEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER,
            220, y - 3, 345, 24, hwnd, (HMENU)ID_GPU_DEV_EDIT, NULL, NULL);
        y += 30;

        // --- Pool-mode row 1 / Solo-mode row 1 (same slot, toggled) ---
        g_hPoolLabel = CreateWindowW(L"STATIC", L"Mining Pool:", WS_VISIBLE | WS_CHILD,
            24, y, 190, 20, hwnd, NULL, NULL, NULL);
        g_hPoolCombo = CreateWindowW(L"COMBOBOX", L"", WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST,
            220, y - 3, 345, 200, hwnd, (HMENU)ID_POOL_COMBO, NULL, NULL);
        for (int i = 0; i < NUM_POOLS; i++)
            SendMessageW(g_hPoolCombo, CB_ADDSTRING, 0, (LPARAM)POOL_NAMES[i]);
        SendMessageW(g_hPoolCombo, CB_SETCURSEL, 0, 0);

        g_hNodeUrlLabel = CreateWindowW(L"STATIC", L"Node RPC URL (http://host:port):", WS_VISIBLE | WS_CHILD,
            24, y, 190, 20, hwnd, NULL, NULL, NULL);
        g_hNodeUrl = CreateWindowW(L"EDIT", L"http://127.0.0.1:6966", WS_VISIBLE | WS_CHILD | WS_BORDER,
            220, y - 3, 345, 24, hwnd, (HMENU)ID_NODE_URL_EDIT, NULL, NULL);
        y += 30;

        // Pool-mode only -- reward mode (Shared/PROP vs Solo-via-pool).
        // Hidden entirely in Solo-(my own node) mode, same as the pool
        // picker itself; the Solo radio here disables per-pool if that
        // pool has no real solo-payout port (see POOL_URLS_SOLO).
        g_hRewardLabel = CreateWindowW(L"STATIC", L"Reward Mode:", WS_VISIBLE | WS_CHILD,
            24, y, 190, 20, hwnd, NULL, NULL, NULL);
        g_hRewardProp = CreateWindowW(L"BUTTON", L"Shared (PROP)",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP,
            220, y, 130, 22, hwnd, (HMENU)ID_REWARD_PROP, NULL, NULL);
        g_hRewardSolo = CreateWindowW(L"BUTTON", L"Solo (via pool)",
            WS_VISIBLE | WS_CHILD | BS_AUTORADIOBUTTON,
            355, y, 150, 22, hwnd, (HMENU)ID_REWARD_SOLO, NULL, NULL);
        SendMessageW(g_hRewardProp, BM_SETCHECK, BST_CHECKED, 0);
        y += 26;

        // --- Pool-mode row 2 / Solo-mode row 2 ---
        g_hSslCheck = CreateWindowW(L"BUTTON", L"Use SSL/TLS (encrypted connection)",
            WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
            220, y, 300, 22, hwnd, (HMENU)ID_SSL_CHECK, NULL, NULL);

        g_hRpcUserLabel = CreateWindowW(L"STATIC", L"RPC Username:", WS_VISIBLE | WS_CHILD,
            24, y, 190, 20, hwnd, NULL, NULL, NULL);
        g_hRpcUser = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER,
            220, y - 3, 345, 24, hwnd, (HMENU)ID_RPCUSER_EDIT, NULL, NULL);
        y += 26;

        // --- Pool-mode row 3 / Solo-mode row 3 ---
        g_hPoolAddrLabel = CreateWindowW(L"STATIC", L"Pool address (stratum+tcp://...):", WS_VISIBLE | WS_CHILD,
            24, y, 190, 20, hwnd, NULL, NULL, NULL);
        // Real, verified zpool.ca endpoint (port confirmed via zpool's own
        // API; c=MTBC,zap=MTBC confirmed working via live test -- zpool
        // explicitly disables auto-conversion for MTBC and requires this
        // exact syntax to get paid in real MTBC instead of another coin).
        // Still a plain editable field -- picking a pool above just fills
        // this in as a starting point, advanced users can still override it.
        g_hPool = CreateWindowW(L"EDIT", POOL_URLS[0][0],
            WS_VISIBLE | WS_CHILD | WS_BORDER,
            220, y - 3, 345, 24, hwnd, (HMENU)ID_POOL_EDIT, NULL, NULL);

        g_hRpcPassLabel = CreateWindowW(L"STATIC", L"RPC Password:", WS_VISIBLE | WS_CHILD,
            24, y, 190, 20, hwnd, NULL, NULL, NULL);
        g_hRpcPass = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD,
            220, y - 3, 345, 24, hwnd, (HMENU)ID_RPCPASS_EDIT, NULL, NULL);
        y += 20;

        int groupBottom = y + 10;
        HWND hGroup = CreateWindowW(L"BUTTON", L"Mining Configuration", WS_VISIBLE | WS_CHILD | BS_GROUPBOX,
            10, groupTop, 575, groupBottom - groupTop, hwnd, NULL, NULL, NULL);
        SendMessageW(hGroup, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Start in Pool mode -- hide the solo-only controls until switched.
        ShowWindow(g_hNodeUrlLabel, SW_HIDE); ShowWindow(g_hNodeUrl, SW_HIDE);
        ShowWindow(g_hRpcUserLabel, SW_HIDE); ShowWindow(g_hRpcUser, SW_HIDE);
        ShowWindow(g_hRpcPassLabel, SW_HIDE); ShowWindow(g_hRpcPass, SW_HIDE);
        // Match field visibility (CPU Threads vs GPU Devices, Solo-node lock)
        // to whichever device actually got auto-selected above.
        SetDeviceVisibility(g_hasGpu);

        y = groupBottom + 12;
        // Two lines (status + running Blocks Found count) -- taller than a
        // single status line, so this row gets a bit more vertical room
        // below than the buttons alone would need.
        g_hStatusLabel = CreateWindowW(L"STATIC", L"● Idle\r\nBlocks Found: 0", WS_VISIBLE | WS_CHILD,
            24, y + 2, 190, 36, hwnd, NULL, NULL, NULL);

        g_hStart = CreateWindowW(L"BUTTON", L"Start Mining",
            WS_VISIBLE | WS_CHILD | BS_OWNERDRAW,
            220, y, 170, 32, hwnd, (HMENU)ID_START_BTN, NULL, NULL);
        g_hStop = CreateWindowW(L"BUTTON", L"Stop", WS_VISIBLE | WS_CHILD | WS_DISABLED,
            400, y, 165, 32, hwnd, (HMENU)ID_STOP_BTN, NULL, NULL);
        y += 48;

        g_hOutput = CreateWindowW(L"EDIT", L"MateableCoin Miner ready. Enter your wallet address and press Start Mining.\r\n",
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_VSCROLL |
            ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            15, y, 570, 160, hwnd, (HMENU)ID_OUTPUT_EDIT, NULL, NULL);

        HWND children[] = { g_hWallet, g_hWorker, g_hAlgo, g_hPool, g_hSslCheck, g_hStop, g_hStatusLabel,
            g_hModePool, g_hModeSolo, g_hNodeUrl, g_hRpcUser, g_hRpcPass,
            g_hDevCpu, g_hDevGpu, g_hThreadsCombo, g_hGpuDevEdit,
            g_hRewardProp, g_hRewardSolo };
        for (size_t i = 0; i < sizeof(children)/sizeof(children[0]); i++)
            SendMessageW(children[i], WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(g_hOutput, WM_SETFONT, (WPARAM)g_hMonoFont, TRUE);

        // Label every plain STATIC text (not the group box, which keeps its
        // own default rendering) with the stock font too.
        HWND hChild = GetWindow(hwnd, GW_CHILD);
        while (hChild) {
            wchar_t cls[32];
            GetClassNameW(hChild, cls, 32);
            if (_wcsicmp(cls, L"Static") == 0)
                SendMessageW(hChild, WM_SETFONT, (WPARAM)hFont, TRUE);
            hChild = GetWindow(hChild, GW_HWNDNEXT);
        }

        UpdatePoolSelection();
        return 0;
    }
    case WM_APPEND_OUTPUT: {
        wchar_t* text = (wchar_t*)lp;
        AppendOutput(text);

        // "N of N miner threads started" only means the process launched and
        // *tried* to connect -- NOT that it actually did (caught this live:
        // against an unreachable pool, that line prints and the threads spin
        // up anyway, with no connection ever following). The real proof is
        // "Stratum connection established" (pool mode) or the first "New
        // Block"/"New Work:" (solo/GBT mode, which has no such line -- both
        // phrases confirmed present in real captured output from earlier
        // live tests). Checked against a small tail buffer too, so a chunk
        // boundary can't split a phrase and hide it.
        if (!g_connectedAnnounced) {
            const wchar_t* markers[] = { L"Stratum connection established", L"New Block", L"New Work:" };
            BOOL found = FALSE;
            for (size_t m = 0; m < sizeof(markers)/sizeof(markers[0]) && !found; m++)
                found = wcsstr(text, markers[m]) != NULL;
            if (!found) {
                wchar_t boundary[256];
                swprintf(boundary, 256, L"%ls%.150ls", g_connectTail, text);
                for (size_t m = 0; m < sizeof(markers)/sizeof(markers[0]) && !found; m++)
                    found = wcsstr(boundary, markers[m]) != NULL;
            }
            if (found) {
                g_connectedAnnounced = TRUE;
                AppendOutput(L"\r\n>>> MateableCoin Miner: connected and mining! <<<\r\n\r\n");
            } else {
                size_t len = wcslen(text);
                size_t tailLen = len > 100 ? 100 : len;
                wcscpy(g_connectTail, text + (len - tailLen));
            }
        }

        // "BLOCK SOLVED" -- real, not inferred (see g_blocksFound comment).
        // Keeps scanning for the whole session, not just until first match.
        {
            BOOL foundBlock = wcsstr(text, L"BLOCK SOLVED") != NULL;
            if (!foundBlock) {
                wchar_t boundary[128];
                swprintf(boundary, 128, L"%ls%.80ls", g_blockTail, text);
                foundBlock = wcsstr(boundary, L"BLOCK SOLVED") != NULL;
            }
            if (foundBlock) {
                g_blocksFound++;
                UpdateStatusText();
                wchar_t blockMsg[128];
                swprintf(blockMsg, 128,
                    L"\r\n>>> BLOCK FOUND! Total this session: %d <<<\r\n\r\n", g_blocksFound);
                AppendOutput(blockMsg);
            }
            size_t len = wcslen(text);
            size_t tailLen = len > 40 ? 40 : len;
            wcscpy(g_blockTail, text + (len - tailLen));
        }

        free(text);
        return 0;
    }
    case WM_PAINT: {
        // Branded header strip -- same dark tone as the splash screen and
        // About box, so the main window doesn't look like a different app.
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        rc.bottom = HEADER_H;
        HBRUSH bg = CreateSolidBrush(MTBC_DARK);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        RECT divider = { rc.left, HEADER_H - 2, rc.right, HEADER_H };
        HBRUSH accentBrush = CreateSolidBrush(MTBC_ACCENT);
        FillRect(hdc, &divider, accentBrush);
        DeleteObject(accentBrush);

        SetBkMode(hdc, TRANSPARENT);
        HFONT fTitle = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT old = (HFONT)SelectObject(hdc, fTitle);
        SetTextColor(hdc, RGB(255, 255, 255));
        const wchar_t* titleText = L"MateableMiner";
        TextOutW(hdc, 16, 6, titleText, (int)wcslen(titleText));
        SelectObject(hdc, old);
        DeleteObject(fTitle);

        HFONT fSub = CreateFontW(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        old = (HFONT)SelectObject(hdc, fSub);
        SetTextColor(hdc, RGB(205, 205, 205));
        const wchar_t* subText = L"Mine MTBC";
        TextOutW(hdc, 16, 25, subText, (int)wcslen(subText));
        SelectObject(hdc, old);
        DeleteObject(fSub);

        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        if ((HWND)lp == g_hStatusLabel) {
            HDC hdcStatic = (HDC)wp;
            SetTextColor(hdcStatic, g_isMining ? RGB(0, 140, 60) : RGB(110, 110, 110));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }
        break;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lp;
        if (dis->CtlID == ID_START_BTN) {
            BOOL disabled = (dis->itemState & ODS_DISABLED) != 0;
            BOOL pressed = (dis->itemState & ODS_SELECTED) != 0;
            COLORREF fill = disabled ? RGB(195, 195, 195) : (pressed ? MTBC_ACCENT_PRESSED : MTBC_ACCENT);
            HBRUSH hBrush = CreateSolidBrush(fill);
            FillRect(dis->hDC, &dis->rcItem, hBrush);
            DeleteObject(hBrush);

            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, disabled ? RGB(235, 235, 235) : RGB(255, 255, 255));
            HFONT old = (HFONT)SelectObject(dis->hDC, g_hBtnFont);
            wchar_t text[64];
            GetWindowTextW(dis->hwndItem, text, 64);
            DrawTextW(dis->hDC, text, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(dis->hDC, old);
            return TRUE;
        }
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == ID_START_BTN) StartMining(hwnd);
        else if (LOWORD(wp) == ID_STOP_BTN) StopMining();
        else if (LOWORD(wp) == ID_MENU_ABOUT)
            ShowAboutDialog(hwnd, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE));
        else if ((LOWORD(wp) == ID_MODE_POOL || LOWORD(wp) == ID_MODE_SOLO) && HIWORD(wp) == BN_CLICKED) {
            SetModeVisibility(LOWORD(wp) == ID_MODE_SOLO);
        }
        else if ((LOWORD(wp) == ID_DEV_CPU || LOWORD(wp) == ID_DEV_GPU) && HIWORD(wp) == BN_CLICKED) {
            SetDeviceVisibility(LOWORD(wp) == ID_DEV_GPU);
        }
        else if ((LOWORD(wp) == ID_ALGO_COMBO && HIWORD(wp) == CBN_SELCHANGE)
                 || (LOWORD(wp) == ID_POOL_COMBO && HIWORD(wp) == CBN_SELCHANGE)
                 || (LOWORD(wp) == ID_SSL_CHECK && HIWORD(wp) == BN_CLICKED)
                 || (LOWORD(wp) == ID_REWARD_PROP && HIWORD(wp) == BN_CLICKED)
                 || (LOWORD(wp) == ID_REWARD_SOLO && HIWORD(wp) == BN_CLICKED)) {
            UpdatePoolSelection();
        }
        return 0;
    case WM_CLOSE:
        StopMining();
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (g_hBtnFont) { DeleteObject(g_hBtnFont); g_hBtnFont = NULL; }
        if (g_hMonoFont) { DeleteObject(g_hMonoFont); g_hMonoFont = NULL; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---- Splash / loading screen -----------------------------------------
// This now mirrors the REAL MateableCoin wallet's own splashscreen.cpp
// layout, not just its color: that code draws a big app icon top-left
// (15,20, 180x180), the "MateableCoin" wordmark centered directly under it,
// and a big bold title + version + copyright block right-aligned near the
// top (paddingTop=50, paddingRight=30, 33pt title). Same 480x320 canvas size
// the real splash uses. Unlike a fake progress bar, the status line here
// reflects real work -- the actual CPU-capability check this program always
// does before it can launch a miner.
static LRESULT CALLBACK SplashWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HBRUSH bg = CreateSolidBrush(MTBC_DARK);
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));

        // Icon top-left + "MateableCoin" wordmark centered right under it --
        // same position/sizing as the real wallet's splash.
        const int iconX = 15, iconY = 20, iconSize = 180;
        if (g_hAppIcon) {
            SetStretchBltMode(hdc, HALFTONE);
            SetBrushOrgEx(hdc, 0, 0, NULL);
            DrawIconEx(hdc, iconX, iconY, g_hAppIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
        }

        HFONT fWordmark = CreateFontW(17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT old = (HFONT)SelectObject(hdc, fWordmark);
        RECT wordmarkRect = { iconX, iconY + iconSize + 25, iconX + iconSize, iconY + iconSize + 50 };
        DrawTextW(hdc, L"MateableCoin", -1, &wordmarkRect, DT_CENTER | DT_TOP | DT_SINGLELINE);
        SelectObject(hdc, old);
        DeleteObject(fWordmark);

        // Title/version/copyright block -- matches the REAL wallet's actual
        // logic (confirmed by compiling and running its real splashscreen.cpp
        // painting code standalone, not guessed from reading it): the title's
        // own measured width sets one shared left X, and version/status/
        // copyright all start at that SAME X too (not each independently
        // right-aligned, which is what this used to do and looked off).
        const int rightEdge = 450; // canvas width 480 - 30 padding

        HFONT fTitle = CreateFontW(32, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        old = (HFONT)SelectObject(hdc, fTitle);
        const wchar_t* titleText = L"Miner";
        SIZE titleSz;
        GetTextExtentPoint32W(hdc, titleText, (int)wcslen(titleText), &titleSz);
        int titleX = rightEdge - titleSz.cx;
        // "Miner" is short, which would push titleX far enough right to leave
        // too little room for the version/status/copyright lines below it
        // (confirmed live -- status text was running off the canvas edge).
        // Clamp so the column always keeps a reasonable minimum width.
        if (titleX > 270) titleX = 270;
        TextOutW(hdc, titleX, 50, titleText, (int)wcslen(titleText));
        SelectObject(hdc, old);
        DeleteObject(fTitle);

        HFONT fSmall = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        old = (HFONT)SelectObject(hdc, fSmall);
        // Bounded + word-wrapped (not plain TextOutW) so nothing can ever
        // run past the canvas edge, however long the real status text gets.
        RECT verRect = { titleX, 95, 475, 113 };
        DrawTextW(hdc, L"Version 1.0", -1, &verRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
        RECT statusRect = { titleX, 118, 475, 150 };
        DrawTextW(hdc, g_splashStatus, -1, &statusRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
        RECT copyRect = { titleX, 155, 475, 195 };
        DrawTextW(hdc, L"© 2022-2026 MateableCoin", -1, &copyRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
        SelectObject(hdc, old);
        DeleteObject(fSmall);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void PumpFor(DWORD ms) {
    DWORD start = GetTickCount();
    MSG msg;
    while (GetTickCount() - start < ms) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(10);
    }
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow) {
    (void)hPrev; (void)cmdLine;

    // LoadIconW always returns the small ~32x32 system-metric frame no
    // matter how many sizes the .ico has embedded -- every DrawIconEx call
    // below was then stretching that tiny bitmap UP to 36/48/180px, which is
    // exactly why it looked blurry. LoadImageW with an explicit large size
    // pulls the real 256x256 frame instead, so every draw downscales a crisp
    // source rather than upscaling a blurry one.
    g_hAppIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON, 256, 256, LR_DEFAULTCOLOR);
    // SS_ICON static controls (the About dialog) draw an icon at its native
    // size with no scaling -- a 48x48 control showing the 256x256 handle
    // would just clip to its top-left corner, so load a second handle
    // that's genuinely 48x48 instead of stretching/cropping one for both uses.
    g_hAppIconSmall = (HICON)LoadImageW(hInst, MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR);

    // --- Splash / loading screen, shown before the main window ---
    WNDCLASSW sc = {0};
    sc.lpfnWndProc = SplashWndProc;
    sc.hInstance = hInst;
    sc.lpszClassName = L"MTBCSplash";
    sc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    sc.hIcon = g_hAppIcon;
    RegisterClassW(&sc);

    int splashW = 480, splashH = 320;
    int sx = (GetSystemMetrics(SM_CXSCREEN) - splashW) / 2;
    int sy = (GetSystemMetrics(SM_CYSCREEN) - splashH) / 2;
    HWND hSplash = CreateWindowW(L"MTBCSplash", L"MateableCoin Miner", WS_POPUP | WS_VISIBLE,
        sx, sy, splashW, splashH, NULL, NULL, hInst, NULL);
    UpdateWindow(hSplash);
    PumpFor(500);

    // Real status update, not a fake progress bar -- this is the actual CPU
    // feature check StartMining() relies on to pick the right miner binary,
    // plus the real GPU presence check that decides the Mining Device default.
    const wchar_t* minerBuild = PickMinerBinary();
    g_hasGpu = DetectNvidiaGpu();
    swprintf(g_splashStatus, 128, L"CPU build: %ls   GPU: %ls",
        minerBuild, g_hasGpu ? L"NVIDIA detected" : L"none detected");
    InvalidateRect(hSplash, NULL, TRUE);
    UpdateWindow(hSplash);
    PumpFor(1100);

    DestroyWindow(hSplash);

    // --- Main window ---
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"MTBCMinerGUI";
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.hIcon = g_hAppIcon;
    RegisterClassW(&wc);

    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenuW(hHelpMenu, MF_STRING, ID_MENU_ABOUT, L"&About MateableCoin Miner");
    HMENU hMenuBar = CreateMenu();
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hHelpMenu, L"&Help");

    HWND hwnd = CreateWindowW(L"MTBCMinerGUI", L"MateableCoin Miner",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 610, 720,
        NULL, hMenuBar, hInst, NULL);

    if (g_hAppIcon) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_hAppIcon);
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hAppIcon);
    }

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
