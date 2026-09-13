"use strict";

/*
 * Host reference for the G6 Ring 3 service boundary.
 *
 * This deliberately models the message/capability contract without touching
 * PCI or DMA.  The native driver remains responsible for validated buffers;
 * this fixture makes the service behavior testable before native service
 * processes and a real IPC transport are available.
 */

const SERVICE = Object.freeze({
  FILE: 2,
  NETWORK: 3,
  INPUT: 4,
  WINDOW: 5,
});

const OPCODE = Object.freeze({
  FILE_READ: 0x0201,
  FILE_WRITE: 0x0202,
  FILE_FLUSH: 0x0203,
  NETWORK_SEND: 0x0301,
  NETWORK_RECEIVE: 0x0302,
  INPUT_POLL: 0x0401,
  WINDOW_DELIVER: 0x0501,
});

const STATUS = Object.freeze({
  OK: 0,
  BAD_REQUEST: 1,
  IO: 2,
  NO_DEVICE: 3,
  RESET: 4,
  DENIED: 6,
  REPLAY: 11,
});

const MAX_PAYLOAD = 4096;
const MAX_FRAME = 1514;
const ABI_VERSION = 1;

class ServiceError extends Error {
  constructor(code, message) {
    super(message);
    this.name = "ServiceError";
    this.code = code;
  }
}

function fail(code, message) {
  throw new ServiceError(code, message);
}

function integer(value, name) {
  if (!Number.isSafeInteger(value) || value < 0) fail(STATUS.BAD_REQUEST, `${name} must be a non-negative integer`);
  return value;
}

function asBytes(value, name) {
  if (Buffer.isBuffer(value)) return Buffer.from(value);
  if (value instanceof Uint8Array) return Buffer.from(value);
  if (typeof value === "string") return Buffer.from(value, "utf8");
  fail(STATUS.BAD_REQUEST, `${name} must be bytes`);
}

class VirtioServiceRuntime {
  constructor({ disk = Buffer.alloc(0), maxPayload = MAX_PAYLOAD } = {}) {
    this.disk = Buffer.from(disk);
    this.maxPayload = maxPayload;
    this.capabilities = new Map();
    this.inputQueue = [];
    this.networkQueue = [];
    this.windowEvents = [];
    this.completed = new Set();
  }

  grant({ handle, service, rights, generation = 1 }) {
    if (typeof handle !== "string" || handle.length === 0) fail(STATUS.BAD_REQUEST, "capability handle required");
    if (!Object.values(SERVICE).includes(service)) fail(STATUS.BAD_REQUEST, "unknown service capability");
    if (!Array.isArray(rights) || rights.length === 0) fail(STATUS.BAD_REQUEST, "capability rights required");
    this.capabilities.set(handle, { service, rights: new Set(rights), generation });
  }

  revoke(handle) {
    return this.capabilities.delete(handle);
  }

  enqueueInput(event) {
    if (!event || !Number.isInteger(event.event_type) || !Number.isInteger(event.event_code) ||
        !Number.isInteger(event.value) || !Number.isSafeInteger(event.timestamp_ns)) {
      fail(STATUS.BAD_REQUEST, "malformed input event");
    }
    this.inputQueue.push({ ...event });
  }

  request(request) {
    this.validateRequest(request);
    const { service_id: service, opcode, request_id: id } = request;
    if (this.completed.has(id)) fail(STATUS.REPLAY, "request id already completed");
    const cap = this.capabilities.get(request.resource && request.resource.handle);
    if (!cap || cap.service !== service) fail(STATUS.DENIED, "capability is absent or bound to another service");
    if (request.resource.generation !== undefined && request.resource.generation !== cap.generation) {
      fail(STATUS.DENIED, "stale capability generation");
    }
    const required = this.requiredRight(opcode);
    if (!cap.rights.has(required)) fail(STATUS.DENIED, `capability lacks ${required} right`);
    let response;
    switch (opcode) {
      case OPCODE.FILE_READ: response = this.fileRead(request); break;
      case OPCODE.FILE_WRITE: response = this.fileWrite(request); break;
      case OPCODE.FILE_FLUSH: response = this.fileFlush(request); break;
      case OPCODE.NETWORK_SEND: response = this.networkSend(request); break;
      case OPCODE.NETWORK_RECEIVE: response = this.networkReceive(request); break;
      case OPCODE.INPUT_POLL: response = this.inputPoll(request); break;
      case OPCODE.WINDOW_DELIVER: response = this.windowDeliver(request); break;
      default: fail(STATUS.BAD_REQUEST, "unsupported service opcode");
    }
    this.completed.add(id);
    return { version: ABI_VERSION, service_id: service, opcode, request_id: id, ...response };
  }

