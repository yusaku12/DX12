# -*- coding: utf-8 -*-
"""
export_model.py — モデルデータ エクスポート CLI

使い方
------
    python export_model.py <input.json> <output_path> [--format mdl]

入力は JSON 形式のモデルデータ。
出力フォーマットは --format で指定（デフォルト: mdl = FlatBuffer）。
新しいエクスポーターを追加した場合は EXPORTERS 辞書に登録する。
"""
from __future__ import annotations

import argparse
import json
import sys
import os

# --- エクスポーター登録テーブル ---
# フォーマット名 → エクスポータークラスのマッピング
# 新しいフォーマットを追加する場合はここにエントリを追加
EXPORTERS = {}


def _register_exporters():
    """利用可能なエクスポーターを登録する"""
    from flatbuffer_exporter import FlatBufferExporter
    EXPORTERS["mdl"] = FlatBufferExporter


def main():
    parser = argparse.ArgumentParser(
        description="モデルデータをバイナリ形式にエクスポートする CLI ツール"
    )
    parser.add_argument(
        "input",
        help="入力 JSON ファイルパス"
    )
    parser.add_argument(
        "output",
        help="出力ファイルパス"
    )
    parser.add_argument(
        "--format", "-f",
        default="mdl",
        help="出力フォーマット (デフォルト: mdl)"
    )

    args = parser.parse_args()

    # エクスポーター登録
    _register_exporters()

    if args.format not in EXPORTERS:
        print(f"ERROR: Unknown format '{args.format}'")
        print(f"Available formats: {', '.join(EXPORTERS.keys())}")
        sys.exit(1)

    # 入力データ読み込み
    if not os.path.exists(args.input):
        print(f"ERROR: Input file not found: {args.input}")
        sys.exit(1)

    with open(args.input, "r", encoding="utf-8") as f:
        model_data = json.load(f)

    # エクスポート実行
    exporter = EXPORTERS[args.format]()
    output_path = exporter.export(model_data, args.output)

    print(f"Done: {output_path}")


if __name__ == "__main__":
    main()
