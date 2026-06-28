#pragma once

//=====================================================
//! アニメーションシステム共通定義
//=====================================================

//! アニメーションパラメータ型
enum class AnimParamType
{
    Float,
    Int,
    Bool,
    Trigger,
};

//! 遷移条件の比較演算子
enum class CompareOp
{
    Greater,
    Less,
    Equal,
    NotEqual,
};

//! レイヤーブレンドモード
enum class LayerBlendMode
{
    Override,   //!< 上位レイヤーで完全上書き
    Additive,   //!< 加算ブレンド
};

//! ループモード
enum class LoopMode
{
    Once,       //!< 1回再生で停止
    Loop,       //!< ループ再生
    PingPong,   //!< 往復再生
};

//! ブレンドツリー種別
enum class BlendTreeType
{
    Blend1D,    //!< 1D ブレンド（閾値ベース）
    Freeform2D, //!< 2D Freeform ブレンド（距離ベース）
    FreeformDirectional2D, //!< 2D Freeform Directional ブレンド（方向+速度ベース）
};