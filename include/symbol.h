#pragma once

#include "token.h"
#include "ast.h"
#include <vector>
#include <string>

enum class symbol_type{
    FUNCTION,
    GLOBAL,
    LOCAL,
    LABEL,
    CONSTRUCTOR,
    METHOD,

};

enum class symbol_table_type{
    GLOBAL,
    FUNCTION,
    BLOCK,
    STRUCT,
    CONSTRUCTOR,
    METHOD,

};

class symbol_table;
class ast_node;
class symbol{
    public:

    symbol_type type;
    std::string name;
    int offset;
    int size;
    std::vector<int> array_sizes;
    int element_size;
    int base_offset;
    class symbol_table* parent_struct;
    class symbol_table* memberof_struct;
    class symbol_table* constructor_table;
    class symbol_table* method_table;
    class ast_node* method_node;
    class ast_node* constructor_node;
    bool is_array=false;

    symbol();

    symbol(symbol_type type);

    symbol(std::string name);

    symbol(std::string name, int offset);

    std::string get_address();

    inline std::string get_nasm_label();

    private:
};

class symbol_table{
    public:

    symbol_table_type type;
    std::vector<symbol*> table;
    std::vector<symbol_table*> structs;
    std::vector<symbol*> struct_fields;
    std::vector<symbol*> labels;
    std::vector<symbol*> constructors;
    std::vector<symbol*> methods;
    symbol_table* parent;
    symbol_table* parent_struct;
    int cur_offset,locals_count=0;
    int total_size=0;
    int param_count=0;
    std::string name;

    symbol_table(symbol_table_type type);

    symbol_table(symbol_table_type type, symbol_table* parent, int offset);

    symbol* lookup(std::string identifier);

    void inc_locals_count();

    void increase_total_size(int size);

    void push(symbol* s);

    void push_struct(symbol_table* s);

    void push_label(symbol* s);

    symbol_table* push_constructor(symbol* s);

    symbol_table* push_method(symbol* s);

    symbol_table* lookup_struct(std::string name);

    symbol_table* lookup_parent_struct();

    symbol* lookup_field(std::string field_name, std::string struct_ref);

    symbol* lookup_constructor(std::string struct_name, int argcount);

    symbol* lookup_method(std::string name, std::string struct_name, int argcount);

    symbol* lookup_label(std::string label);

    void push_params(std::vector<token> &params);

    private:
};