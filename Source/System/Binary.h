#pragma once
#include <ostream>
#include <istream>
#include "Math\SimpleMath.h"

//=====================================================
// バイナリ書き込みヘルパー
//=====================================================
namespace Binary
{
    using namespace DirectX::SimpleMath;

    void writeU32(std::ostream& out, uint32_t v)
    {
        out.write(reinterpret_cast<const char*>(&v), sizeof(v));
    }

    void writeF32(std::ostream& out, float v)
    {
        out.write(reinterpret_cast<const char*>(&v), sizeof(v));
    }

    void writeVec3(std::ostream& out, const Vector3& v)
    {
        out.write(reinterpret_cast<const char*>(&v), sizeof(Vector3));
    }

    void writeVec4(std::ostream& out, const Vector4& v)
    {
        out.write(reinterpret_cast<const char*>(&v), sizeof(Vector4));
    }

    void writeString(std::ostream& out, const std::string& s)
    {
        uint32_t len = (uint32_t)s.size();
        writeU32(out, len);
        if (len > 0)
            out.write(s.data(), len);
    }

    void writeBlob(std::ostream& out, const void* data, size_t bytes)
    {
        out.write(reinterpret_cast<const char*>(data), bytes);
    }

    void writeBool(std::ostream& out, bool v)
    {
        uint8_t b = v ? 1 : 0;
        out.write(reinterpret_cast<const char*>(&b), sizeof(b));
    }

    void writeS32(std::ostream& out, int32_t v)
    {
        out.write(reinterpret_cast<const char*>(&v), sizeof(v));
    }

    void writeVec2(std::ostream& out, const Vector2& v)
    {
        out.write(reinterpret_cast<const char*>(&v), sizeof(Vector2));
    }

    void writeMatrix(std::ostream& out, const Matrix& m)
    {
        out.write(reinterpret_cast<const char*>(&m), sizeof(Matrix));
    }

    void writeQuat(std::ostream& out, const Quaternion& q)
    {
        out.write(reinterpret_cast<const char*>(&q), sizeof(Quaternion));
    }

    void readBlob(std::istream& in, void* data, size_t bytes)
    {
        in.read(reinterpret_cast<char*>(data), bytes);
    }

    template<typename T>
    void writeVector(std::ostream& out, const std::vector<T>& v)
    {
        writeU32(out, (uint32_t)v.size());
        if (!v.empty())
            writeBlob(out, v.data(), sizeof(T) * v.size());
    }

    uint32_t readU32(std::istream& in)
    {
        uint32_t v = 0;
        in.read(reinterpret_cast<char*>(&v), sizeof(v));
        return v;
    }

    float readF32(std::istream& in)
    {
        float v = 0;
        in.read(reinterpret_cast<char*>(&v), sizeof(v));
        return v;
    }

    Vector3 readVec3(std::istream& in)
    {
        Vector3 v;
        in.read(reinterpret_cast<char*>(&v), sizeof(Vector3));
        return v;
    }

    Vector4 readVec4(std::istream& in)
    {
        Vector4 v;
        in.read(reinterpret_cast<char*>(&v), sizeof(Vector4));
        return v;
    }

    std::string readString(std::istream& in)
    {
        uint32_t len = readU32(in);
        if (len == 0) return {};
        std::string s(len, '\0');
        in.read(s.data(), len);
        return s;
    }

    bool readBool(std::istream& in)
    {
        uint8_t b;
        in.read(reinterpret_cast<char*>(&b), sizeof(b));
        return b != 0;
    }

    int32_t readS32(std::istream& in)
    {
        int32_t v;
        in.read(reinterpret_cast<char*>(&v), sizeof(v));
        return v;
    }

    Vector2 readVec2(std::istream& in)
    {
        Vector2 v;
        in.read(reinterpret_cast<char*>(&v), sizeof(Vector2));
        return v;
    }

    Matrix readMatrix(std::istream& in)
    {
        Matrix m;
        in.read(reinterpret_cast<char*>(&m), sizeof(Matrix));
        return m;
    }

    Quaternion readQuat(std::istream& in)
    {
        Quaternion q;
        in.read(reinterpret_cast<char*>(&q), sizeof(Quaternion));
        return q;
    }

    template<typename T>
    void readVector(std::istream& in, std::vector<T>& v)
    {
        uint32_t size = readU32(in);
        v.resize(size);

        if (size > 0)
            readBlob(in, v.data(), sizeof(T) * size);
    }
}