#pragma once

#include "ModelResource.h"

//! FlatBuffers 形式のモデル入出力
namespace ModelFlatBuffer
{
    //! ファイルから FlatBuffers モデルを読み込む
    bool load(const std::filesystem::path& filePath, ModelResource::Model& outModel);

    //! FlatBuffers モデルをファイルへ書き出す
    bool save(const std::filesystem::path& filePath, const ModelResource::Model& model);
}