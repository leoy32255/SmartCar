#!/usr/bin/env bash
# ==============================================================================
#  语法预检脚本
#
#  作用：在还没用 CubeMX 生成 HAL 库之前，就能把 Core/Src 下的所有代码
#        用 gcc -fsyntax-only 编译一遍，抓出拼写/类型/签名/括号错误。
#
#  为什么需要它：HAL 库要 CubeMX 生成后才存在，而生成一次挺费事。
#        如果等到那时才发现少个分号，排查成本高很多。
#        本脚本用 .ci_check/ 下的 stub 头文件替代真实 HAL，
#        只做语法和类型检查，不产出任何目标文件。
#
#  用法：
#    bash .ci_check/check.sh          # 检查两个平台
#    bash .ci_check/check.sh F1       # 只检查 F103
#    bash .ci_check/check.sh F4       # 只检查 F407
#
#  注意：stub 只覆盖本工程实际用到的 HAL 接口。
#        如果新用了别的外设（如 ADC、DMA），需要往 stub 里补对应声明。
# ==============================================================================

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CC=${CC:-gcc}
WARN="-Wall -Wextra"
INC="-I.ci_check -ICore/Inc"
TARGETS="${1:-F1 F4}"

fail=0

check_target() {
    local family="$1" def="$2" label="$3"

    echo "=============================================="
    echo " 语法检查：$label  (-D$def)"
    echo "=============================================="

    if ! command -v "$CC" >/dev/null 2>&1; then
        echo "  跳过：找不到编译器 $CC"
        return 0
    fi

    local n_ok=0
    local n_bad=0
    for f in Core/Src/*.c; do
        local out
        out=$("$CC" -std=c11 $WARN -fsyntax-only -D"$def" -DUSE_HAL_DRIVER $INC "$f" 2>&1)
        if [ -n "$out" ]; then
            echo "  [失败] $f"
            echo "$out" | sed 's/^/         /' | head -20
            fail=1
            n_bad=$((n_bad + 1))
        else
            n_ok=$((n_ok + 1))
        fi
    done
    if [ "$n_bad" -eq 0 ]; then
        echo "  $n_ok 个文件通过，无警告无错误"
    else
        echo "  $n_ok 个文件通过，$n_bad 个文件失败"
    fi
    echo
}

for t in $TARGETS; do
    case "$t" in
        F1) check_target F1 STM32F103xB "STM32F103C8T6" ;;
        F4) check_target F4 STM32F407xx "STM32F407VET6" ;;
        *)  echo "未知目标 '$t'，只支持 F1 / F4" ;;
    esac
done

if [ "$fail" -eq 0 ]; then
    echo "全部通过。"
else
    echo "存在错误，见上面的输出。"
fi

exit "$fail"
