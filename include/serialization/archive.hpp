#ifndef ENGINE_SERIALIZATION_ARCHIVE_HPP
#define ENGINE_SERIALIZATION_ARCHIVE_HPP

#include "../OS/types.h"
#include <cstddef>

namespace engine::serialization {

    class IOutputArchive {
    public:
        virtual ~IOutputArchive() = default;

        virtual void WriteI32(const char* name, i32 value) = 0;
        virtual void WriteU32(const char* name, u32 value) = 0;
        virtual void WriteF32(const char* name, f32 value) = 0;
        virtual void WriteF64(const char* name, f64 value) = 0;
        virtual void WriteBool(const char* name, bool value) = 0;
        virtual void WriteString(const char* name, const char* value) = 0;

        virtual void BeginObject(const char* name) = 0;
        virtual void EndObject() = 0;

        virtual void BeginArray(const char* name, size_t size) = 0;
        virtual void EndArray() = 0;
    };

    class IInputArchive {
    public:
        virtual ~IInputArchive() = default;

        virtual bool ReadI32(const char* name, i32& out_value) = 0;
        virtual bool ReadU32(const char* name, u32& out_value) = 0;
        virtual bool ReadF32(const char* name, f32& out_value) = 0;
        virtual bool ReadF64(const char* name, f64& out_value) = 0;
        virtual bool ReadBool(const char* name, bool& out_value) = 0;

        // Caller must allocate or handle string memory
        virtual bool ReadString(const char* name, char* out_buffer, size_t max_len) = 0;

        virtual bool BeginObject(const char* name) = 0;
        virtual void EndObject() = 0;

        virtual bool BeginArray(const char* name, size_t& out_size) = 0;
        virtual void EndArray() = 0;
    };

} // namespace engine::serialization

#endif // ENGINE_SERIALIZATION_ARCHIVE_HPP
