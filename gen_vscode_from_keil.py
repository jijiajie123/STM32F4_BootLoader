# -*- coding: utf-8 -*-
"""
===============================================================
从 Keil .uvprojx 文件自动生成 VSCode c_cpp_properties.json 配置脚本
===============================================================

【功能说明】
本脚本用于从 Keil 工程文件 (.uvprojx) 中自动提取：
    - 头文件包含路径 (IncludePath)
    - 宏定义 (Define)
并生成/更新 VSCode 的 IntelliSense 配置文件：
    .vscode/c_cpp_properties.json

可选功能：
    - 自动生成 .editorconfig 文件（用于统一代码风格）

【使用方法】
-----------------------------------------
1️⃣ 基本用法（在当前目录查找 .uvprojx 并生成配置）：
    python gen_vscode_from_keil.py

2️⃣ 指定 Keil 工程目录：
    python gen_vscode_from_keil.py -s ./firmware

3️⃣ 指定输出 VSCode 配置目录（默认是当前目录下的 .vscode）：
    python gen_vscode_from_keil.py -v ./vscode_config

4️⃣ 同时生成 .editorconfig 文件：
    python gen_vscode_from_keil.py --create-editorconfig

5️⃣ 如果你想在 c_cpp_properties.json 不存在时创建一个默认配置：
    python gen_vscode_from_keil.py --create-default-config

6️⃣ 指定配置名称（默认取 .uvprojx 文件名）：
    python gen_vscode_from_keil.py --config-name MyProject

示例：
    python gen_vscode_from_keil.py -s D:/STM32_Project --create-editorconfig

【输出结果】
-----------------------------------------
执行完成后生成：
    .vscode/c_cpp_properties.json
    （可选）.editorconfig
===============================================================
"""

import os
import xml.etree.ElementTree as ET
import json
import re
import argparse
from pathlib import Path
import logging

# ====== 全局可配置项 ======
# 修改此处即可更换编译器路径
COMPILER_PATH = "C:/Users/25799/Documents/gcc-arm-none-eabi-10.3-2021.10/bin/arm-none-eabi-gcc.exe"
# =========================

# .editorconfig 文件的标准内容
EDITORCONFIG_CONTENT = """# EditorConfig is awesome: https://EditorConfig.org

# top-most EditorConfig file
root = true

[*]
indent_style = space
indent_size = 4
end_of_line = crlf
trim_trailing_whitespace = true
insert_final_newline = true
"""

# 日志初始化
logging.basicConfig(
    level=logging.INFO,
    format='[%(levelname)s] %(message)s'
)

def log_info(msg: str) -> None:
    logging.info(msg)

def log_warning(msg: str) -> None:
    logging.warning(msg)

def log_error(msg: str) -> None:
    logging.error(msg)

def safe_read_json(path: str) -> dict:
    try:
        with open(path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except Exception as e:
        log_error(f"读取 JSON 文件失败: {e}")
        return {}

def safe_write_json(path: str, data: dict) -> None:
    try:
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=4, ensure_ascii=False)
        log_info(f"成功写入 JSON 文件: {path}")
    except Exception as e:
        log_error(f"写入 JSON 文件失败: {e}")

def generate_vscode_config_from_file(input_filepath, config_name):
    """
    从 Keil .uvprojx 文件读取配置，返回提取到的 includePath 和 defines。
    """
    def normalize_and_clean_path(path):
        path = path.replace('\\', '/')
        while path.startswith('../'):
            path = path[3:]
        return path

    try:
        tree = ET.parse(input_filepath)
        root = tree.getroot()
        cads_node = root.find('.//Cads')
        if cads_node is None:
            return None

        extracted_data = {"includePath": [], "defines": []}

        path_node = cads_node.find('.//VariousControls/IncludePath')
        if path_node is not None and path_node.text:
            paths = [normalize_and_clean_path(p.strip()) for p in path_node.text.split(';') if p.strip()]
            extracted_data['includePath'] = paths

        define_node = cads_node.find('.//VariousControls/Define')
        if define_node is not None and define_node.text:
            defines = [d.strip() for d in define_node.text.split(',') if d.strip()]
            extracted_data['defines'] = defines

        return extracted_data

    except FileNotFoundError:
        print(f"错误: 文件 '{input_filepath}' 不存在。")
        return None
    except ET.ParseError as e:
        print(f"错误: XML 解析失败 - {e}")
        return None
    except Exception as e:
        print(f"发生未知错误: {e}")
        return None

def ensure_vscode_c_cpp_properties(path, create_default=True):
    vscode_dir = Path(path)
    vscode_dir.mkdir(parents=True, exist_ok=True)
    c_cpp_path = vscode_dir / 'c_cpp_properties.json'
    if not c_cpp_path.exists():
        if create_default:
            configurations = [
                {
                    "name": "Default",
                    "intelliSenseMode": "linux-gcc-arm",
                    "compilerPath": COMPILER_PATH,
                    "cStandard": "c99",
                    "cppStandard": "c++11",
                    "includePath": [],
                    "defines": []
                }
            ]
        else:
            configurations = []
        template = {"configurations": configurations, "version": 4}
        with open(c_cpp_path, 'w', encoding='utf-8') as f:
            json.dump(template, f, indent=4, ensure_ascii=False)
    return str(c_cpp_path)

