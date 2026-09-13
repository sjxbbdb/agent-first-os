# G4 Supervisor group/DAG reference evidence

命令：

```text
bash product/tests/supervisor-g4-group-test.sh
```

测试验证依赖服务必须在依赖进入 READY 后才能启动；显式 task-group freeze
阻止新的启动；故障/重启会撤销旧 generation 的 capability handle，并生成
新的 generation handle。输出：

```text
Supervisor G4 group/DAG/capability lifecycle OK
```

这是 host-side Supervisor reference 证据，尚未表示内核已经实现通用 task-group
调度或原生 capability 回收。
