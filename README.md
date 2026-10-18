# Licensebox for C++17

A C++ RAII/exception wrapper around the shared [C SDK](https://github.com/licensebox/c).
Machine identity, signature verification, storage and policy handling have one
implementation. Linux is supported and runtime-validated; other OS adapters are
pending in the C implementation.

Install the C SDK's dependencies (OpenSSL 3, libcurl 7.85+, json-c), then:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /your/install/prefix
```

CMake finds an installed C SDK or fetches its pinned release. For local
development pass `-DLICENSEBOX_C_SOURCE_DIR=/path/to/c`. Consumers use
`find_package(licensebox-cpp CONFIG REQUIRED)` and link `licensebox::cpp`.

```cpp
#include <licensebox/client.hpp>

void run_export() {
    licensebox::Config options;
    options.product = "myapp";
    options.keys = {{"myapp-k1", "YOUR_PUBLIC_VERIFICATION_KEY"}};
    options.version = "1.0";
    licensebox::Client client(std::move(options));
    client.require("export"); // throws licensebox::Error if access is refused
    // Read/process user files only after this succeeds.
}
```

Keep one client per process. `lease`, `status` (local only), `sign_in`, `logout`,
`modules`, `request_access`, `requests`, `access_link` and `record` expose the
application operations. Lease/module/request results are owned JSON strings;
access links are private URLs. `Error.code` provides the stable error string,
`Error.result` the C result enum. Do not catch a refusal and continue protected
work. Callback exceptions are captured safely across the C boundary and
rethrown to the caller. Callbacks must not reenter the client.

Use product/module IDs and **public verification keys** from the administrator.
Private signing keys and admin tokens must never be embedded. The Go admin CLI
remains the administration tool. Licensebox does not handle payments or billing.

Signed leases renew silently; cached access lasts until expiry if connectivity
fails. Machine/seat/location refusals persist offline. Copying credentials to a
different native identity is rejected. Raw native IDs remain local; this is not
hardware attestation. Cloned/spoofed IDs and modified host software remain
limitations. No background heartbeat is provided.

See [protocol/conformance](https://github.com/licensebox/protocol) and
[Licensebox documentation](https://licensebox.dev/docs).

MIT. Copyright © 2026 [Vijai Kumar](https://meshing.engineer).

## Optional protected-work checkpoints

```cpp
auto checkpoint = client.heartbeat("core");
// Authorize the next protected unit only if no exception was raised.
```

The result is the owned JSON value/string `{"lease":{...},"online":true|false}`
using this binding's existing JSON and cleanup conventions.

The explicit checkpoint always attempts signed renewal with a three-second
network bound, even after a cached `Require` succeeded. It never activates,
opens interactive sign-in, creates/rotates an installation key or transfers a
seat. Missing saved state returns `not_signed_in`; invalid saved state, a removed
module or explicit server refusal fails closed. Eligible outages may return a
verified unexpired saved lease with `online=false`, without extending its signed
expiry or clearing a saved refusal. A successful renewal replaces cached grants
before checking the requested nonempty module.

Use the matching native core containing the heartbeat API. Consumers own their
event loop and safe stop/pause behavior. Read the optional verified `heartbeat`
policy (`interval_seconds`, `grace_seconds`) to schedule checks; missing/null is
legacy policy. Independently stop protected work by the last verified `expires`
even when renewal is slow, and never move that deadline after a failed check.
Construction, Lease and Require start no background worker. Callback lifetime,
thread ownership and reentry requirements remain unchanged; the SDK never kills
the application or discards application results.
