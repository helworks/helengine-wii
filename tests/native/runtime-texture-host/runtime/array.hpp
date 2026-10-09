#pragma once
#include <cstdint>
/// Host fixture-owned array with the generated API used by the actual runtime translation unit.
template<class T>struct Array { T*Data;std::int32_t Length;explicit Array(std::int32_t n):Data(new T[n]{}),Length(n){}~Array(){delete[]Data;}T&operator[](std::int32_t n){return Data[n];}std::int32_t get_Length()const{return Length;} };
