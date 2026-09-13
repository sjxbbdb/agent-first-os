# G6 native Ring3 file write

`product/tests/g6-native-file-write-test.sh` builds the BIOS image and boots
the real image under SeaBIOS/QEMU. The Ring3 agent submits a fixed write,
receives a service minted endpoint+nonce token, and the Ring3 service consumes
it before updating its fixed in-memory backend. The same token is replayed and
the service rejects it; the log also proves the scheduler reaches idle.

The evidence boundary is the host build plus QEMU guest serial output. It
verifies the ABI and fixture behavior in this emulated BIOS environment; it
does not claim a physical disk or production filesystem backend.

Command:

```sh
wsl.exe bash /mnt/d/Agent\ OS/product/tests/g6-native-file-write-test.sh
```
