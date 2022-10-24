#include "parser.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

#define VERSION "Rice metacompiler v0.1.0"

template <typename T> using uptr = std::unique_ptr<T>;

std::vector<std::string> split(const std::string &target, char c) {
    std::string temp;
    std::stringstream ss{target};
    std::vector<std::string> result;

    while (std::getline(ss, temp, c)) {
        result.push_back(temp);
    }

    return result;
}

int parsePositiveInt(const std::string &s) {
    try {
        std::size_t pos;
        int result = std::stoi(s, &pos);
        if (pos != s.size())
            return -1;
        return result;
    } catch (const std::invalid_argument e) {
        return -1;
    }
}

std::string Field::getAttributes() const {
    std::string attributes_vector = "{";
    for (auto &attribute : attributes) {
        attributes_vector += attribute + ", ";
    }
    return attributes_vector + "}";
}

bool LocationNode::isTemplated() const {
    return type == LocationNodeType::STRUCT && !associated_struct->template_params.empty();
}

using TemplateDeclarationHierarchy = std::vector<TemplateDeclaration>;

std::string Struct::getTemplateHeading() const {
    std::string templateStr;
    if (!template_params.empty()) {
        templateStr = "template <typename " + template_params.front().name;
        for (int i = 1; i < template_params.size(); i++) {
            templateStr += ", typename " + template_params.at(i).name;
        }
        return templateStr + "> ";
    }
    return templateStr;
}

std::string Struct::getLocation(bool include_name) const {
    using std::string;
    if (location.empty()) {
        return include_name ? getName() : "";
    }
    string loc = (string)location.at(0);
    for (int i = 1; i < location.size(); i++) {
        loc += "::" + (string)location.at(i);
    }
    return loc + "::" + getName();
}

std::string Struct::getName() const {
    std::string full_name = name;
    if (!template_params.empty()) {
        full_name += "<" + template_params.at(0).name;
        for (int i = 1; i < template_params.size(); i++) {
            full_name += ", " + template_params.at(i).name;
        }
    }
    return full_name + ">";
}

bool Struct::isNestedInTemplates() const {
    for (auto &location_node : location) {
        if (location_node.isTemplated()) {
            return true;
        }
    }
    return false;
}

class Parser {
    Location current_location;
    std::vector<std::unique_ptr<Struct>> current_struct_tree;
    TemplateDeclarationHierarchy current_template_declaration_hierarchy;
    std::vector<std::unique_ptr<Struct>> all_structs;
    std::stringstream &ss;
    int currentLevel = 0;

  public:
    explicit Parser(std::stringstream &ss) : ss(ss) { skipUntil('\n'); }

    // get line level in the AST
    int getLineLevel() {
        using namespace std;
        int lvl = 0;
        int ch;
        stringstream::pos_type pos;
        while (true) {
            pos = ss.tellg();
            ch = ss.get();
            if (ch == '|' || ch == '`' || ch == ' ') {
                lvl++;
                continue;
            }
            break;
        }
        ss.seekg(pos);
        return lvl / 2 + 1;
    }

    void skipAllChars(char ch) {
        // skip all 'ch' characters
        std::stringstream::pos_type pos;
        do {
            pos = ss.tellg();
        } while (ss.get() == ch);
        ss.seekg(pos);
    }

    // skip intil the chracter equals to ch
    void skipUntil(char ch) {
        while (ss.peek() != ch && ss.peek() != -1) {
            ss.get();
        }
        ss.get();
    }

    // get args from the definition (name, line, etc.)
    std::vector<std::string> extractArgs() {
        std::vector<std::string> args;
        std::string current_arg;
        int current_char;

        bool under_parenthesis = false;
        bool under_triangle_parenthesis = false;

        std::string args_raw;
        while (ss.peek() != '\n') {
            current_char = ss.get();

            if (current_char == '\'') {
                under_parenthesis = !under_parenthesis;
            } else if (current_char == '<') {
                under_triangle_parenthesis = true;
            } else if (current_char == '>') {
                under_triangle_parenthesis = false;
            }

            if (current_char == ' ' && !under_parenthesis && !under_triangle_parenthesis) {
                args.push_back(current_arg);
                current_arg.clear();
            } else {
                current_arg += (char)current_char;
            }
        }
        if (!current_arg.empty()) {
            args.push_back(current_arg);
        }
        return args;
    }

