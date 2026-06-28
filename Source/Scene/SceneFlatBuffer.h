#pragma once

#include <filesystem>
#include <string>
#include <vector>

enum class SceneId : int;

namespace SceneFlatBuffer
{
    struct PreparedSceneData
    {
        std::filesystem::path sourcePath;
        std::vector<uint8_t> bytes;
        SceneId sceneId{};
    };

    //! 現在の GameObject 階層を FlatBuffers 形式で保存
    bool save(const std::filesystem::path& filePath, SceneId sceneId);

    //! シーンファイルを読み込み・検証し、メインスレッド適用用データを作成
    bool prepareLoad(const std::filesystem::path& filePath, PreparedSceneData& outData, std::string* outErrorMessage = nullptr);

    //! prepareLoad で作成したデータを現在シーンへ適用
    bool loadPrepared(const PreparedSceneData& prepared, SceneId currentSceneId, SceneId* outSceneId = nullptr);

    //! FlatBuffers 形式のシーンを読み込み、現在の GameObject 階層へ適用
    bool load(const std::filesystem::path& filePath, SceneId currentSceneId, SceneId* outSceneId = nullptr);
}