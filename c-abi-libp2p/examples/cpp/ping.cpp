#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// Crossplatform
#ifdef _WIN32
#include <windows.h>
#undef max
#undef min
using LibHandle = HMODULE;
#define LOAD_LIB(path) LoadLibraryA(path)
#define GET_PROC(lib, name) GetProcAddress(lib, name)
#define CLOSE_LIB(lib) FreeLibrary(lib)
constexpr auto LIB_NAME = "cabi_rust_libp2p.dll";
#else
#include <dlfcn.h>
using LibHandle = void*;
#define LOAD_LIB(path) dlopen(path, RTLD_LAZY)
#define GET_PROC(lib, name) dlsym(lib, name)
#define CLOSE_LIB(lib) dlclose(lib)
constexpr auto LIB_NAME = "./libcabi_rust_libp2p.so";
#endif

using std::cout;
using std::cerr;
using std::string;

// General statuses
// Operation completed successfully.
constexpr int CABI_STATUS_SUCCESS = 0;
// One of the provided pointers was null.
constexpr int CABI_STATUS_NULL_POINTER = 1;
// Invalid argument supplied (e.g. malformed multiaddr).
constexpr int CABI_STATUS_INVALID_ARGUMENT = 2;
// Internal runtime error – check logs for details.
constexpr int CABI_STATUS_INTERNAL_ERROR = 3;

// AutoNAT statuses
// AutoNAT status has not yet been determined.
constexpr int CABI_AUTONAT_UNKNOWN = 0;
// AutoNAT reports the node as privately reachable only.
constexpr int CABI_AUTONAT_PRIVATE = 1;
// AutoNAT reports the node as publicly reachable.
constexpr int CABI_AUTONAT_PUBLIC  = 2;

// Payload statuses
// Queue contains no new messages.
constexpr int CABI_STATUS_QUEUE_EMPTY = 4;
// Provided buffer too small to hold the next message.
constexpr int CABI_STATUS_BUFFER_TOO_SMALL = 5;
// Default capacity for the message queue.
constexpr int DEFAULT_MESSAGE_QUEUE_CAPACITY = 64;

using InitTracingFunc = int (*)();
using NewNodeFunc = void* (*)(
  bool useQuic,
  bool enableRelayHop,
  const char* const* bootstrapPeers,
  size_t bootstrapPeersLen);
using ListenNodeFunc = int (*)(void* handle, const char* multiaddr);
using DialNodeFunc = int (*)(void* handle, const char* multiaddr);
using AutonatStatusFunc = int (*)(void* handle);
using EnqueueMessageFunc = int (*)(void* handle, const uint8_t* data_ptr, size_t data_len);
using DequeueMessageFunc = int (*)(void* handle, uint8_t* out_buffer, size_t buffer_len, size_t* written_len);
using FreeNodeFunc = void (*)(void* handle);

struct CabiRustLibp2p
{
  InitTracingFunc       InitTracing{};
  NewNodeFunc           NewNode{};
  ListenNodeFunc        ListenNode{};
  DialNodeFunc          DialNode{};
  AutonatStatusFunc     AutonatStatus{};
  EnqueueMessageFunc    EnqueueMessage{};
  DequeueMessageFunc    DequeueMessage{};
  FreeNodeFunc          FreeNode{};
};

enum class Role
{
  Relay,
  Leaf,
};

struct Arguments
{
  Role role = Role::Leaf;
  bool useQuic = false;
  string listen;
  std::vector<string> bootstrapPeers{};
  std::vector<string> targetPeers{};
};

// RAII for handle of the node
struct NodeHandle
{
  void reset(void* newHandle = nullptr)
  {
    if (handle && abi && abi->FreeNode)
    {
      abi->FreeNode(handle);
    }

    handle = newHandle;
  }

  ~NodeHandle()
  {
    reset();
  }

  void* handle = nullptr;
  const CabiRustLibp2p* abi = nullptr;
};

