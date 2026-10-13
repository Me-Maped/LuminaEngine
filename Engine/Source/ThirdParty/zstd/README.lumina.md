# zstd

上游：https://github.com/facebook/zstd，release v1.5.7。

`zstd.c` 是上游自己的单文件 amalgamation，由其脚本原样生成，未经手工编辑。若要刷新它，
请取 release tarball 并运行：

```
cd zstd-<version>/build/single_file_libs
python3 combine.py -r ../../lib -x legacy/zstd_legacy.h -o zstd.c zstd-in.c
```

然后把生成的 `zstd.c` 连同 `lib/zstd.h`、`lib/zstd_errors.h` 和 `LICENSE` 一起复制到这里。
注意这里没有使用 `-k zstd.h`，否则会留下一个 `../zstd.h` 的 include，而此处扁平的目录布局
无法满足它；不使用它时，该头文件会被内联进 amalgamation，而位于其旁的 `zstd.h` 副本则供引擎使用。

amalgamation 固化了两个值得了解的点。它设置了 `ZSTD_DISABLE_ASM`，因为汇编版 Huffman 解码器
无法被 amalgamation，而且构建工具本来也不汇编 `.S` 文件；出于这一原因，解码会比上游构建稍慢。
它还定义了 `ZSTD_MULTITHREAD`，这在默认零 worker 时是不起作用的，并且是有意保持这种状态：
package 容器已经会切分成块并通过 Task::ParallelFor 压缩它们，因此 zstd 自带的线程机制
会与引擎争夺同样的核心。
