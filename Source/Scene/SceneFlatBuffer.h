#pragma once

#include "SceneManager.h"

namespace SceneFlatBuffer
{
    //! 現在の GameObject 階層を FlatBuffers 形式で保存
    bool save(const std::filesystem::path& filePath, SceneId sceneId);

    //! FlatBuffers 形式のシーンを読み込み、現在の GameObject 階層へ適用
    bool load(const std::filesystem::path& filePath, SceneId currentSceneId, SceneId* outSceneId = nullptr);
}