# 固定的 F407 依赖

本目录及 `Core/Startup/startup_stm32f407xx.s` 是官方源码的未修改子集，构建不联网，不使用 `.ci_check` 桩。

| 组件 | 固定版本 | commit | 许可证位置 |
|---|---|---|---|
| [ST HAL](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/tree/v1.8.3) | v1.8.3 | c2e1406d7ea4b73aa42b98ddeb75a8670b1a5a16 | STM32F4xx_HAL_Driver/LICENSE.md |
| [ST CMSIS Device](https://github.com/STMicroelectronics/cmsis-device-f4/tree/v2.6.10) | v2.6.10 | 5f41fb29d22773896c780052bf61e47fc924d524 | CMSIS/Device/ST/STM32F4xx/LICENSE.md |
| [Arm CMSIS Core](https://github.com/ARM-software/CMSIS_5/tree/5.9.0) | 5.9.0 | 2b7495b8535bdcb306dac29b9ded4cfb679d7e5c | CMSIS/LICENSE.txt |

`manifest.json` 记录每个文件的上游路径和 SHA-256。启动文件的许可证随 CMSIS Device 提供。发布二进制时随附这些许可证。

需要重新导入时，克隆上表三个仓库并 checkout 对应 commit，再执行：

```sh
python tools/vendor_f407.py <HAL-checkout> <device-checkout> <CMSIS_5-checkout>
python -m unittest discover -s tests -p test_vendor.py -v
```

导入器拒绝错误 commit 或带有本地修改的上游 checkout。仅导入 Makefile 使用的 HAL 模块及依赖头文件；CMSIS Core 头文件保持配套版本。`.gitattributes` 禁止 Git 改写这些文件的换行，以保持哈希稳定。

HAL Flash 扩展模块在 F407 单 bank 配置下有三个上游未使用的 `Banks` 参数，Makefile 仅对此对象禁用 `unused-parameter` 警告，其余项目源码保持 `-Wall -Wextra -Werror`。
