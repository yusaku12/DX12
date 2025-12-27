#pragma once

#include "Model\PmxActor.h"

//=====================================================
// PMXモデルレンダリングクラス
//=====================================================
class PmxRender
{
public:

    explicit PmxRender() {};
    ~PmxRender() {};

    //! PMXモデル描画
    void render() const;

    //! PMXアクター追加
    void addPmxActor(const std::shared_ptr<PmxActor>& pmxActor);

private:

    std::vector<std::shared_ptr<PmxActor>> m_pmxActor;
};