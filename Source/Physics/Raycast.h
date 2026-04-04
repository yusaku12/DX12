#pragma once

#include "PhysicsWorld.h"

class GameObject;
class CameraComponent;

//=====================================================
// Physics — レイキャスト ユーティリティ（静的関数群）
//=====================================================
namespace Physics
{
    //! レイキャストの結果情報
    struct RaycastHit
    {
        Vector3    point = Vector3::Zero;        //!< ヒットしたワールド座標
        Vector3    normal = Vector3::Zero;       //!< ヒット面の法線
        float      distance = 0.0f;              //!< レイの始点からの距離
        GameObject* gameObject = nullptr;        //!< ヒットした GameObject
        RigidbodyComponent* rigidbody = nullptr; //!< ヒットした RigidbodyComponent
    };

    //! origin から direction 方向に maxDistance だけレイを飛ばし、最も近いヒットを返す
    bool Raycast(const Vector3& origin, const Vector3& direction, float maxDistance, RaycastHit& outHit);

    //! ヒット情報が不要な場合のオーバーロード（当たったかだけ知りたい）
    bool Raycast(const Vector3& origin, const Vector3& direction, float maxDistance);

    //! Ray 構造体を使用するオーバーロード
    bool Raycast(const Ray& ray, float maxDistance, RaycastHit& outHit);

    //! origin から direction 方向のすべてのヒットを返す
    bool RaycastAll(const Vector3& origin, const Vector3& direction, float maxDistance, std::vector<RaycastHit>& outHits);

    //! Ray 構造体を使用するオーバーロード
    bool RaycastAll(const Ray& ray, float maxDistance, std::vector<RaycastHit>& outHits);

    //! スクリーン座標 (ピクセル) からワールド空間のレイを生成する
    //! @param screenX     スクリーン X 座標（ピクセル）
    //! @param screenY     スクリーン Y 座標（ピクセル）
    //! @param viewportX   ビューポート左上 X
    //! @param viewportY   ビューポート左上 Y
    //! @param viewportW   ビューポート幅
    //! @param viewportH   ビューポート高さ
    //! @param view        ビュー行列
    //! @param projection  プロジェクション行列
    //! @return            ワールド空間のレイ
    Ray ScreenPointToRay(
        float screenX, float screenY,
        float viewportX, float viewportY,
        float viewportW, float viewportH,
        const Matrix& view, const Matrix& projection);

    //! メインカメラを使用してスクリーン座標からワールド空間レイを生成する
    //! @param screenX  スクリーン X 座標（ピクセル）
    //! @param screenY  スクリーン Y 座標（ピクセル）
    //! @return         ワールド空間のレイ（カメラが存在しない場合は原点から前方へのレイ）
    Ray ScreenPointToRay(float screenX, float screenY);
}