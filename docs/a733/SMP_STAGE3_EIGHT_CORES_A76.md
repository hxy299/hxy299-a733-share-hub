# SMP 阶段 3：六 A55 + 两 A76（v85 候选）

前置恢复点为 `a733-smp-6a55-v84-basic-verified` 与 v84 镜像。
本阶段 CPU 数改为 8、默认 CPUSET=`0xff`；CPU0..5 为 A55，CPU6/7 为
A76。继续使用 TF-A PSCI CPU_ON，不直接修改 CPU 电源/PLL/PMIC。

新增 `aipetllm cpucheck` 仅做逐核亲和迁移、MPIDR/MIDR 读取和相同的
10 万步整数校验，结束后恢复任务原有亲和掩码。它不会改变 LLM 并行策略，
也不是生成速度基准。CPU6/7 必须实际执行此命令并读到 MIDR part=`d0b`，
才能确认 A76 执行检查通过；不能只看八个 IDLE。

实机命令：

```text
ps
aipetllm cpucheck
time "sleep 5"
dd if=/dev/mmcsd0 of=/dev/null bs=65536 count=16
echo selftest > /dev/npu0
echo apitest > /dev/npu0
cat /dev/npu0
wifi connect <SSID>
ifconfig
ping -c 20 <router-IP>
ps
dmesg
```

逐核 checkpoint 全部为 0，六行 A55、两行 A76，其他基础回归正常后，
才进入 A76 优先的 LLM 工作线程、权重内存缓存与持久 KV cache 优化。
当前不承诺频率、实际 tok/s 或八核长期稳定性。失败时保留完整日志并回退
v84；不得覆盖任何既有镜像。
