#include <string>
#include <vector>

#include "../include/MetaCompiler/ReflectionHelper.hpp"

template <typename T> struct REFLECTABLE template_struct {
    int x;
    T element;
    std::vector<T> element_vector;
};

template <typename T> struct REFLECTABLE template_container_struct {
    T x;
    template_struct<T> test_struct;
    template_struct<int> int_struct;
};

template <typename T, typename U> struct REFLECTABLE template_container_struct_2 {
    U y;
    template_struct<T> test_struct;
    template_struct<int> int_struct;

    struct REFLECTABLE inner_template_struct {
        int x;
    };
};