  validateRequest(request) {
    if (!request || request.version !== ABI_VERSION || request.size !== 56) fail(STATUS.BAD_REQUEST, "invalid service ABI header");
    integer(request.request_id, "request_id");
    if (request.request_id === 0) fail(STATUS.BAD_REQUEST, "request_id must be non-zero");
    if (!Object.values(SERVICE).includes(request.service_id)) fail(STATUS.BAD_REQUEST, "unknown service id");
    integer(request.payload_length || 0, "payload_length");
    if ((request.payload_length || 0) > this.maxPayload) fail(STATUS.BAD_REQUEST, "payload exceeds service limit");
    const actual = this.expectedPayloadLength(request);
    if (actual > this.maxPayload || actual !== (request.payload_length || 0)) fail(STATUS.BAD_REQUEST, "payload length mismatch");
  }

  expectedPayloadLength(request) {
    const payload = request.payload;
    switch (request.opcode) {
      case OPCODE.FILE_READ: return payload && Number.isInteger(payload.offset) && Number.isInteger(payload.length) ? 16 : 0;
      case OPCODE.FILE_WRITE: return payload && payload.data !== undefined ? 16 + asBytes(payload.data, "data").length : 0;
      case OPCODE.NETWORK_SEND: return payload && payload.frame !== undefined ? asBytes(payload.frame, "frame").length : 0;
      case OPCODE.WINDOW_DELIVER: return payload && payload.event ? 16 : 0;
      default: return 0;
    }
  }

  requiredRight(opcode) {
    if (opcode === OPCODE.FILE_READ || opcode === OPCODE.NETWORK_RECEIVE || opcode === OPCODE.INPUT_POLL) return "read";
    return "write";
  }

  fileRead(request) {
    const range = request.payload || {};
    integer(range.offset, "offset"); integer(range.length, "length");
    if (range.length > this.maxPayload || range.offset + range.length > this.disk.length) fail(STATUS.IO, "file range outside device");
    const data = Buffer.from(this.disk.subarray(range.offset, range.offset + range.length));
    return { status: STATUS.OK, payload: data, payload_length: data.length, bytes_transferred: data.length };
  }

  fileWrite(request) {
    const range = request.payload || {};
    integer(range.offset, "offset");
    const data = asBytes(range.data, "data");
    if (data.length > this.maxPayload || range.offset + data.length > this.disk.length) fail(STATUS.IO, "file range outside device");
    data.copy(this.disk, range.offset);
    return { status: STATUS.OK, payload_length: 0, bytes_transferred: data.length };
  }

  fileFlush() {
    return { status: STATUS.OK, payload_length: 0, bytes_transferred: 0 };
  }

  networkSend(request) {
    const frame = asBytes(request.payload && request.payload.frame, "frame");
    if (frame.length === 0 || frame.length > MAX_FRAME) fail(STATUS.BAD_REQUEST, "invalid Ethernet frame length");
    this.networkQueue.push(Buffer.from(frame));
    return { status: STATUS.OK, payload_length: 0, bytes_transferred: frame.length };
  }

  networkReceive() {
    const frame = this.networkQueue.shift() || Buffer.alloc(0);
    return { status: STATUS.OK, payload: frame, payload_length: frame.length, bytes_transferred: frame.length };
  }

  inputPoll() {
    const event = this.inputQueue.shift() || null;
    const length = event ? 16 : 0;
    return { status: STATUS.OK, payload: event, payload_length: length, bytes_transferred: length };
  }

  windowDeliver(request) {
    const event = request.payload && request.payload.event;
    if (!event || !Number.isInteger(event.event_type) || !Number.isInteger(event.event_code) ||
        !Number.isInteger(event.value) || !Number.isSafeInteger(event.timestamp_ns)) fail(STATUS.BAD_REQUEST, "malformed window event");
    this.windowEvents.push({ ...event });
    return { status: STATUS.OK, payload_length: 0, bytes_transferred: 16 };
  }
}

module.exports = { VirtioServiceRuntime, SERVICE, OPCODE, STATUS, MAX_PAYLOAD, MAX_FRAME, ServiceError };
