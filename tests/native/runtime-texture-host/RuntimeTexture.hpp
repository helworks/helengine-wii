#pragma once
#include <cstdint>
/// Minimal generated runtime shape; production ownership remains implemented by the real GameCube subclass under test.
class RuntimeTexture { std::int32_t Width=0,Height=0;public:virtual ~RuntimeTexture()=default;std::int32_t get_Width()const{return Width;}std::int32_t get_Height()const{return Height;}void set_Width(std::int32_t value){Width=value;}void set_Height(std::int32_t value){Height=value;} };
