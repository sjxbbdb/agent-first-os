# G3 capability lineage 证据

命令：

```text
bash product/tests/g3-cap-lineage-test.sh
```

结果：通过 host C 运行证据。`restrict` 与跨表 `transfer` 保存父表 incarnation、父句柄和有限深度；祖先撤销使所有后代失效，单分支撤销不影响 sibling，独立表中同一 object 的句柄不受影响。表重初始化、generation 最大值退休和超过 64 层的派生也 fail closed。

这是 capability 元数据与撤销语义的 host 证据，不宣称已经覆盖硬件对象回收、并发读写或完整 native task-group 生命周期。
