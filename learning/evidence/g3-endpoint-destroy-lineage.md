# G3 endpoint destroy lineage evidence

审查发现 dynamic endpoint destroy previously retired only the destroying
process's capability. A transferred sibling handle still named the endpoint
object; reusing the same bounded endpoint pool slot could therefore make the
old sibling point at a new endpoint.

The minimal fix adds `agent_os_capability_revoke_object` and makes native
`SYS_IPC_DESTROY` retire matching entries in every live process capability
table. Existing parent-lineage validation remains in place, so descendants
whose object entry is not directly scanned also fail through their retired
parent.

定向红证据：旧实现链接不了 object-wide revoke test；代码审查同时 showed
source-only revoke left transferred sibling active. 绿证据：

```text
G3 endpoint destroy lineage OK: source and transferred sibling handles retired
```

验证命令：

```text
bash product/tests/g3-endpoint-destroy-lineage-test.sh
```

该 host test mint/transfer 一个 endpoint object，destroy 时分别撤销 source
和 sibling table 的 handle，然后复用同一 object 地址确认新 handle 的
generation 与旧 handle 不同。QEMU G3 endpoint close/destroy 全链路未在本轮
重复运行；host evidence 不代表完整 native lifecycle coverage。
