#pragma once
#include <stdexcept>
/// Exception shapes used by the generated runtime; pointer throws are caught and deleted by the fixture.
struct InvalidOperationException:std::runtime_error {using std::runtime_error::runtime_error;};
struct ArgumentNullException:std::runtime_error {using std::runtime_error::runtime_error;};
struct ArgumentOutOfRangeException:std::runtime_error {using std::runtime_error::runtime_error;ArgumentOutOfRangeException(const char*a,const char*):std::runtime_error(a){} };
