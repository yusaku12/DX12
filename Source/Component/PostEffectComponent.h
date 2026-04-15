#include "IRenderComponent.h"
#include "Graphics\ConstantBuffer.h"

//============================================================================
// PostEffectComponent — ポストエフェクトコンポーネント
//============================================================================
class PostEffectComponent : public IRenderComponent
{
public:

    explicit PostEffectComponent();
    ~PostEffectComponent() override = default;

    //! 初期化
    void awake() override;

    //! 更新処理
    void update() override;

    //! 描画（シングルスレッド描画）
    void render() override;

    //! 描画（マルチスレッド描画）
    void render(ID3D12GraphicsCommandList* cmd) override;

    //! インスペクタ表示
    void inspectGUI() override;
};