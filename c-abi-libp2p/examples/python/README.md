# Python: ping_two_nodes

Small `ctypes` script that:

1. Loads the compiled `libcabi_rust_libp2p.so`.
2. Creates two nodes (`listener` and `dialer`) through the C ABI.
3. Starts TCP listeners and initiates a connection.
4. Gives the nodes time to exchange libp2p ping traffic.

## Environment setup

1. Activate the dedicated Conda environment for the project:
   ```bash
   conda activate fidonext-abi
   ```
2. Create a local virtual environment for the Python example:
   ```bash
   cd /home/georgeb/fidonext-core/c-abi-libp2p/examples/python
   bash setup_env.sh
   source .venv/bin/activate
   ```

## Run the example

1. Build the Rust shared library (produces `target/debug/libcabi_rust_libp2p.so`):
   ```bash
   cd /home/georgeb/fidonext-core/c-abi-libp2p
   cargo build
   ```
2. (Optional) Tweak logging for verbose ping output:
   ```bash
   export RUST_LOG="info,peer=debug,ffi=debug"
   ```
3. Execute the Python client-to-client ping test:
   ```bash
   python3 examples/python/ping_two_nodes.py
   ```
   To force QUIC instead of TCP, pass the `--use-quic` flag (the script will
   switch to `/udp/.../quic-v1` multiaddrs automatically):
   ```bash
   python3 examples/python/ping_two_nodes.py --use-quic
   ```
4. Observe the console: successful runs show both peers listening, dialing, and
   establishing a connection. Ping RTTs appear in the Rust logs when
   `peer=debug` is enabled. The script automatically shuts down both nodes after
   ~5 seconds, so you may see a final “connection closed” warning—this is
   expected during teardown.

* By default the script expects `target/debug/libcabi_rust_libp2p.so`. Override
  the location via the `FIDONEXT_C_ABI` environment variable.
* Rust logs (`peer` / `ffi`) surface connection events and ping RTTs.
* Additional CLI knobs: `--listener-port`, `--dialer-port`, and `--duration`
  (seconds to keep nodes alive after dialing).
* Example command pair for taking a QUIC capture while running the test
  (execute in two terminals):
  ```bash
  # Terminal 1: capture QUIC packets for 15 seconds
  sudo tshark -i lo -f "udp port 41000 or udp port 41001" \
      -a duration:15 -w /home/georgeb/fidonext-core/fidonext_ping.pcapng
  ```
  ```bash
  # Terminal 2: run the QUIC ping demo
  conda activate fidonext-abi
  cd /home/georgeb/fidonext-core/c-abi-libp2p
  RUST_LOG="info,peer=debug,ffi=debug" python3 examples/python/ping_two_nodes.py --use-quic
  ```
