# -*- coding: utf-8 -*-
"""
flatbuffer_exporter.py — FlatBuffer (.mdl) 形式のモデルエクスポーター

使用例
------
    from flatbuffer_exporter import FlatBufferExporter

    exporter = FlatBufferExporter()
    exporter.export(model_data, "output/model.mdl")

model_data の構造
-----------------
{
    "bones": [
        {"id": int, "name": str, "parent": int,
         "scale": [x,y,z], "rotate": [x,y,z,w], "translate": [x,y,z]},
        ...
    ],
    "meshes": [
        {
            "name": str,
            "vertices": [
                {"position": [x,y,z], "normal": [x,y,z], "tangent": [x,y,z],
                 "uv": [u,v],
                 "bone_indices": [i0,i1,i2,i3],
                 "bone_weights": [w0,w1,w2,w3]},
                ...
            ],
            "indices": [int, ...],
            "sub_meshes": [
                {"start_index": int, "index_count": int, "material_index": int},
                ...
            ],
            "node_index": int,
            "node_indices": [int, ...],
            "offset_matrices": [[16 floats row-major], ...],
            "bounds_min": [x,y,z],
            "bounds_max": [x,y,z]
        },
        ...
    ],
    "materials": [
        {"name": str, "texture_albedo": str|None, "texture_normal": str|None,
         "diffuse_color": [r,g,b,a]},
        ...
    ],
    "animations": [
        {
            "name": str,
            "duration": float,
            "keyframes": [
                {
                    "time": float,
                    "node_keys": [
                        {"scale": [x,y,z], "rotate": [x,y,z,w], "translate": [x,y,z]},
                        ...
                    ]
                },
                ...
            ]
        },
        ...
    ]
}
"""
from __future__ import annotations

import struct
from typing import Any, Dict, List, Optional

from base_exporter import BaseExporter

# =========================================================================
# FlatBuffer 手動ビルダー
#   flatbuffers Python パッケージが無い環境でも動作するように、
#   最小限の FlatBuffer バイナリを直接構築する軽量実装。
# =========================================================================


