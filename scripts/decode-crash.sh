#!/bin/bash
# Decode ESP32 crash stack traces from serial output
# Usage: ./scripts/decode-crash.sh [serial_log_file]
#   or pipe serial output: cat /dev/cu.usbmodem101 | ./scripts/decode-crash.sh
#   or just run to monitor live: ./scripts/decode-crash.sh --monitor

set -euo pipefail

ELF=".pio/build/default/firmware.elf"
ADDR2LINE="$(find "$HOME/.platformio/packages/toolchain-riscv32-esp" -name 'riscv32-esp-elf-addr2line' 2>/dev/null | head -1)"
PORT="${SERIAL_PORT:-/dev/cu.usbmodem101}"

if [[ -z "$ADDR2LINE" ]]; then
  echo "Error: riscv32-esp-elf-addr2line not found in ~/.platformio"
  exit 1
fi

if [[ ! -f "$ELF" ]]; then
  echo "Error: $ELF not found. Run 'pio run' first."
  exit 1
fi

decode_addresses() {
  # Extract hex addresses from crash dump and decode them
  local input="$1"

  # Extract PC address from abort line
  local pc_addr=$(echo "$input" | grep -oE 'PC 0x[0-9a-fA-F]+' | head -1 | grep -oE '0x[0-9a-fA-F]+')

  # Extract all addresses from stack memory (4-byte aligned, in code range 0x4200xxxx-0x42xxxxxx)
  local stack_addrs=$(echo "$input" | grep -oE '0x42[0-9a-fA-F]{6}' | sort -u)

  echo "=== CRASH DECODE ==="
  echo ""

  if [[ -n "$pc_addr" ]]; then
    echo "--- Crash PC ---"
    echo -n "  $pc_addr: "
    "$ADDR2LINE" -e "$ELF" -f -C "$pc_addr" 2>/dev/null | tr '\n' ' '
    echo ""
    echo ""
  fi

  echo "--- Stack Trace (code addresses) ---"
  for addr in $stack_addrs; do
    local result=$("$ADDR2LINE" -e "$ELF" -f -C "$addr" 2>/dev/null)
    local func=$(echo "$result" | head -1)
    local loc=$(echo "$result" | tail -1)
    # Skip unknown/runtime addresses
    if [[ "$loc" != *"??"* && "$func" != "??" ]]; then
      printf "  %-14s %-50s %s\n" "$addr" "$func" "$loc"
    fi
  done
  echo ""
  echo "=== END ==="
}

if [[ "${1:-}" == "--monitor" ]]; then
  echo "Monitoring $PORT for crashes... (open the epub now, Ctrl+C to stop)"
  echo "---"
  # Capture serial output, detect crash, decode
  stty -f "$PORT" 115200 2>/dev/null || true
  buffer=""
  in_crash=false
  while IFS= read -r line; do
    echo "$line"
    if echo "$line" | grep -q "abort() was called"; then
      in_crash=true
      buffer="$line"
    elif $in_crash; then
      buffer="$buffer"$'\n'"$line"
      # End of crash dump — decode after seeing enough stack memory
      if echo "$line" | grep -q "^$" || echo "$buffer" | grep -c "0x42" | grep -q "[5-9]\|[1-9][0-9]"; then
        if echo "$buffer" | grep -cq "0x42"; then
          echo ""
          decode_addresses "$buffer"
          in_crash=false
          buffer=""
        fi
      fi
    fi
  done < "$PORT"
elif [[ -n "${1:-}" && -f "${1:-}" ]]; then
  decode_addresses "$(cat "$1")"
else
  # Read from stdin
  input=$(cat)
  decode_addresses "$input"
fi
