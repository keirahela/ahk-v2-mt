## AutoHotkey v2 – Extended API (Threads + WebSockets)

This document describes the additional built-in functions available in your custom AutoHotkey v2 build. These differ from stock AHK v2 and are callable directly from scripts.

- Status: experimental
- Platform: Windows (x64)
- Networking: ws:// (non‑TLS)

### Quick Start

```ahk
; Create a worker thread that updates a shared var 3 times
tid := ThreadCreate("
(
i := 0
while (i < 3)
{
    ThreadSetVar('worker_msg', 'tick ' . i)
    Sleep(500)
    i := i + 1
}
)")

MsgBox("Started thread " . tid)
MsgBox("Active threads: " . ThreadCount())

; Read the shared var set by the worker
Sleep(1600)
MsgBox("worker_msg => " . ThreadGetVar("worker_msg"))

; Stop the worker
ok := ThreadDestroy(tid)
MsgBox("Stopped: " . ok)
```

---

## Multithreading API

### ThreadCreate(scriptText) → Integer threadId
Runs `scriptText` in a new OS thread with an isolated (experimental) interpreter.

- Parameters
  - `scriptText` (String): AutoHotkey v2 code to execute in the worker thread.
- Returns
  - `threadId` (Integer): A positive identifier for the new thread.
- Notes
  - Cooperative shutdown: the thread checks an internal stop flag periodically.
  - Unsupported functions inside threads cause the thread to set `thread_<id>_error` via `ThreadSetVar` and stop.
  - Currently supported inside workers (incremental): basic `while/if/else/end`, `Sleep`, simple expressions, `ThreadSetVar/ThreadGetVar`, `WebSocket*`.

Example:
```ahk
tid := ThreadCreate("
(
count := 0
while (count < 5) {
    ThreadSetVar('last_tick', count)
    Sleep(200)
    count := count + 1
}
)")
```

### ThreadDestroy(threadId) → Boolean success
Requests the worker to stop and joins the OS thread.

- Parameters
  - `threadId` (Integer): A value previously returned by `ThreadCreate`.
- Returns
  - `true` if the thread was found and shut down, otherwise `false`.

### ThreadCount() → Integer
Returns the number of currently active worker threads.

### ThreadSetVar(name, value) → Boolean success
Sets a process‑wide shared variable in a thread‑safe store.

- Parameters
  - `name` (String): Variable key.
  - `value` (String/Number): Value to store (stored as string internally).
- Returns
  - `true` on success.
- Notes
  - Accessible from both main script and worker threads.

### ThreadGetVar(name) → String valueOrEmpty
Gets a value previously set via `ThreadSetVar`.

- Parameters
  - `name` (String): Variable key.
- Returns
  - Stored string value, or empty string if missing.

---

## WebSocket Client API

Single global client instance (per process) using WinSock. Supports `ws://host:port` only.

### WebSocketConnect(url) → Boolean success
Opens a WebSocket connection.

- Parameters
  - `url` (String): e.g. `"ws://127.0.0.1:8080"`.
- Returns
  - `true` on successful handshake, else `false`.
- Notes
  - Frames sent are correctly client‑masked per RFC6455.
  - `wss://` (TLS) is not supported in this build.

### WebSocketSend(text) → Boolean success
Sends a text message.

- Parameters
  - `text` (String): Message payload.
- Returns
  - `true` on success.

### WebSocketReceive() → String textOrEmpty
Retrieves the next queued message.

- Returns
  - Next message as a string, or empty string if none available.

### WebSocketDisconnect() → Boolean success
Closes the current WebSocket connection.

---

## HTTP Utility

### HttpRequest(url) → String responseBody
Simple HTTP GET helper.

- Parameters
  - `url` (String): The GET endpoint.
- Returns
  - Response body as string (empty on failure).

---

## Behavior, Errors, and Limitations

- Interpreter in workers is incremental: not all AHK features are supported.
- Unsupported calls inside workers:
  - The thread stops and sets a shared error variable named `thread_<id>_error` with a brief reason.
- Shutdown is cooperative: long blocking operations in worker code can delay stop.
- WebSockets: one global connection; suitable for simple client use and demos.
- Networking: `ws://` only (no TLS). Use a local proxy/tunnel if TLS is needed.

---

## Examples

### 1) Basic thread lifecycle
```ahk
tid := ThreadCreate("
(
i := 0
while (i < 3) {
    ThreadSetVar('worker_msg', 'tick ' . i)
    Sleep(250)
    i := i + 1
}
)")

Sleep(900)
MsgBox("From worker: " . ThreadGetVar('worker_msg'))
ThreadDestroy(tid)
```

### 2) WebSocket echo
```ahk
if WebSocketConnect("ws://127.0.0.1:8080") {
    WebSocketSend("hello")
    Sleep(100)
    msg := WebSocketReceive()
    if (msg != "")
        MsgBox("Got: " . msg)
    WebSocketDisconnect()
}
```

### 3) WebSockets from a worker thread
```ahk
tid := ThreadCreate("
(
if WebSocketConnect('ws://127.0.0.1:8080') {
    WebSocketSend('hi from worker')
    Sleep(100)
    r := WebSocketReceive()
    ThreadSetVar('ws_reply', r)
    WebSocketDisconnect()
} else {
    ThreadSetVar('thread_' . A_ThreadId . '_error', 'ws connect failed')
}
)")

Sleep(300)
MsgBox("Reply: " . ThreadGetVar('ws_reply'))
ThreadDestroy(tid)
```

---

## Versioning

- Functions and behavior may change as the per‑thread interpreter matures.
- Keep scripts resilient to missing features by guarding with simple checks.

---

## Changelog (high level)

- Added: ThreadCreate, ThreadDestroy, ThreadCount
- Added: ThreadSetVar, ThreadGetVar (shared, thread‑safe)
- Added: WebSocketConnect/Send/Receive/Disconnect (ws:// only)
- Added: HttpRequest (GET)


