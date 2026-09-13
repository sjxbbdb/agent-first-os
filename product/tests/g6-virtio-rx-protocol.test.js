"use strict";
const assert = require("assert");
const { validateRxBuffer } = require("../services/virtio_rx_protocol");
const valid = { version: 1, queue: 0, descriptor: 2, capability_handle: "rx:1:2", capability_generation: 3, address: 0x400000, length: 1514 };
assert.strictEqual(validateRxBuffer(valid).capability_generation, 3);
for (const bad of [
  { ...valid, version: 2 }, { ...valid, queue: 1 }, { ...valid, capability_generation: 2.5 },
  { ...valid, capability_handle: "" }, { ...valid, length: 1515 }, { ...valid, address: -1 },
]) assert.throws(() => validateRxBuffer(bad), /invalid virtio RX/);
console.log("G6 virtio RX buffer handoff contract OK");
