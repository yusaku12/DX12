#pragma once

#include "IModelExporter.h"

//==============================================================
// FlatBuffer 形式でモデルデータを書き出すエクスポーター
//==============================================================
class FlatBufferModelExporter : public IModelExporter
{
public:

    bool exportModel(const ModelResource::Model& model, const std::string& filePath) override;

    std::string getExtension()  const override { return ".mdl"; }
    std::string getFormatName() const override { return "FlatBuffer MDL"; }
};