class _FlatBufferBuilder:
    """最小限の FlatBuffer バイナリビルダー"""

    def __init__(self, initial_size: int = 1024):
        self._buf = bytearray(initial_size)
        self._head = initial_size  # 書き込みポインタ（末尾から前方へ伸びる）
        self._vtables: List[int] = []  # 過去の vtable オフセット
        self._object_start = 0
        self._vtable_entries: List[int] = []
        self._nested = False
        self._finished = False

    # --- 低レベル ---

    def _grow(self, needed: int) -> None:
        while self._head < needed:
            old = self._buf
            self._buf = bytearray(len(old) * 2)
            self._buf[len(self._buf) - len(old):] = old
            self._head += len(self._buf) // 2

    def _place(self, fmt: str, value) -> None:
        size = struct.calcsize(fmt)
        self._grow(size)
        self._head -= size
        struct.pack_into(fmt, self._buf, self._head, value)

    def _pad(self, align: int) -> None:
        mask = align - 1
        while (len(self._buf) - self._head) & mask:
            self._grow(1)
            self._head -= 1
            self._buf[self._head] = 0

    def offset(self) -> int:
        return len(self._buf) - self._head

    # --- スカラ書き込み ---

    def add_uint8(self, x: int) -> None:
        self._place('<B', x)

    def add_uint16(self, x: int) -> None:
        self._pad(2)
        self._place('<H', x)

    def add_uint32(self, x: int) -> None:
        self._pad(4)
        self._place('<I', x)

    def add_int32(self, x: int) -> None:
        self._pad(4)
        self._place('<i', x)

    def add_uint64(self, x: int) -> None:
        self._pad(8)
        self._place('<Q', x)

    def add_float(self, x: float) -> None:
        self._pad(4)
        self._place('<f', x)

    # --- 文字列 ---

    def create_string(self, s: str) -> int:
        encoded = s.encode('utf-8')
        self._grow(1)
        self._head -= 1
        self._buf[self._head] = 0  # null terminator
        self._grow(len(encoded))
        self._head -= len(encoded)
        self._buf[self._head:self._head + len(encoded)] = encoded
        self._pad(4)
        self._place('<I', len(encoded))
        return self.offset()

    # --- ベクター ---

    def start_vector(self, elem_size: int, num_elems: int, alignment: int) -> None:
        self._pad(alignment)

    def end_vector(self, num_elems: int) -> int:
        self._place('<I', num_elems)
        return self.offset()

    def create_vector_of_uint32(self, data: List[int]) -> int:
        for v in reversed(data):
            self.add_uint32(v)
        return self.end_vector(len(data))

    def create_vector_of_int32(self, data: List[int]) -> int:
        for v in reversed(data):
            self.add_int32(v)
        return self.end_vector(len(data))

    def create_vector_of_offsets(self, offsets: List[int]) -> int:
        self._pad(4)
        for off in reversed(offsets):
            ref = self.offset() - off + 4
            self.add_uint32(ref)
        return self.end_vector(len(offsets))

    def create_vector_of_structs_raw(self, raw_bytes: bytes, count: int, alignment: int = 4) -> int:
        self._pad(alignment)
        self._grow(len(raw_bytes))
        self._head -= len(raw_bytes)
        self._buf[self._head:self._head + len(raw_bytes)] = raw_bytes
        return self.end_vector(count)

    # --- テーブル ---

    def start_table(self, num_fields: int) -> None:
        self._nested = True
        self._vtable_entries = [0] * num_fields
        self._object_start = self.offset()

    def add_offset_field(self, field_index: int, offset_val: int) -> None:
        if offset_val == 0:
            return
        ref = self.offset() - offset_val + 4
        self._pad(4)
        self._place('<I', ref)
        self._vtable_entries[field_index] = self.offset()

    def add_scalar_field_uint64(self, field_index: int, value: int, default: int = 0) -> None:
        if value == default:
            return
        self.add_uint64(value)
        self._vtable_entries[field_index] = self.offset()

    def add_scalar_field_int32(self, field_index: int, value: int, default: int = 0) -> None:
        if value == default:
            return
        self.add_int32(value)
        self._vtable_entries[field_index] = self.offset()

    def add_scalar_field_float(self, field_index: int, value: float, default: float = 0.0) -> None:
        if value == default:
            return
        self.add_float(value)
        self._vtable_entries[field_index] = self.offset()

    def add_struct_field(self, field_index: int, struct_bytes: bytes, alignment: int = 4) -> None:
        if not struct_bytes:
            return
        self._pad(alignment)
        self._grow(len(struct_bytes))
        self._head -= len(struct_bytes)
        self._buf[self._head:self._head + len(struct_bytes)] = struct_bytes
        self._vtable_entries[field_index] = self.offset()

    def end_table(self) -> int:
        self._pad(4)
        self._place('<i', 0)  # placeholder for vtable offset
        table_end = self.offset()

        # vtable: [vtable_size, table_size, field_offsets...]
        vtable_size = 4 + 2 * len(self._vtable_entries)
        table_size = table_end - self._object_start

        vtable = struct.pack('<HH', vtable_size, table_size)
        for entry in self._vtable_entries:
            if entry:
                vtable += struct.pack('<H', table_end - entry)
            else:
                vtable += struct.pack('<H', 0)

        # 重複 vtable チェック
        existing = -1
        for vt_off in self._vtables:
            vt_pos = len(self._buf) - vt_off
            vt_len = struct.unpack_from('<H', self._buf, vt_pos)[0]
            if vt_len == vtable_size and self._buf[vt_pos:vt_pos + vtable_size] == vtable:
                existing = vt_off
                break

        if existing >= 0:
            # 既存 vtable を参照
            table_pos = len(self._buf) - table_end
            struct.pack_into('<i', self._buf, table_pos, existing - table_end)
        else:
            # 新規 vtable 書き込み
            self._pad(2)
            self._grow(len(vtable))
            self._head -= len(vtable)
            self._buf[self._head:self._head + len(vtable)] = vtable
            vt_offset = self.offset()
            self._vtables.append(vt_offset)
            table_pos = len(self._buf) - table_end
            struct.pack_into('<i', self._buf, table_pos, vt_offset - table_end)

        self._nested = False
        return table_end

    # --- 仕上げ ---

    def finish(self, root_table: int, file_identifier: Optional[str] = None) -> None:
        self._pad(4)
        if file_identifier:
            ident = file_identifier.encode('ascii')
            assert len(ident) == 4
            self._grow(4)
            self._head -= 4
            self._buf[self._head:self._head + 4] = ident
        ref = self.offset() - root_table + 4
        self._place('<I', ref)
        self._finished = True

    def output(self) -> bytes:
        assert self._finished
        return bytes(self._buf[self._head:])