    // check if the stream starts with str
    bool startsWith(const std::string &str) {
        auto pos = ss.tellg();
        for (char ch : str) {
            if (ch != ss.get()) {
                ss.seekg(pos);
                return false;
            }
        }
        // reset to start
        ss.seekg(pos);
        return true;
    }

    // try to find str in stream, stop at 'end'
    bool tryFind(const std::string_view &str, char end = '\n') {
        int ch;
        auto str_pos = str.begin();
        auto pos = ss.tellg();
        do {
            ch = ss.get();
            if (ch == *str_pos) {
                str_pos++;
                if (str_pos == str.end()) {
                    ss.seekg(pos);
                    return true;
                }
                continue;
            }
            str_pos = str.begin();
        } while (ch != end);
        ss.seekg(pos);
        return false;
    }

    // perse one line on AST
    void parseLine(bool &is_struct_definition, LocationNode &location, bool &is_template_declaration) {
        // statement always starts with -
        skipAllChars('-');
        // struct or class declaration
        if (startsWith("CXXRecordDecl")) {

            // declaration needs to be not implicit and we only need structs and classes
            if (!tryFind("implicit") && (tryFind("struct") || tryFind("class"))) {

                TemplateDeclaration templateDeclaration;
                if (!current_template_declaration_hierarchy.empty()) {
                    templateDeclaration = current_template_declaration_hierarchy.back();
                }

                // get args
                std::vector<std::string> args = extractArgs();
                // we only need declarations with a definition
                if (args.back() == "definition") {
                    args.pop_back();
                    // type comes before 'definition' keyword
                    if (args.back() != "struct" && args.back() != "class") {
                        uptr<Struct> str;
                        Struct *raw_struct = new Struct{current_location, args.back(), {}, templateDeclaration};
                        str.reset(raw_struct);
                        current_struct_tree.push_back(std::move(str));
                        location = {args.back(), LocationNodeType::STRUCT, raw_struct};
                        is_struct_definition = true;
                    }
                }
            }
            // parse variable definitions
        } else if (startsWith("FieldDecl")) {
            // extract args
            std::vector<std::string> args = extractArgs();
            if (!current_struct_tree.empty()) {
                if (args.back() == "mutable") {
                    args.pop_back();
                }
                std::string type = args.back();
                type.erase(0, 1);
                size_t pos = type.find_first_of('\'');
                if (pos != std::string::npos) {
                    type = type.substr(0, pos);
                }
                // add to last struct
                current_struct_tree.back()->fields.push_back({args.at(args.size() - 2), type});
            }
            // parse annotations
        } else if (startsWith("AnnotateAttr")) {

            if (!current_struct_tree.empty()) {
                if (tryFind("\"reflectable\"")) {
                    // we only need to parse structs with reflectable attribute
                    current_struct_tree.back()->is_reflectable = true;
                } else if (!current_struct_tree.back()->fields.empty()) {
                    auto &last_field = current_struct_tree.back()->fields.back();
                    if (tryFind("\"not_reflectable\"")) {
                        // we don't need to parse not_reflectable fields
                        current_struct_tree.back()->fields.back().not_reflectable = true;
                    } else {
                        last_field.attributes.push_back(extractArgs().back());
                    }
                }
            }

            // parse namespaces
        } else if (startsWith("NamespaceDecl")) {
            std::vector<std::string> args = extractArgs();
            // remove inline arg if it exists
            if (args.back() == "inline") {
                args.pop_back();
            }
            // get namespace name
            location = {args.back(), LocationNodeType::NAMESPACE};
        } else if (startsWith("ClassTemplateDecl")) {
            // we found a template declaration, add an empty element to mark it
            is_template_declaration = true;
        } else if (startsWith("TemplateTypeParmDecl")) {

            // the format is as follows:
            // TemplateTypeParmDecl 0x7fffeb31bc28 <col:11, col:20> col:20 referenced typename depth 0 index 0 T

            if (current_template_declaration_hierarchy.empty()) {
                goto parseLine_finalize;
            }

            std::vector<std::string> args = extractArgs();

            if (parsePositiveInt(args.back()) != -1) {
                goto parseLine_finalize;
            }

            current_template_declaration_hierarchy.back().push_back(
                {parsePositiveInt(args.at(args.size() - 2)), parsePositiveInt(args.at(args.size() - 4)), args.back()});
        }
    parseLine_finalize:
        // skip until the end of the line
        skipUntil('\n');
    }

