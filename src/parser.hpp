#include <string>

struct Struct;

enum class LocationNodeType { STRUCT, NAMESPACE };

struct LocationNode {
    std::string name;
    LocationNodeType type;
    Struct *associated_struct = nullptr;

    bool isTemplated() const;

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

    std::string getTemplateHeading() const;

    std::string getLocation(bool include_name) const;
    std::string getName() const;

    bool isNestedInTemplates() const;

    friend std::ostream &operator<<(std::ostream &os, const Struct &str);
};

// Struct to stream output
inline std::ostream &operator<<(std::ostream &os, const Struct &str) {
    os << str.getTemplateHeading();
    os << str.getLocation(true);
    os << " {";
    for (const auto &field : str.fields) {
        os << "\n    " << field;
    }
    os << "\n}";
    return os;
}

inline LocationNode::operator std::string() const {
    if (type == LocationNodeType::STRUCT) {
        return associated_struct->getName();
    } else {
        return name;
    }
}
