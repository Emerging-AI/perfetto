import os
import sys
import logging
import shutil

# 获取当前脚本目录
SCRIPT_DIR = os.path.realpath(os.path.dirname(__file__))
BUILDTOOLS_DIR = os.path.dirname(SCRIPT_DIR)

# 设置目标项目目录和补丁文件目录
TARGET_PROJ_DIR = os.path.realpath(os.path.join(BUILDTOOLS_DIR, "lib_gpu_counters"))
PATCH_FILES_DIR = SCRIPT_DIR
PATCH_LIST = os.path.join(SCRIPT_DIR, "patch_list.txt")

if shutil.which('patch') is None:
  logging.error("Error: 'patch' 工具未安装，请安装该工具后再运行脚本。")
  sys.exit(1)


def Main():
  # read patch_list.txt
  with open(PATCH_LIST, "r") as patch_file:
    for line in patch_file:
      # 跳过空行和注释行
      line = line.strip()
      if not line or line.startswith("#"):
          continue

      # 解析目标文件和补丁文件路径
      target, patch = line.split(",")

      # 如果有补丁文件，执行 patch 命令
      if patch:
        target_file = os.path.realpath(os.path.join(TARGET_PROJ_DIR, target))
        patch_file_path = os.path.realpath(os.path.join(PATCH_FILES_DIR, patch))
        logging.debug(f"TARGET_FILE: {target_file}")
        if not os.path.exists(target_file):
            logging.error(f"Error: 目标文件 {target_file} 不存在！")
            continue
        logging.debug(f"PATCH_FILE: {patch_file_path}")
        if not os.path.exists(patch_file_path):
            logging.error(f"Error: 目标文件 {patch_file_path} 不存在！")
            continue

        logging.info(f"Applying patch {patch_file_path} to {target_file}")
        os.system(f"patch -N -p1 {target_file} {patch_file_path}")

if __name__ == '__main__':
  logging.basicConfig(level=logging.INFO)
  sys.exit(Main())