    // parse whole level
    void parseLevel(int targetLevel = 1) {
        do {
            // get line beginning
            auto line_begin = ss.tellg();
            // get current level
            currentLevel = getLineLevel();

            // we are on our target level
            if (currentLevel == targetLevel) {

                bool is_struct_definition = false;
                LocationNode last_location_node;
                bool is_template_declaration = false;

                // parse each line
                parseLine(is_struct_definition, last_location_node, is_template_declaration);

                // we parsed a struct or a namespace, add to the current location
                if (!last_location_node.name.empty()) {
                    current_location.push_back(last_location_node);
                    // parse next level
                    parseLevel(targetLevel + 1);
                    current_location.pop_back();
                }

                // we parsed a struct, add it to the list
                if (is_struct_definition) {
                    if (current_struct_tree.back()->is_reflectable) {
                        all_structs.push_back(std::move(current_struct_tree.back()));
                    }
                    current_struct_tree.pop_back();
                } else if (is_template_declaration) {
                    // we found a template, add to the current template tree
                    current_template_declaration_hierarchy.push_back({});
                    // parse next level
                    parseLevel(targetLevel + 1);
                    current_template_declaration_hierarchy.pop_back();
                }

            } else {
                // reset to line beginning before parsing next or prev level
                ss.seekg(line_begin);
                if (currentLevel > targetLevel) {
                    parseLevel(targetLevel + 1);
                } else {
                    break;
                }
            }

        } while (ss.peek() != -1);
    }

    void dump() const {
        for (auto &s : all_structs) {
            std::cout << *s << "\n\n";
        }
    }

    // generate code for reflectionHelper from the parsed structs
    std::string generateMetaCode(const std::string &header_file) const {
        std::stringstream generated_code;
        std::string type_string;
        std::string field_string;
        std::string full_name;

        generated_code << "#pragma once\n\n";
        generated_code << "#include \"" << header_file << "\"\n";
        generated_code << "#include <MetaCompiler/ReflectionHelper.hpp>\n\n";

        for (auto &str : all_structs) {

            if (str->isNestedInTemplates()) {
                std::cout << "WARNING: structs nested in templated structs are not supported(yet), affected struct: " +
                                 str->getName() + "\n";
            }

            field_string.clear();
            full_name = str->getLocation(true);
            generated_code << str->getTemplateHeading() << " struct Meta::TypeOf<" + full_name;
            generated_code << "> {\n";
            type_string = "Type<" + full_name;
            for (auto &field : str->fields) {
                if (field.not_reflectable) {
                    continue;
                }
                type_string += ", " + field.type;
                field_string += ", \n    {\"" + field.name + "\", &" + full_name + "::" + field.name + ", " +
                                field.getAttributes() + "}";
            }
            type_string += ">";
            generated_code << "    " << type_string << " type() { \n    return " << type_string
                           << "{Types::Struct,\n    "
                           << "\"" << str->getLocation(false) << "\", "
                           << "\"" << str->name << "\"" << field_string << "}; }\n};\n";
        }
        return generated_code.str();
    }
};

std::string exec(const std::string_view &cmd) {
    std::shared_ptr<FILE> pipe(popen(cmd.data(), "r"), pclose);
    if (!pipe)
        return "ERROR";
    constexpr size_t buf_size = 0x100000; // 1MB buffer
    char buffer[buf_size];
    std::string result = "";
    while (!feof(pipe.get())) {
        if (fgets(buffer, buf_size, pipe.get()) != NULL)
            result += buffer;
    }
    return result;
}

