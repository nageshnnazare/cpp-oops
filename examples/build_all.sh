#!/usr/bin/env bash
# Build (and optionally run) all cpp-oops examples.
#
# Usage:
#   ./build_all.sh          # compile every example your compiler supports
#   ./build_all.sh --run    # compile AND run each example
#
# Most examples are C++20. ch14_deducing_this.cpp requires C++23
# (GCC 14+/Clang 18+). Unsupported ones are skipped with a note.

set -u
CXX="${CXX:-clang++}"
WARN="-Wall -Wextra -Wnon-virtual-dtor"
RUN=0
[[ "${1:-}" == "--run" ]] && RUN=1

cd "$(dirname "$0")" || exit 1
OUT="$(mktemp -d)"
pass=0; fail=0; skip=0

# detect standard support
supports() { echo 'int main(){}' | "$CXX" -std="$1" -x c++ - -o /dev/null 2>/dev/null; }
STD20=c++20; STD23=""
supports c++23 && STD23=c++23 || (supports c++2b && STD23=c++2b)

for src in ch*.cpp; do
    std="$STD20"
    case "$src" in
        ch14_deducing_this.cpp)
            if [[ -z "$STD23" ]]; then
                echo "SKIP  $src (needs C++23)"; skip=$((skip+1)); continue
            fi
            std="$STD23" ;;
    esac

    bin="$OUT/${src%.cpp}"
    if "$CXX" -std="$std" $WARN "$src" -o "$bin" 2>"$OUT/err.txt"; then
        echo "OK    $src  [-std=$std]"
        pass=$((pass+1))
        if [[ $RUN -eq 1 ]]; then
            echo "----- run $src -----"
            "$bin"
            echo "--------------------"
        fi
    else
        echo "FAIL  $src  [-std=$std]"
        cat "$OUT/err.txt"
        fail=$((fail+1))
    fi
done

echo
echo "Summary: $pass passed, $fail failed, $skip skipped"
rm -rf "$OUT"
[[ $fail -eq 0 ]]
