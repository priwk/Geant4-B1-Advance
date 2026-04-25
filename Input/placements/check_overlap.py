import tkinter as tk
from tkinter import filedialog
import pandas as pd
import numpy as np
from scipy.spatial import cKDTree
import re
import time


def select_file():
    """打开文件选择对话框"""
    root = tk.Tk()
    root.withdraw()  # 隐藏主窗口
    filepath = filedialog.askopenfilename(
        title="请选择包含球体坐标的CSV/TXT文件",
        filetypes=[("CSV/TXT 数据文件", "*.csv *.txt"), ("所有文件", "*.*")]
    )
    return filepath


def get_closest_pair_same_type(coords):
    """使用 KDTree 极速寻找同类坐标中距离最近的两个点"""
    if len(coords) < 2:
        return None, float('inf')
    tree = cKDTree(coords)
    # k=2 表示寻找最近的 2 个点（第1个是自己距离为0，第2个就是最近的邻居）
    dists, indices = tree.query(coords, k=2)
    nearest_dists = dists[:, 1]

    min_idx1 = np.argmin(nearest_dists)
    min_idx2 = indices[min_idx1, 1]
    min_dist = nearest_dists[min_idx1]
    return (min_idx1, min_idx2), min_dist


def get_closest_pair_diff_type(coords1, coords2):
    """使用 KDTree 极速寻找两类不同坐标之间距离最近的点对"""
    if len(coords1) == 0 or len(coords2) == 0:
        return None, float('inf')
    tree1 = cKDTree(coords1)
    # 寻找 coords2 中每个点在 coords1 中的最近邻
    dists, indices = tree1.query(coords2, k=1)

    min_idx2_in_coords2 = np.argmin(dists)
    min_idx1_in_coords1 = indices[min_idx2_in_coords2]
    min_dist = dists[min_idx2_in_coords2]
    return (min_idx1_in_coords1, min_idx2_in_coords2), min_dist


def check_spheres_overlap():
    filepath = select_file()
    if not filepath:
        print("未选择任何文件，程序退出。")
        return

    print(f"正在读取文件: {filepath}")
    start_time = time.time()

    # 1. 提取球体半径 (支持灵活匹配空白符)
    bn_radius = None
    zns_radius = None

    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            if 'bnRadius_um' in line:
                match = re.search(r'bnRadius_um\s*=\s*([\d\.]+)', line)
                if match:
                    bn_radius = float(match.group(1))
            elif 'znsRadius_um' in line:
                match = re.search(r'znsRadius_um\s*=\s*([\d\.]+)', line)
                if match:
                    zns_radius = float(match.group(1))

            if bn_radius is not None and zns_radius is not None:
                break

    if bn_radius is None or zns_radius is None:
        print("❌ 错误：无法在文件中提取到完整的 bnRadius_um 或 znsRadius_um 参数。")
        return

    print(f"✅ 成功解析半径: BN = {bn_radius} μm, ZnS = {zns_radius} μm")

    # 2. 读取坐标数据
    try:
        df = pd.read_csv(filepath, comment='#',
                         sep=r'[,\s\t]+', engine='python')
        df = df.dropna(subset=['type', 'x_um', 'y_um', 'z_um'])
    except Exception as e:
        print(f"❌ 读取坐标数据时发生错误: {e}")
        return

    bn_coords = df[df['type'] == 'BN'][[
        'x_um', 'y_um', 'z_um']].values.astype(float)
    zns_coords = df[df['type'] == 'ZnS'][[
        'x_um', 'y_um', 'z_um']].values.astype(float)

    print(f"✅ 数据加载完成: BN数量 = {len(bn_coords)}, ZnS数量 = {len(zns_coords)}\n")

    # 3. 统计重叠数量并寻找最近对
    epsilon = 1e-6
    total_overlaps = 0

    print("🔍 [1/2] 正在统计重叠总数...")

    tree_bn = cKDTree(bn_coords) if len(bn_coords) > 0 else None
    tree_zns = cKDTree(zns_coords) if len(zns_coords) > 0 else None

    if tree_bn:
        count_bn = len(tree_bn.query_pairs(2 * bn_radius - epsilon))
        print(f"  --> [BN - BN] 内部重叠数: {count_bn} 对")
        total_overlaps += count_bn

    if tree_zns:
        count_zns = len(tree_zns.query_pairs(2 * zns_radius - epsilon))
        print(f"  --> [ZnS - ZnS] 内部重叠数: {count_zns} 对")
        total_overlaps += count_zns

    if tree_bn and tree_zns:
        bn_zns_matches = tree_bn.query_ball_tree(
            tree_zns, bn_radius + zns_radius - epsilon)
        count_mix = sum(len(matches) for matches in bn_zns_matches)
        print(f"  --> [BN - ZnS] 交叉重叠数: {count_mix} 对")
        total_overlaps += count_mix

    print("\n🔬 [2/2] 正在寻找每种组合中的【极限最近对】(Min Margin)...")

    # 分析 BN - BN 最近对
    if len(bn_coords) >= 2:
        pair, dist = get_closest_pair_same_type(bn_coords)
        margin = dist - (2.0 * bn_radius)
        status = "⚠️ 重叠" if margin < - \
            epsilon else ("🔵 相切" if abs(margin) <= epsilon else "✅ 分离")
        print(f"  [BN - BN] 最近对索引: {pair[0]} 与 {pair[1]}")
        print(
            f"            球心距离: {dist:.6f} μm | 边缘间距(margin): {margin:.6f} μm ({status})")

    # 分析 ZnS - ZnS 最近对
    if len(zns_coords) >= 2:
        pair, dist = get_closest_pair_same_type(zns_coords)
        margin = dist - (2.0 * zns_radius)
        status = "⚠️ 重叠" if margin < - \
            epsilon else ("🔵 相切" if abs(margin) <= epsilon else "✅ 分离")
        print(f"  [ZnS-ZnS] 最近对索引: {pair[0]} 与 {pair[1]}")
        print(
            f"            球心距离: {dist:.6f} μm | 边缘间距(margin): {margin:.6f} μm ({status})")

    # 分析 BN - ZnS 最近对
    if len(bn_coords) > 0 and len(zns_coords) > 0:
        pair, dist = get_closest_pair_diff_type(bn_coords, zns_coords)
        margin = dist - (bn_radius + zns_radius)
        status = "⚠️ 重叠" if margin < - \
            epsilon else ("🔵 相切" if abs(margin) <= epsilon else "✅ 分离")
        print(f"  [BN -ZnS] 最近对索引: BN[{pair[0]}] 与 ZnS[{pair[1]}]")
        print(
            f"            球心距离: {dist:.6f} μm | 边缘间距(margin): {margin:.6f} μm ({status})")

    # 4. 总结输出
    print("\n" + "=" * 50)
    if total_overlaps > 0:
        print(f"❌ 最终结论: 存在体积重叠！总共发现了 {total_overlaps} 处重叠。")
    else:
        print("✨ 最终结论: 完美！没有任何球体发生体积重叠。")
    print(f"⏱️ 整体耗时: {time.time() - start_time:.4f} 秒")
    print("=" * 50)


if __name__ == "__main__":
    check_spheres_overlap()
