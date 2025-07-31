#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
plot_soc.py
-----------
读取 *_pred.soc.csv，绘制实际 SOC(g t) 与 预测 SOC(pred) 曲线
"""
import sys, pathlib as pl, pandas as pd, matplotlib.pyplot as plt

if len(sys.argv) < 2:
    print("用法: python plot_soc.py record_labeled_不同电流电压_pred.soc.csv")
    sys.exit(0)

csv_path = pl.Path(sys.argv[1])
df = pd.read_csv(csv_path)

plt.figure(figsize=(10, 4))
if "gt" in df.columns and df["gt"].notna().any():
    plt.plot(df["t"], df["gt"],  label="Ground‑Truth SOC", linewidth=1.2)
plt.plot(df["t"], df["pred"],    label="Predicted SOC",   linewidth=1.2, linestyle="--")

plt.xlabel("Time / s")
plt.ylabel("SOC (0–1)")
plt.title(csv_path.name)
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.show()
