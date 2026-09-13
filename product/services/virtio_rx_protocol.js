"use strict";

const MAX_FRAME = 1514;
function validateRxBuffer(value) {
  if (!value || value.version !== 1 || !Number.isSafeInteger(value.queue) || value.queue !== 0 ||
      !Number.isSafeInteger(value.descriptor) || value.descriptor < 0 ||
      typeof value.capability_handle !== "string" || value.capability_handle.length === 0 ||
      !Number.isSafeInteger(value.capability_generation) || value.capability_generation < 1 ||
      !Number.isSafeInteger(value.address) || value.address < 0 ||
      !Number.isSafeInteger(value.length) || value.length < 14 || value.length > MAX_FRAME) {
    throw new TypeError("invalid virtio RX buffer descriptor");
  }
  return Object.freeze({ ...value });
}
module.exports = { MAX_FRAME, validateRxBuffer };
