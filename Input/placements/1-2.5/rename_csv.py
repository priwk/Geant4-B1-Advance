import os
import re
import glob


def get_seedbase(filepath):
    """提取CSV文件前20行中的seedBase值"""
    try:
        # 尝试优先用 utf-8 读取，如果遇到编码错误用 gbk
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                return _find_seed_in_lines(f)
        except UnicodeDecodeError:
            with open(filepath, 'r', encoding='gbk') as f:
                return _find_seed_in_lines(f)
    except Exception as e:
        print(f"无法读取文件 {filepath}: {e}")
    return None


def _find_seed_in_lines(file_obj):
    for _ in range(20):  # 限制只读取前20行寻找特征，提高效率
        line = file_obj.readline()
        if not line:
            break
        # 正则匹配类似于: # seedBase=1813364110
        match = re.search(r'#\s*seedBase=([^\s]+)', line)
        if match:
            return match.group(1)
    return None


def main():
    # 1. 程序开始时让用户确认前缀
    default_prefix = "placement_f_0.64_"
    print("=" * 50)
    user_input = input(
        f"请输入重命名文件的前缀 \n(直接按回车默认使用 '{default_prefix}'): ").strip()
    prefix = user_input if user_input else default_prefix
    print("=" * 50)

    # 2. 获取当前目录所有CSV文件
    csv_files = glob.glob("*.csv")
    if not csv_files:
        print("当前目录下没有找到CSV文件。")
        return

    seen_seeds = set()
    existing_numbers = set()

    # 构造正则表达式，用于匹配已经符合前缀格式的文件，例如 placement_f_0.64_0032.csv
    pattern = re.compile(rf"^{re.escape(prefix)}(\d+)\.csv$")

    other_files = []

    # 3. 第一遍扫描：优先处理已经符合命名规范的文件
    # 这步为了保留现有文件序号，并记录哪些seedBase和序号已被占用
    for f in csv_files:
        match = pattern.match(f)
        if match:
            seed = get_seedbase(f)
            if not seed:
                print(f"[-] 删除无seedBase的文件: {f}")
                os.remove(f)
            elif seed in seen_seeds:
                print(f"[-] 删除重复seedBase的文件: {f}")
                os.remove(f)
            else:
                seen_seeds.add(seed)
                num = int(match.group(1))
                existing_numbers.add(num)
        else:
            other_files.append(f)

    # 4. 第二遍扫描：处理格式不规范、或者是刚生成还没重命名的CSV文件
    files_to_rename = []
    for f in other_files:
        seed = get_seedbase(f)
        if not seed:
            print(f"[-] 删除无seedBase的文件: {f}")
            os.remove(f)
        elif seed in seen_seeds:
            print(f"[-] 删除重复seedBase的文件: {f}")
            os.remove(f)
        else:
            seen_seeds.add(seed)
            files_to_rename.append(f)

    # 5. 重命名剩余文件，智能填补空缺或向后排列
    current_num = 1
    renamed_count = 0

    for f in files_to_rename:
        # 寻找下一个可用的数字序号 (如果该数字已存在于 set 中，自增 1)
        while current_num in existing_numbers:
            current_num += 1

        # 生成新文件名 (04d表示填充至4位数字，例如0032)
        new_name = f"{prefix}{current_num:04d}.csv"
        print(f"[+] 重命名: {f} -> {new_name}")
        os.rename(f, new_name)

        # 将新序号加入集合，以便继续判断下一个遗漏值
        existing_numbers.add(current_num)
        renamed_count += 1

    print("=" * 50)
    print(
        f"处理完成！\n本次共重命名 {renamed_count} 个文件，保留了 {len(existing_numbers) - renamed_count} 个原有文件。")


if __name__ == "__main__":
    main()
