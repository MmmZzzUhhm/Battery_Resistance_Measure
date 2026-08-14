Import("env")
import os
import shutil

from platformio.project.config import ProjectConfig

# NOTE: pioarduino platform-espressif32 (platform.py の _install_with_idf_tools) は
# platform_packages でtoolchain-riscv32-espをバージョン固定した場合、pio run毎に
# packages/toolchain-riscv32-esp を削除してtools/toolchain-riscv32-espから再配置しようと
# するが、この再配置(pm.install)がエラーを出さずに失敗する既知の不具合がある
# (呼び出し元 configure_default_packages() が例外を握りつぶすため表面化しない)。
# tools/側は毎回正常に展開されることを確認済みなので、コンパイル開始前に
# packages/側が空ならtools/からコピーして復旧する。どのPC・core_dirでも動くよう
# パスは動的に取得する。
core_dir = ProjectConfig.get_instance().get("platformio", "core_dir")
TOOLS_DIR = os.path.join(core_dir, "tools", "toolchain-riscv32-esp")
PACKAGES_DIR = os.path.join(core_dir, "packages", "toolchain-riscv32-esp")


def _has_compiler(path):
    return os.path.isfile(os.path.join(path, "bin", "riscv32-esp-elf-g++.exe"))


if _has_compiler(TOOLS_DIR) and not _has_compiler(PACKAGES_DIR):
    if os.path.isdir(PACKAGES_DIR):
        shutil.rmtree(PACKAGES_DIR)
    shutil.copytree(TOOLS_DIR, PACKAGES_DIR)
    print("[fix_toolchain] toolchain-riscv32-esp を tools/ から packages/ へ復旧しました")

# platform.py側がPATHを計算した時点ではpackages/側が無かった可能性があるため、
# ビルド実行環境のPATHにも明示的に追加しておく (念のため常に実行)
if _has_compiler(PACKAGES_DIR):
    env.PrependENVPath("PATH", os.path.join(PACKAGES_DIR, "bin"))
