"use strict";

const assert = require("node:assert/strict");
const {
  VirtioServiceRuntime,
  SERVICE,
  OPCODE,
  STATUS,
  ServiceError,
} = require("../services/virtio_service_runtime");

function request(service_id, opcode, request_id, handle, payload, payload_length) {
  return {
    version: 1,
    size: 56,
    flags: 0,
    service_id,
    opcode,
    request_id,
    resource: { handle },
    payload,
    payload_length,
  };
}

function fails(fn, code) {
  assert.throws(fn, (error) => error instanceof ServiceError && error.code === code);
}

function main() {
  const disk = Buffer.alloc(4096, 0);
  disk.set(Buffer.from("hello"), 128);
  const runtime = new VirtioServiceRuntime({ disk });
  runtime.grant({ handle: "cap.file.g1", service: SERVICE.FILE, rights: ["read", "write"] });
  runtime.grant({ handle: "cap.net.g1", service: SERVICE.NETWORK, rights: ["read", "write"] });
  runtime.grant({ handle: "cap.input.g1", service: SERVICE.INPUT, rights: ["read"] });
  runtime.grant({ handle: "cap.window.g1", service: SERVICE.WINDOW, rights: ["write"] });

  const read = runtime.request(request(SERVICE.FILE, OPCODE.FILE_READ, 1, "cap.file.g1", { offset: 128, length: 5 }, 16));
  assert.equal(read.status, STATUS.OK);
  assert.equal(read.payload.toString(), "hello");
  assert.equal(read.bytes_transferred, 5);

  const data = Buffer.from("agent");
  const write = runtime.request(request(SERVICE.FILE, OPCODE.FILE_WRITE, 2, "cap.file.g1", { offset: 128, data }, 16 + data.length));
  assert.equal(write.status, STATUS.OK);
  assert.equal(runtime.disk.subarray(128, 133).toString(), "agent");
  assert.equal(runtime.request(request(SERVICE.FILE, OPCODE.FILE_FLUSH, 3, "cap.file.g1", undefined, 0)).status, STATUS.OK);

  const frame = Buffer.from([0, 1, 2, 3]);
  assert.equal(runtime.request(request(SERVICE.NETWORK, OPCODE.NETWORK_SEND, 4, "cap.net.g1", { frame }, frame.length)).bytes_transferred, frame.length);
  const received = runtime.request(request(SERVICE.NETWORK, OPCODE.NETWORK_RECEIVE, 5, "cap.net.g1", undefined, 0));
  assert.deepEqual(received.payload, frame);

  const event = { event_type: 1, event_code: 30, value: 1, timestamp_ns: 99 };
  runtime.enqueueInput(event);
  const input = runtime.request(request(SERVICE.INPUT, OPCODE.INPUT_POLL, 6, "cap.input.g1", undefined, 0));
  assert.deepEqual(input.payload, event);
  assert.equal(runtime.request(request(SERVICE.WINDOW, OPCODE.WINDOW_DELIVER, 7, "cap.window.g1", { event }, 16)).status, STATUS.OK);
  assert.deepEqual(runtime.windowEvents, [event]);

  fails(() => runtime.request(request(SERVICE.FILE, OPCODE.FILE_READ, 8, "cap.file.g1", { offset: 0, length: 5000 }, 16)), STATUS.IO);
  fails(() => runtime.request(request(SERVICE.NETWORK, OPCODE.NETWORK_SEND, 9, "cap.file.g1", { frame }, frame.length)), STATUS.DENIED);
  fails(() => runtime.request(request(SERVICE.FILE, OPCODE.FILE_READ, 10, "cap.file.g1", { offset: 0, length: 1 }, 15)), STATUS.BAD_REQUEST);
  const stale = request(SERVICE.FILE, OPCODE.FILE_READ, 12, "cap.file.g1", { offset: 0, length: 1 }, 16);
  stale.resource.generation = 0;
  fails(() => runtime.request(stale), STATUS.DENIED);
  fails(() => runtime.request(request(SERVICE.FILE, OPCODE.FILE_READ, 1, "cap.file.g1", { offset: 128, length: 5 }, 16)), STATUS.REPLAY);
  assert.equal(runtime.revoke("cap.file.g1"), true);
  fails(() => runtime.request(request(SERVICE.FILE, OPCODE.FILE_READ, 11, "cap.file.g1", { offset: 0, length: 1 }, 16)), STATUS.DENIED);

  console.log("G6 Ring3 file/network/input/window service fixture: PASS");
  console.log("evidence: bounded IPC-shaped requests, capability binding, revoke, replay, and malformed/range rejection");
  console.log("boundary: host reference only; no native Ring3 process or full virtio network/input queue claim");
}

main();
