#!/usr/bin/env python
# save_as: csv2soc.py
import pandas as pd, sys, pathlib as pl

src = pl.Path(sys.argv[1])                     # 原始 csv
dst = src.with_suffix(".soc.csv")              # 输出文件名 = xxx.soc.csv

df  = pd.read_csv(src)

out = pd.DataFrame({
    "t" : df["time_s"],            # ← 已有秒单位，可直接用
    "I" : df["current_A"],
    "V" : df["电压(V)"],
    # 若有 SOC 标注，可顺带输出看看误差
    "gt": df.get("SOC", None)      # 没有就自动全 NaN
})
out.to_csv(dst, index=False, float_format="%.6f")
print("✓ 导出", dst)
