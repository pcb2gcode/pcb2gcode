#!/bin/bash
set -e

echo "=== pcb2gcode Ubuntu Setup Script ==="

# Install / update all required build dependencies via apt
echo "Installing build essentials and dependencies via apt..."
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config libboost-program-options-dev libboost-test-dev libgtkmm-2.4-dev gerbv librsvg2-dev libgeos++-dev git

# Fix known Ubuntu 24.04 bug where libgerbv.pc has an empty Version field
GERBV_PC=$(find /usr/lib -name "libgerbv.pc" 2>/dev/null | head -n 1)
if [ -n "$GERBV_PC" ] && [ -f "$GERBV_PC" ]; then
    if grep -q "^Version:[[:space:]]*$" "$GERBV_PC"; then
        echo "Fixing empty Version field in $GERBV_PC (Ubuntu 24.04 workaround)..."
        sudo sed -i 's/^Version:[[:space:]]*$/Version: 2.10.0/' "$GERBV_PC"
    fi
fi

echo "Building pcb2gcode with MCP support..."
mkdir -p build
cd build
cmake .. -DBUILD_TESTING=OFF -DPCB2GCODE_COMPILE_WARNING_AS_ERROR=OFF
make -j$(nproc)

BINARY_PATH=$(pwd)/pcb2gcode
CONFIG_FILE="../mcp-config.json"

# Detect if running under WSL
if grep -qi microsoft /proc/version 2>/dev/null; then
  # WSL environment
  cat <<EOF > ${CONFIG_FILE}
{
  "mcpServers": {
    "pcb2gcode": {
      "command": "wsl.exe",
      "args": [
        "-e",
        "${BINARY_PATH}",
        "--mcp"
      ]
    }
  }
}
EOF
else
  # Native Ubuntu environment
  cat <<EOF > ${CONFIG_FILE}
{
  "mcpServers": {
    "pcb2gcode": {
      "command": "${BINARY_PATH}",
      "args": [
        "--mcp"
      ]
    }
  }
}
EOF
fi

echo ""
echo "=== Build Complete! ==="
echo "Binary location: ${BINARY_PATH}"
echo "Generated MCP configuration: $(readlink -f ${CONFIG_FILE})"
echo ""
echo "Copy the contents of ${CONFIG_FILE} into your MCP client configuration (e.g., mcp.json or claude_desktop_config.json)."
