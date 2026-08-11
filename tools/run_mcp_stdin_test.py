#!/usr/bin/env python3
"""End-to-end test for `pcb2gcode --mcp` over stdio.

Spawns the pcb2gcode binary with --mcp, sends a couple of JSON-RPC requests
on stdin (`initialize`, then `tools/list`), reads and validates the responses
on stdout, closes stdin, and asserts the process exits cleanly.

This exercises the same lifecycle a real MCP client (Claude Desktop, Cursor,
etc.) uses for a stdio server: launch subprocess, exchange line-delimited
JSON-RPC 2.0 messages, close stdin to shut it down.

Usage:
  tools/run_mcp_stdin_test.py <pcb2gcode_binary> [--timeout SECONDS]
"""

from __future__ import print_function

import argparse
import json
import subprocess
import sys
import threading


_DEFAULT_TIMEOUT = 30.0


def _send(proc, request):
    line = json.dumps(request) + "\n"
    proc.stdin.write(line)
    proc.stdin.flush()


def _read_response(proc, timeout):
    """Read a single line-delimited JSON-RPC response from the server.

    Uses a helper thread so we can enforce a timeout without relying on
    non-blocking I/O (which behaves differently on Windows).
    """
    result = {}

    def _reader():
        try:
            result["line"] = proc.stdout.readline()
        except Exception as exc:
            result["error"] = exc

    t = threading.Thread(target=_reader)
    t.daemon = True
    t.start()
    t.join(timeout)
    if t.is_alive():
        proc.kill()
        raise AssertionError(
            "Timed out after {}s waiting for MCP response".format(timeout))
    if "error" in result:
        raise result["error"]
    line = result.get("line", "")
    if not line:
        stderr = proc.stderr.read() if proc.stderr is not None else ""
        raise AssertionError(
            "MCP server closed stdout before sending a response; "
            "stderr was:\n{}".format(stderr))
    return json.loads(line)


def _wait_for_exit(proc, timeout):
    try:
        return proc.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait()
        raise AssertionError(
            "MCP server did not exit within {}s after closing stdin".format(
                timeout))


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcb2gcode_binary",
                        help="Path to the pcb2gcode executable to test")
    parser.add_argument("--timeout", type=float, default=_DEFAULT_TIMEOUT,
                        help="Per-step timeout in seconds (default: %(default)s)")
    args = parser.parse_args(argv)

    proc = subprocess.Popen(
        [args.pcb2gcode_binary, "--mcp"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1)

    try:
        _send(proc, {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {}})
        resp = _read_response(proc, args.timeout)
        assert resp.get("jsonrpc") == "2.0", "bad jsonrpc: {}".format(resp)
        assert resp.get("id") == 1, "bad id: {}".format(resp)
        server_info = resp.get("result", {}).get("serverInfo", {})
        assert server_info.get("name") == "pcb2gcode", \
            "bad serverInfo: {}".format(resp)

        _send(proc, {
            "jsonrpc": "2.0",
            "id": 2,
            "method": "tools/list",
            "params": {}})
        resp = _read_response(proc, args.timeout)
        assert resp.get("id") == 2, "bad id: {}".format(resp)
        tools = resp.get("result", {}).get("tools", [])
        tool_names = sorted(t.get("name") for t in tools)
        assert tool_names == [
            "pcb2gcode_help", "pcb2gcode_run", "pcb2gcode_version"], \
            "unexpected tools: {}".format(tool_names)

        # tools/call branches are covered by tests/mcp_server_tests.cpp
        # (which can measure coverage in-process). Here we only verify the
        # stdio lifecycle: after we close stdin below, the server should
        # exit cleanly on its own.
    finally:
        try:
            proc.stdin.close()
        except (OSError, ValueError):
            pass

    exit_code = _wait_for_exit(proc, args.timeout)
    assert exit_code == 0, "pcb2gcode --mcp exited with {}\nstderr:\n{}".format(
        exit_code, proc.stderr.read() if proc.stderr else "")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