string statusMessage(int status)
{
  switch (status)
  {
  case CABI_STATUS_SUCCESS:
    return "ok";
  case CABI_STATUS_NULL_POINTER:
    return "Null pointer passed into ABI";
  case CABI_STATUS_INVALID_ARGUMENT:
    return "Invalid argument (multiaddr or UTF-8)";
  case CABI_STATUS_QUEUE_EMPTY:
    return "Queue empty";
  case CABI_STATUS_BUFFER_TOO_SMALL:
    return "Provided buffer too small";
  default:
    return "Internal error - inspect Rust logs for details";
  }
}

bool loadAbi(LibHandle lib, CabiRustLibp2p& abi)
{
  abi.InitTracing = reinterpret_cast<InitTracingFunc>(GET_PROC(lib, "cabi_init_tracing"));
  abi.NewNode = reinterpret_cast<NewNodeFunc>(GET_PROC(lib, "cabi_node_new_with_relay_bootstrap_and_seed"));
  abi.ListenNode = reinterpret_cast<ListenNodeFunc>(GET_PROC(lib, "cabi_node_listen"));
  abi.DialNode = reinterpret_cast<DialNodeFunc>(GET_PROC(lib, "cabi_node_dial"));
  abi.AutonatStatus = reinterpret_cast<AutonatStatusFunc>(GET_PROC(lib, "cabi_autonat_status"));
  abi.EnqueueMessage = reinterpret_cast<EnqueueMessageFunc>(GET_PROC(lib, "cabi_node_enqueue_message"));
  abi.DequeueMessage = reinterpret_cast<DequeueMessageFunc>(GET_PROC(lib, "cabi_node_dequeue_message"));
  abi.FreeNode = reinterpret_cast<FreeNodeFunc>(GET_PROC(lib, "cabi_node_free"));

  return  abi.InitTracing && abi.NewNode &&
          abi.ListenNode && abi.DialNode && abi.AutonatStatus && 
          abi.EnqueueMessage && abi.DequeueMessage && abi.FreeNode;
}

string defaultListen(bool useQuic)
{
  if (useQuic)
  {
    return "/ip4/127.0.0.1/udp/41000/quic-v1";
  }

  return "/ip4/127.0.0.1/tcp/41000";
}

Arguments parseArgs(int argc, char** argv)
{
  Arguments args;
  bool listenProvided = false;

  for (int i = 1; i < argc; ++i)
  {
    const string arg = argv[i];

    if (arg == "--role" && i + 1 < argc)
    {
      const string roleValue = argv[++i];
      if (roleValue == "relay")
      {
        args.role = Role::Relay;
      }
      else if (roleValue == "leaf")
      {
        args.role = Role::Leaf;
      }
      else
      {
        throw std::invalid_argument("--role must be 'relay' or 'leaf'");
      }
    }
    else if (arg == "--use-quic")
    {
      args.useQuic = true;
    }
    else if (arg == "--listen" && i + 1 < argc)
    {
      args.listen = argv[++i];
      listenProvided = true;
    }
    else if (arg == "--bootstrap" && i + 1 < argc)
    {
      args.bootstrapPeers.emplace_back(argv[++i]);
    }
    else if (arg == "--target" && i + 1 < argc)
    {
      args.targetPeers.emplace_back(argv[++i]);
    }
    else if (arg == "--help" || arg == "-h")
    {
      cout  << "relay_chat usage:\n"
            << "  --role relay|leaf (default: leaf)\n"
            << "  --use-quic\n"
            << "  --listen <multiaddr>\n"
            << "  --bootstrap <multiaddr> (repeatable)\n"
            << "  --target <multiaddr> (repeatable)\n";

      std::exit(0);
    }
    else
    {
      throw std::invalid_argument("Unknown argument: " + arg);
    }
  }

  if (!listenProvided)
  {
    args.listen = defaultListen(args.useQuic);
  }

  return args;
}

