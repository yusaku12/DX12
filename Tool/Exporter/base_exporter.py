# -*- coding: utf-8 -*-
"""
base_exporter.py — モデルエクスポーターの基底クラス

新しいフォーマットを追加する場合:
  1. BaseExporter を継承するクラスを作成
  2. extension / format_name プロパティを定義
  3. _export_impl(model_data, output_path) を実装
"""
from __future__ import annotations

import abc
import os
from typing import Any, Dict


class BaseExporter(abc.ABC):
    """モデルデータ書き出しの抽象基底クラス"""

    # --- 派生クラスで実装 ---

    @property
    @abc.abstractmethod
    def extension(self) -> str:
        """出力ファイルの拡張子 (例: '.mdl')"""
        ...

    @property
    @abc.abstractmethod
    def format_name(self) -> str:
        """フォーマット名 (例: 'FlatBuffer MDL')"""
        ...

    @abc.abstractmethod
    def _export_impl(self, model_data: Dict[str, Any], output_path: str) -> None:
        """
        実際のエクスポート処理。

        Parameters
        ----------
        model_data : dict
            エクスポート対象のモデルデータ（JSON 互換辞書）。
            キー例: "bones", "meshes", "materials", "animations"
        output_path : str
            出力先ファイルパス
        """
        ...

    # --- 共通ロジック ---

    def export(self, model_data: Dict[str, Any], output_path: str) -> str:
        """
        エクスポートを実行する。

        Parameters
        ----------
        model_data : dict
            エクスポート対象のモデルデータ
        output_path : str
            出力先ファイルパス (拡張子が無ければ自動付与)

        Returns
        -------
        str
            実際に書き出したファイルパス
        """
        # 拡張子の自動補完
        _, ext = os.path.splitext(output_path)
        if ext.lower() != self.extension.lower():
            output_path += self.extension

        # 出力先ディレクトリの確保
        out_dir = os.path.dirname(output_path)
        if out_dir and not os.path.exists(out_dir):
            os.makedirs(out_dir, exist_ok=True)

        self._export_impl(model_data, output_path)

        file_size = os.path.getsize(output_path)
        print(f"[{self.format_name}] Exported {file_size:,} bytes -> {output_path}")
        return output_path
