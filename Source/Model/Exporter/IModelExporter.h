#pragma once

#include "Model\ModelResource.h"

//==============================================================
// モデルエクスポーターインターフェース
//  - 今後フォーマットを追加する場合はこのインターフェースを継承する
//==============================================================
class IModelExporter
{
public:

    virtual ~IModelExporter() = default;

    //! モデルデータをファイルに書き出す
    //! @param model  書き出し対象のモデルデータ
    //! @param filePath  出力先ファイルパス
    //! @return 成功時 true
    virtual bool exportModel(const ModelResource::Model& model, const std::string& filePath) = 0;

    //! このエクスポーターが扱うファイル拡張子を返す (例: ".mdl")
    virtual std::string getExtension() const = 0;

    //! フォーマット名を返す (例: "FlatBuffer MDL")
    virtual std::string getFormatName() const = 0;
};
