#pragma once

#include <string>
#include <atomic>

//=====================================================
// すべてのエンジンオブジェクトの基底クラス
// UnityEngine.Object 相当
//=====================================================
class Object
{
public:

    Object() { m_instanceId = ++s_idCounter; }
    virtual ~Object() = default;

    //! 一意なインスタンスID
    uint64_t getInstanceId() const { return m_instanceId; }

    //! オブジェクト名の取得・設定
    const std::string& getName() const { return m_name; }
    void setName(const std::string& name) { m_name = name; }

protected:

    std::string m_name = "Object";

private:

    uint64_t m_instanceId;
    static inline std::atomic<uint64_t> s_idCounter = 0;     //!< 全Object共通のIDカウンタ
};