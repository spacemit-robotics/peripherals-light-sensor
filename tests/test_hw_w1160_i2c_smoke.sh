#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
module_root="$(cd "$script_dir/.." && pwd)"
artifact_dir="${SROBOTIS_TEST_ARTIFACT_DIR:-${SROBOTIS_OUTPUT_ROOT:-$PWD/output}/test-artifacts/components/peripherals/light_sensor/${SROBOTIS_TEST_NAME:-light-sensor-w1160-hardware-smoke}}"
log_dir="$artifact_dir/logs"
build_dir="$artifact_dir/build"
log_file="$log_dir/light_sensor_w1160_hardware_smoke.log"

dev_path="${LIGHT_SENSOR_I2C_DEV:-/dev/i2c-5}"
addr="${LIGHT_SENSOR_I2C_ADDR:-0x48}"
timeout_s="${LIGHT_SENSOR_HW_SMOKE_TIMEOUT_S:-30}"

mkdir -p "$log_dir" "$build_dir"

{
    echo "[info] module_root=$module_root"
    echo "[info] build_dir=$build_dir"
    echo "[info] dev_path=$dev_path"
    echo "[info] addr=$addr"
    echo "[info] timeout_s=$timeout_s"

    cmake -S "$module_root" -B "$build_dir" \
        -DBUILD_TESTS=ON \
        -DSROBOTIS_PERIPHERALS_LIGHT_SENSOR_ENABLED_DRIVERS=drv_i2c_w1160
    cmake --build "$build_dir" --target test_light_sensor_i2c -j"$(nproc)"
    echo "[info] waiting for a light sensor value"
    set +e
    LD_LIBRARY_PATH="$build_dir:${LD_LIBRARY_PATH:-}" \
        stdbuf -oL -eL "$build_dir/test_light_sensor_i2c" "$dev_path" "$addr" &
    test_pid=$!
    seen=0
    for _ in $(seq 1 "$timeout_s"); do
        if grep -q "Light value:" "$log_file"; then
            seen=1
            break
        fi
        if ! kill -0 "$test_pid" 2>/dev/null; then
            break
        fi
        sleep 1
    done
    if kill -0 "$test_pid" 2>/dev/null; then
        kill "$test_pid" 2>/dev/null
        wait "$test_pid" 2>/dev/null
    else
        wait "$test_pid" 2>/dev/null
    fi
    set -e
    if [ "$seen" -ne 1 ]; then
        echo "[error] no light sensor value observed within ${timeout_s}s"
        exit 1
    fi
} 2>&1 | tee "$log_file"

grep -q "Light value:" "$log_file"