std::vector<const char*> toCStrVector(const std::vector<string>& values)
{
  std::vector<const char*> result;
  result.reserve(values.size());

  for (const auto& value : values)
  {
    result.push_back(value.c_str());
  }

  return result;
}

void* createNode(
  const CabiRustLibp2p& abi,
  bool useQuic,
  bool enableRelayHop,
  const std::vector<string>& bootstrapPeers)
{
  auto bootstrapPtrs = toCStrVector(bootstrapPeers);
  void* node = abi.NewNode(
    useQuic,
    enableRelayHop,
    bootstrapPtrs.data(),
    bootstrapPtrs.size());

  if (!node)
  {
    throw std::runtime_error("failed to create node; see Rust logs for details");
  }

  return node;
}

// Get Autonat status in order to have a possibility
// to detect whether it is public or private
bool waitForPublicAutonat(const CabiRustLibp2p& abi, void* node,
  std::chrono::seconds timeout = std::chrono::seconds(10))
{
  auto start = std::chrono::steady_clock::now();

  while (std::chrono::steady_clock::now() - start < timeout)
  {
    const int status = abi.AutonatStatus(node);
    if (status == CABI_AUTONAT_PUBLIC)
    {
      return true;
    }

    if (status == CABI_AUTONAT_PRIVATE)
    {
      cout << "AutoNAT: private\n";
    }
    else if (status == CABI_AUTONAT_UNKNOWN)
    {
      cout << "AutoNAT: unknown\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return false;
}

void recvLoop(
  const CabiRustLibp2p& abi,
  void* node,
  std::atomic<bool>& keepRunning)
{
  std::vector<uint8_t> buffer(1024);

  while (keepRunning.load(std::memory_order_acquire))
  {
    size_t written = 0;
    // Here you get the message
    const auto recvStatus = abi.DequeueMessage(
      node,
      buffer.data(),
      buffer.size(),
      &written);

    if (recvStatus == CABI_STATUS_SUCCESS)
    {
      const string payload(
        reinterpret_cast<const char*>(buffer.data()),
        written);
      cout << "Received payload: '" << payload << "'\n";
      continue;
    }

    // Wait a lil bit to reduce rquests
    if (recvStatus == CABI_STATUS_QUEUE_EMPTY)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    // Hadle bufer is to small to recieve payload
    if (recvStatus == CABI_STATUS_BUFFER_TOO_SMALL)
    {
      const auto newSize = std::max(buffer.size() * 2, written);
      buffer.resize(newSize);
      cerr << "Resized receive buffer to " << newSize << " bytes\n";
      continue;
    }

    cerr << "Failed to dequeue message: " << statusMessage(recvStatus) << "\n";
    keepRunning.store(false, std::memory_order_release);
    break;
  }
}

void sendLoop(
  const CabiRustLibp2p& abi,
  void* node,
  std::atomic<bool>& keepRunning)
{
  cout << "Enter payload (empty line or /quit to exit):\n";
  string line;

  while (keepRunning.load(std::memory_order_acquire) && std::getline(std::cin, line))
  {
    // Quit scenario
    if (line.empty() || line == "/quit")
    {
      keepRunning.store(false, std::memory_order_release);
      break;
    }

    // This one sends the payloads
    const auto sendStatus = abi.EnqueueMessage(
      node,
      reinterpret_cast<const uint8_t*>(line.data()),
      line.size());

    // Quit of failing sending message
    if (sendStatus != CABI_STATUS_SUCCESS)
    {
      cerr << "Failed to send message: " << statusMessage(sendStatus) << "\n";
      keepRunning.store(false, std::memory_order_release);
      break;
    }

    cout << "Enter payload (empty line or /quit to exit):\n";
  }
}

// Initital dial to know that peer is enabled
void dialPeers(const CabiRustLibp2p& abi, void* node, const std::vector<string>& peers, const char* label)
{
  for (const auto& addr : peers)
  {
    const auto status = abi.DialNode(node, addr.c_str());
    if (status == CABI_STATUS_SUCCESS)
    {
      cout << "Dialed " << label << " peer: " << addr << "\n";
    }
    else
    {
      cerr << "Failed to dial " << label << " peer " << addr << ": " << statusMessage(status) << "\n";
    }
  }
}

int main(int argc, char** argv)
{
  // Step 1. Load lib
  LibHandle lib = LOAD_LIB(LIB_NAME);
  if (!lib)
  {
    cerr << "Error loading lib: " << LIB_NAME << "\n";
    return 1;
  }

  // Step 2. Load functions from lib
  CabiRustLibp2p abi{};
  if (!loadAbi(lib, abi))
  {
    cerr << "Missing required functions in library\n";
    CLOSE_LIB(lib);
    return 1;
  }

  // Step 3. Parse args
  Arguments args;
  try
  {
    args = parseArgs(argc, argv);
  }
  catch (const std::exception& ex)
  {
    cerr << "Argument error: " << ex.what() << "\n";
    CLOSE_LIB(lib);
    return 1;
  }

  #if _DEBUG
  // Optionally init rust's tracing
  if (abi.InitTracing() != CABI_STATUS_SUCCESS)
  {
    cerr << "Failed to initialize tracing. Continuing without tracing\n";
  }
  #endif

  NodeHandle node;
  node.abi = &abi;

  std::atomic<bool> keepRunning(true);

  auto signalHandler = [](int) {
    // no-op placeholder to break getline
  };
  std::signal(SIGINT, signalHandler);

  try
  {
    // Step 4. Create node for this peer
    node.reset(createNode(abi, args.useQuic, false, args.bootstrapPeers));

    // Step 5. Try listen on provided addr
    auto status = abi.ListenNode(node.handle, args.listen.c_str());
    cout << "Listening on " << args.listen << "\n";
    if (status != CABI_STATUS_SUCCESS)
    {
      throw std::runtime_error("cabi_node_listen failed: " + statusMessage(status));
    }

    // Relay node behaviour
    if (args.role == Role::Relay)
    {
      std::chrono::seconds waitTime(10);
      cout << "Waiting up to " << waitTime.count() << "s for PUBLIC AutoNAT status before enabling relay hop...\n";
      
      // Step 6. Try understand wheter node is public or private
      // And if public, remake the node
      if (waitForPublicAutonat(abi, node.handle, waitTime))
      {
        cout << "AutoNAT is PUBLIC; restarting with relay hop enabled\n";
        node.reset();
        node.reset(createNode(abi, args.useQuic, true, args.bootstrapPeers));

        status = abi.ListenNode(node.handle, args.listen.c_str());
        cout << "Listening with hop relay on " << args.listen << "\n";
        if (status != CABI_STATUS_SUCCESS)
        {
          throw std::runtime_error("cabi_node_listen failed after hop restart: " + statusMessage(status));
        }
      }
      else
      {
        cout << "AutoNAT did not report PUBLIC within window; staying without hop\n";
      }
    }

    // Step 7. Initail dial to know active peers from bootstrap and target
    dialPeers(abi, node.handle, args.bootstrapPeers, "bootstrap");
    dialPeers(abi, node.handle, args.targetPeers, "target");

    std::thread receiver(
      recvLoop,
      std::cref(abi),
      node.handle,
      std::ref(keepRunning));

    // Step 8. Start sending loop
    sendLoop(abi, node.handle, keepRunning);

    keepRunning.store(false, std::memory_order_release);
    receiver.join();
  }
  catch (const std::exception& ex)
  {
    cerr << "Fatal error: " << ex.what() << "\n";
    keepRunning.store(false, std::memory_order_release);
    node.reset();
    CLOSE_LIB(lib);
    return 1;
  }

  node.reset();
  CLOSE_LIB(lib);
  return 0;
}