# =========================================================================
# 構造体パック用ヘルパー
# =========================================================================

def _pack_vec2(v) -> bytes:
    return struct.pack('<2f', v[0], v[1])


def _pack_vec3(v) -> bytes:
    return struct.pack('<3f', v[0], v[1], v[2])


def _pack_vec4(v) -> bytes:
    return struct.pack('<4f', v[0], v[1], v[2], v[3])


def _pack_matrix(m) -> bytes:
    return struct.pack('<16f', *m)


def _pack_vertex(v) -> bytes:
    return (
        _pack_vec3(v["position"])
        + _pack_vec3(v["normal"])
        + _pack_vec3(v["tangent"])
        + _pack_vec2(v["uv"])
        + struct.pack('<4I', *v["bone_indices"])
        + struct.pack('<4f', *v["bone_weights"])
    )


def _pack_sub_mesh(s) -> bytes:
    return struct.pack('<3I', s["start_index"], s["index_count"], s["material_index"])


def _pack_node_key(nk) -> bytes:
    return _pack_vec3(nk["scale"]) + _pack_vec4(nk["rotate"]) + _pack_vec3(nk["translate"])


# =========================================================================
# FlatBufferExporter
# =========================================================================


class FlatBufferExporter(BaseExporter):
    """FlatBuffer (.mdl) 形式でモデルデータを書き出す"""

    @property
    def extension(self) -> str:
        return ".mdl"

    @property
    def format_name(self) -> str:
        return "FlatBuffer MDL"

    # --- 内部 ---

    def _build_bone(self, fbb: _FlatBufferBuilder, bone: dict) -> int:
        name_off = fbb.create_string(bone.get("name", ""))

        scale_bytes = _pack_vec3(bone.get("scale", [1, 1, 1]))
        rotate_bytes = _pack_vec4(bone.get("rotate", [0, 0, 0, 1]))
        translate_bytes = _pack_vec3(bone.get("translate", [0, 0, 0]))

        # bone table: id(0), name(1), parent(2), scale(3), rotate(4), translate(5)
        fbb.start_table(6)
        fbb.add_struct_field(5, translate_bytes)
        fbb.add_struct_field(4, rotate_bytes)
        fbb.add_struct_field(3, scale_bytes)
        fbb.add_scalar_field_int32(2, bone.get("parent", 0))
        fbb.add_offset_field(1, name_off)
        fbb.add_scalar_field_uint64(0, bone.get("id", 0))
        return fbb.end_table()

    def _build_mesh(self, fbb: _FlatBufferBuilder, mesh: dict) -> int:
        name_off = fbb.create_string(mesh.get("name", ""))

        # vertices (struct vector)
        verts = mesh.get("vertices", [])
        raw = b"".join(_pack_vertex(v) for v in verts)
        vert_off = fbb.create_vector_of_structs_raw(raw, len(verts)) if verts else 0

        # indices
        indices = mesh.get("indices", [])
        idx_off = fbb.create_vector_of_uint32(indices) if indices else 0

        # sub_meshes (struct vector)
        subs = mesh.get("sub_meshes", [])
        raw = b"".join(_pack_sub_mesh(s) for s in subs)
        sub_off = fbb.create_vector_of_structs_raw(raw, len(subs)) if subs else 0

        # node_indices
        ni = mesh.get("node_indices", [])
        ni_off = fbb.create_vector_of_int32(ni) if ni else 0

        # offset_matrices (struct vector)
        mats = mesh.get("offset_matrices", [])
        raw = b"".join(_pack_matrix(m) for m in mats)
        mat_off = fbb.create_vector_of_structs_raw(raw, len(mats)) if mats else 0

        bounds_min = _pack_vec3(mesh.get("bounds_min", [0, 0, 0]))
        bounds_max = _pack_vec3(mesh.get("bounds_max", [0, 0, 0]))

        # mesh table: name(0), vertices(1), indices(2), sub_meshes(3),
        #             node_index(4), node_indices(5), offset_matrices(6),
        #             bounds_min(7), bounds_max(8)
        fbb.start_table(9)
        fbb.add_struct_field(8, bounds_max)
        fbb.add_struct_field(7, bounds_min)
        fbb.add_offset_field(6, mat_off)
        fbb.add_offset_field(5, ni_off)
        fbb.add_scalar_field_int32(4, mesh.get("node_index", 0))
        fbb.add_offset_field(3, sub_off)
        fbb.add_offset_field(2, idx_off)
        fbb.add_offset_field(1, vert_off)
        fbb.add_offset_field(0, name_off)
        return fbb.end_table()

    def _build_material(self, fbb: _FlatBufferBuilder, mat: dict) -> int:
        name_off = fbb.create_string(mat.get("name", ""))

        albedo = mat.get("texture_albedo")
        albedo_off = fbb.create_string(albedo) if albedo else 0

        normal = mat.get("texture_normal")
        normal_off = fbb.create_string(normal) if normal else 0

        diffuse_bytes = _pack_vec4(mat.get("diffuse_color", [1, 1, 1, 1]))

        # material table: name(0), texture_albedo(1), texture_normal(2), diffuse_color(3)
        fbb.start_table(4)
        fbb.add_struct_field(3, diffuse_bytes)
        fbb.add_offset_field(2, normal_off)
        fbb.add_offset_field(1, albedo_off)
        fbb.add_offset_field(0, name_off)
        return fbb.end_table()

    def _build_keyframe(self, fbb: _FlatBufferBuilder, kf: dict) -> int:
        keys = kf.get("node_keys", [])
        raw = b"".join(_pack_node_key(nk) for nk in keys)
        keys_off = fbb.create_vector_of_structs_raw(raw, len(keys)) if keys else 0

        # keyframe table: time(0), node_keys(1)
        fbb.start_table(2)
        fbb.add_offset_field(1, keys_off)
        fbb.add_scalar_field_float(0, kf.get("time", 0.0))
        return fbb.end_table()

    def _build_animation(self, fbb: _FlatBufferBuilder, anim: dict) -> int:
        name_off = fbb.create_string(anim.get("name", ""))

        kf_offsets = [self._build_keyframe(fbb, kf) for kf in anim.get("keyframes", [])]
        kf_vec_off = fbb.create_vector_of_offsets(kf_offsets) if kf_offsets else 0

        # animation table: name(0), duration(1), keyframes(2)
        fbb.start_table(3)
        fbb.add_offset_field(2, kf_vec_off)
        fbb.add_scalar_field_float(1, anim.get("duration", 0.0))
        fbb.add_offset_field(0, name_off)
        return fbb.end_table()

    def _export_impl(self, model_data: Dict[str, Any], output_path: str) -> None:
        fbb = _FlatBufferBuilder(4096)

        # 各データ構築
        bone_offsets = [self._build_bone(fbb, b) for b in model_data.get("bones", [])]
        mesh_offsets = [self._build_mesh(fbb, m) for m in model_data.get("meshes", [])]
        mat_offsets = [self._build_material(fbb, m) for m in model_data.get("materials", [])]
        anim_offsets = [self._build_animation(fbb, a) for a in model_data.get("animations", [])]

        bones_off = fbb.create_vector_of_offsets(bone_offsets) if bone_offsets else 0
        meshes_off = fbb.create_vector_of_offsets(mesh_offsets) if mesh_offsets else 0
        mats_off = fbb.create_vector_of_offsets(mat_offsets) if mat_offsets else 0
        anims_off = fbb.create_vector_of_offsets(anim_offsets) if anim_offsets else 0

        # model table: bones(0), meshes(1), materials(2), animations(3)
        fbb.start_table(4)
        fbb.add_offset_field(3, anims_off)
        fbb.add_offset_field(2, mats_off)
        fbb.add_offset_field(1, meshes_off)
        fbb.add_offset_field(0, bones_off)
        root = fbb.end_table()

        fbb.finish(root, file_identifier="MDL0")

        data = fbb.output()
        with open(output_path, "wb") as f:
            f.write(data)
