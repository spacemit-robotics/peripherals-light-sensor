#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
module_root="$(cd "$script_dir/.." && pwd)"
artifact_dir="${SROBOTIS_TEST_ARTIFACT_DIR:-${SROBOTIS_OUTPUT_ROOT:-$PWD/output}/test-artifacts/components/peripherals/light_sensor/${SROBOTIS_TEST_NAME:-light-sensor-api-contract}}"
log_dir="$artifact_dir/logs"
build_dir="$artifact_dir/build"
mode="${1:-all}"
case "$mode" in
    all|functional|error-paths) ;;
    *) echo "usage: $0 [all|functional|error-paths]" >&2; exit 2 ;;
esac

log_file="$log_dir/light_sensor_api_${mode//-/_}.log"
cc="${CC:-cc}"

mkdir -p "$log_dir" "$build_dir"

{
    echo "[info] module_root=$module_root"
    echo "[info] build_dir=$build_dir"
    echo "[info] cc=$cc"
    echo "[info] mode=$mode"

    "$cc" -std=c99 -Wall -Wextra -Werror \
        -I"$module_root/include" \
        -I"$module_root/src" \
        -I"$module_root/src/drivers" \
        "$module_root/src/light_sensor_core.c" \
        "$module_root/tests/test_light_sensor_api_contract.c" \
        -o "$build_dir/test_light_sensor_api_contract"

    "$build_dir/test_light_sensor_api_contract" "$mode"
} | tee "$log_file"

case "$mode" in
    all) grep -q "light_sensor api contract test PASSED" "$log_file" ;;
    functional) grep -q "light_sensor api functional test PASSED" "$log_file" ;;
    error-paths) grep -q "light_sensor api error paths test PASSED" "$log_file" ;;
esac
