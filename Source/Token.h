#pragma once

namespace RTGL1
{

namespace detail
{
    struct InitAsExisting
    {
        explicit InitAsExisting() = default;
    };
}
inline constexpr detail::InitAsExisting InitAsExisting{};


template< size_t ID >
struct Token
{
    Token() = default;
    explicit Token( detail::InitAsExisting ) : exists( true ) {}

    ~Token() = default;

    Token( Token&& other ) noexcept
    {
        this->exists = other.exists;
        other.exists = false;
    }
    Token& operator=( Token&& other ) noexcept
    {
        this->exists = other.exists;
        other.exists = false;
        return *this;
    }

    Token( const Token& )            = delete;
    Token& operator=( const Token& ) = delete;

    operator bool() const { return exists; }

private:
    bool exists{ false };
};


using StaticGeometryToken  = Token< 0 >;
using DynamicGeometryToken = Token< 1 >;

}
