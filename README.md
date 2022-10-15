# RiceMetaCompiler
Code generation from clang ast

## Installation (Compiling from source)
### Requirements

| Package                 | Min    |
|-------------------------|--------|
| clang                   | 13.0.0 |

- Download the source
```shell
git clone https://github.com/dgudim/RiceMetaCompiler
cd RiceMetaCompiler
```
- Build
```shell
mkdir build
cd build
cmake ..
make -j8
```
## Usage
RiceMetaCompiler [options]

### Options: 
| Option                  | Description                                                                                                 |
| ----------------------- | ----------------------------------------------------------------------------------------------------------- |
| --help                  | Display this help page                                                                                      |
| --version               | Print version string                                                                                        |
| --print                 | Print generated code to stdout                                                                              |
| --dump                  | Dump generated AST to a file                                                                                |
| header_file_path=[path] | Path to the header file to build the AST for                                                                |
| source_file_path=[path] | Path to the source file, used with 'compile_commands_path' to search for additional includes and parameters |
| compile_commands_path=  | Path to compile_commands.json                                                                               |

## Example
### Input (test.hpp)
```cpp
struct struct3 {
    int a;
    int b;
};

struct REFLECTABLE struct1 {
    int i;
};

struct REFLECTABLE struct2 {
    NOT_REFLECTABLE double d;
    struct REFLECTABLE inner_struct {
        std::string i;
    };
};
```
### Command
```shell
./RiceMetaCompiler header_file_path=./RiceEngine/RiceEngine/ComponentBuilder/tests/include/test.hpp
```

### Output (test_meta.hpp)
```cpp
#pragma once

#include "/mnt/personal_misc/!Danila/_projects/RiceEngine/RiceEngine/ComponentBuilder/tests/include/test.cpp"
#include <MetaCompiler/ReflectionHelper.hpp>

template <> struct Meta::TypeOf<struct1> {
    Type<struct1, int> type() { 
    return Type<struct111, int>{Types::Struct,
    "", "struct1", 
    {"i", &struct1::i, {}}}; }
};
template <> struct Meta::TypeOf<struct2::inner_struct> {
    Type<struct2::inner_struct, std::basic_string<char>> type() { 
    return Type<struct2::inner_struct, std::basic_string<char>>{Types::Struct,
    "struct2", "inner_struct", 
    {"i", &struct2::inner_struct::i, {}}}; }
};
template <> struct Meta::TypeOf<struct2> {
    Type<struct2> type() { 
    return Type<struct2>{Types::Struct,
    "", "struct2"}; }
};
```
### Usage

```cpp
#include "test.hpp"
#include "test_meta.hpp"

int main() {
    using namespace std;

    struct1 s;
    s.i = 5;
    
    auto type = Meta::TypeOf<struct1>().type();    
    cout << "object: " << type.getShortName() << "\n";
    auto members = type.getMembers();
    
    cout << "members: \n";
    
    Meta::for_each(members, [&](const auto &member) {
        std::string name = member.getName();
        auto p = member.getMemberPointer();
        auto &value = t->*p;
        cout << "name: " << name << "\n";
        cout << "value: " << value << "\n";
        cout << "\n";
    });
    
    cout << object;
    
    return 0;
}
```
### Main Output
```
object: struct1
members:
name: i
value: 5
```
