

adb push buildtools/ndk/toolchains/llvm/prebuilt/darwin-x86_64/lib/clang/18/lib/linux/aarch64/lldb-server /data/local/tmp

adb shell /data/local/tmp/lldb-server p --server --listen unix-abstract:///data/local/tmp/debug.sock