int main(int argc, char *argv[]) {
    using json = nlohmann::json;
    using namespace std;

    string compileCommandsFile;

    string sourceFile;
    string sourceFileAbsPath;

    string headerFile;
    string headerFileName;

    string curr_arg;

    bool print_to_console = false;
    bool dump_ast = false;

    for (int i = 0; i < argc; i++) {
        curr_arg = std::string(argv[i]);
        if (curr_arg == "--version") {
            cout << VERSION << endl;
            exit(0);
        } else if (curr_arg == "--help") {
            cout << "OVERVIEW: " << VERSION << "\n\n";
            cout << "USAGE: RiceMetaCompiler [options]\n\n";
            cout << "OPTIONS: \n";
            cout << "  --help                    Display this help page\n";
            cout << "  --version                 Print version string\n";
            cout << "  --print                   Print generated code to stdout\n";
            cout << "  --dump                    Dump generated AST to a file\n";
            cout << "  header_file_path=[path]   Path to the header file to build the AST for\n";
            cout << "  source_file_path=[path]   Path to the source file, used with 'compile_commands_path' to "
                    "search for additional includes and parameters\n";
            cout << "  compile_commands_path=    Path to compile_commands.json\n";
            exit(0);
        } else if (curr_arg.starts_with("header_file_path=")) {
            headerFile = curr_arg.substr(17);
            filesystem::path headerFilePath = headerFile;
            headerFileName = headerFilePath.stem().string();
        } else if (curr_arg.starts_with("source_file_path=")) {
            sourceFile = curr_arg.substr(17);
            filesystem::path sourceFilePath = sourceFile;
            sourceFileAbsPath = filesystem::absolute(sourceFilePath);
        } else if (curr_arg.starts_with("compile_commands_path=")) {
            compileCommandsFile = curr_arg.substr(22);
        } else if (curr_arg == "--print") {
            print_to_console = true;
        } else if (curr_arg == "--dump") {
            dump_ast = true;
        }
    }

    if (system("clang++ -v") == -1) {
        cout << "No clang++ found, exiting\n";
        exit(1);
    }

    if (!headerFile.length()) {
        cout << "\n\nPlease set header_file_path\n";
        exit(1);
    }

    if (!sourceFile.length() && compileCommandsFile.length()) {
        cout << "\n\nPlease set source_file_path\n";
        exit(1);
    }

    cout << "\nRunning on " << filesystem::absolute(headerFile) << "\n\n";

    string additional_params;

    if (compileCommandsFile.length()) {
        ifstream compileCommandsStream(compileCommandsFile);
        json compileCommandsJson = json::parse(compileCommandsStream);

        cout << "Including additonal params:";

        for (const auto &[command_index, compile_command] : compileCommandsJson.items()) {
            if (filesystem::absolute(compile_command["file"]) == sourceFileAbsPath) {
                vector<string> params = split(compile_command["command"], ' ');
                int param_index = 0;
                for (const auto &param : params) {
                    if (param == "-o") {
                        break;
                    }
                    if (param_index >= 1) {
                        additional_params += " " + param;
                        cout << param << "\n";
                    }
                    param_index++;
                }
                break;
            }
        }
    }

    auto start_clang = chrono::steady_clock::now();
    stringstream ss(exec("clang++" + additional_params +
                         " -Xclang -ast-dump -fsyntax-only -fno-color-diagnostics -Wno-visibility -std=c++17 '" +
                         headerFile + "'"));
    auto end_clang = chrono::steady_clock::now();

    if (dump_ast) {
        ofstream ast_file(headerFileName + "_ast");
        ast_file << ss.str();
        ast_file.close();
    }

    auto start_parse = chrono::steady_clock::now();

    Parser parser(ss);
    parser.parseLevel();

    if (print_to_console) {
        parser.dump();
    }

    ofstream meta(headerFileName + "_meta.hpp");
    meta << parser.generateMetaCode(headerFile);
    meta.close();

    cout << "\nBuilt in: "
         << chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start_parse).count() << "ms + "
         << chrono::duration_cast<chrono::milliseconds>(end_clang - start_clang).count() << "ms clang ast generation\n";

    return 0;
}