def update_c_cpp_properties(new_data, output_filepath, config_name):
    if not os.path.exists(output_filepath):
        print(f"错误: 文件 '{output_filepath}' 不存在。请先创建一个基础配置 JSON 文件。")
        return

    try:
        with open(output_filepath, 'r', encoding='utf-8') as f:
            content = json.load(f)

        if 'configurations' not in content or not isinstance(content['configurations'], list):
            print("警告: 缺少 'configurations' 列表，无法更新。")
            return

        target_config = None
        for config in content['configurations']:
            if config.get('name') == config_name:
                target_config = config
                break

        if target_config is None:
            print(f"警告: 未找到 '{config_name}'，将创建新配置。")
            target_config = {
                "name": config_name,
                "intelliSenseMode": "linux-gcc-arm",
                "compilerPath": COMPILER_PATH,  # ✅ 使用全局路径
                "cStandard": "c99",
                "cppStandard": "c++11"
            }
            content['configurations'].append(target_config)

        # ✅ 修正编译器路径 & 更新配置内容
        target_config['compilerPath'] = COMPILER_PATH
        target_config['includePath'] = new_data.get('includePath', [])
        target_config['defines'] = new_data.get('defines', [])

        with open(output_filepath, 'w', encoding='utf-8') as f:
            json.dump(content, f, indent=4, ensure_ascii=False)

        print(f"成功更新 c_cpp_properties.json 中配置 '{config_name}'。")

    except json.JSONDecodeError:
        print(f"错误: JSON 格式不正确。")
    except Exception as e:
        print(f"更新文件时发生未知错误: {e}")

def write_editorconfig_file(output_dir):
    editorconfig_path = os.path.join(output_dir, '.editorconfig')
    try:
        if os.path.exists(editorconfig_path):
            print(f"文件 '{editorconfig_path}' 已存在，跳过生成。")
            return
        with open(editorconfig_path, 'w', encoding='utf-8') as f:
            f.write(EDITORCONFIG_CONTENT)
        print(f"成功生成文件: {editorconfig_path}")
    except Exception as e:
        print(f"生成 .editorconfig 文件时发生错误: {e}")

def find_uvprojx_files(start_dir):
    uvprojx_files = []
    for root, _, files in os.walk(start_dir):
        for file in files:
            if file.endswith('.uvprojx'):
                uvprojx_files.append(os.path.join(root, file))
    return uvprojx_files

def find_first_uvprojx(src_dir: str) -> str | None:
    files = find_uvprojx_files(src_dir)
    if not files:
        log_warning(f"在目录 '{src_dir}' 未找到任何 .uvprojx 文件。")
        return None
    return files[0]

def parse_keil_config(uvprojx_path: str, config_name: str) -> dict | None:
    data = generate_vscode_config_from_file(uvprojx_path, config_name)
    if not data:
        log_error(f"解析 Keil 工程文件失败: {uvprojx_path}")
        return None
    return data

def ensure_vscode_config(vscode_dir: str, create_default: bool) -> str:
    return ensure_vscode_c_cpp_properties(vscode_dir, create_default=create_default)

def update_vscode_config(data: dict, config_path: str, config_name: str) -> None:
    update_c_cpp_properties(data, config_path, config_name)

def main():
    parser = argparse.ArgumentParser(
        description='从 Keil .uvprojx 生成或更新 VSCode c_cpp_properties.json',
        formatter_class=argparse.RawTextHelpFormatter
    )
    script_dir = Path(__file__).parent.resolve()
    parser.add_argument('--src-dir', '-s', type=str, default=str(script_dir), help='搜索 .uvprojx 的起始目录。')
    parser.add_argument('--vscode-dir', '-v', type=str, default=str(script_dir / '.vscode'), help='目标 .vscode 目录。')
    parser.add_argument('--create-editorconfig', action='store_true', help='生成 .editorconfig 文件。')
    parser.add_argument('--config-name', help='指定配置名称。')
    parser.add_argument('--create-default-config', action='store_true', help='若不存在 c_cpp_properties.json，则创建默认配置。')
    args = parser.parse_args()

    if args.create_editorconfig:
        write_editorconfig_file(args.src_dir)

    uvprojx_path = find_first_uvprojx(args.src_dir)
    if not uvprojx_path:
        return
    config_name = args.config_name or Path(uvprojx_path).stem
    log_info(f"解析文件: {uvprojx_path}")
    log_info(f"目标配置名称: {config_name}")

    data = parse_keil_config(uvprojx_path, config_name)
    if not data:
        return

    vscode_dir = args.vscode_dir
    Path(vscode_dir).mkdir(exist_ok=True)
    config_path = ensure_vscode_config(vscode_dir, args.create_default_config)
    update_vscode_config(data, config_path, config_name)
    log_info("全部流程完成！")

if __name__ == "__main__":
    main()
