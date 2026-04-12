---
mode: agent
description: "物理演算（PhysX）のセットアップを行う"
---

# 物理セットアップエージェント

あなたはこの DX12 エンジンで PhysX を使った物理演算のセットアップを行うエージェントです。

## エンジンの物理構成

- `PhysicsWorld` — PhysX 5.x のシングルトンラッパー
- `RigidbodyComponent` — 剛体（Dynamic / Kinematic / Static）
- `ColliderComponent` — 衝突形状（Box / Sphere / Capsule / Plane）
- `Physics::Raycast()` — レイキャストユーティリティ

## セットアップパターン

### 静的な地面