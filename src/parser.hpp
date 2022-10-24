#include <string>

struct Struct;

enum class LocationNodeType { STURCT, NAMESPACE };

struct LocationNode {
    std::string name;
    LocationNodeType type;
    Struct *associated_struct = nullptr;
    explicit operator std::string() const;
};

using Location = std::vector<LocationNode>;

struct Field {
    std::string name;
    std::string type;
    std::vector<std::string> attributes;
    bool not_reflectable;
    std::string getAttributes() const;
    friend std::ostream &operator<<(std::ostream &os, const Field &field);
};

inline std::ostream &operator<<(std::ostream &os, const Field &field) {
    os << field.type << " " << field.name << ";";
    return os;
}

struct TemplateParameter {
    int index;
    int depth;
    std::string name;
};

using TemplateDeclaration = std::vector<TemplateParameter>;

struct Struct {

    Location location;
    std::string name;
    std::vector<Field> fields;
    TemplateDeclaration template_params;

    bool is_reflectable = false;

    std::string getTemplate(bool include_typename = false) const;

    std::string getLocation() const;

    std::string getFullName() const;

    friend std::ostream &operator<<(std::ostream &os, const Struct &str);
};

// Struct to stream output
inline std::ostream &operator<<(std::ostream &os, const Struct &str) {
    os << str.getTemplate(true);
    os << str.getFullName();
    os << " {";
    for (const auto &field : str.fields) {
        os << "\n    " << field;
    }
    os << "\n}";
    return os;
}

inline LocationNode::operator std::string() const {
    std::string str = name;
    if (type == LocationNodeType::STURCT) {
        str += associated_struct->getTemplate();
    }
    return str;
}
