#include <string>
#include <vector>
#include <cstdlib>

#include "symbol.h"
#include "utils.h"
#include "ast.h"

symbol::symbol()
    : parent_struct(nullptr){}

symbol::symbol(symbol_type type)
    : type(type), parent_struct(nullptr){}

symbol::symbol(std::string name)
    : name(name), parent_struct(nullptr){}

symbol::symbol(std::string name, int offset)
    : name(name), offset(offset), parent_struct(nullptr){}

std::string symbol::get_address(){
    if (type==symbol_type::GLOBAL || type==symbol_type::FUNCTION || 
        type==symbol_type::LABEL || type==symbol_type::CONSTRUCTOR || 
        type==symbol_type::METHOD){
        return get_nasm_label();
    }

    std::string address="RBP";

    address+=offset>=0?"+":"-";

    std::string absoffset=std::to_string(std::abs(offset));

    address+=absoffset;

    return address;

}

symbol_table::symbol_table(symbol_table_type type)
    : type(type), parent(nullptr){}

symbol_table::symbol_table(symbol_table_type type, symbol_table* parent, int offset)
    : type(type), parent(parent), cur_offset(offset){}

symbol* symbol_table::lookup(std::string identifier){
    for (int i=0;i<table.size();i++){
        if (table.at(i)->name==identifier)
            return table.at(i);

    }
    if (parent!=nullptr)
        return parent->lookup(identifier);

    return nullptr;
}

symbol_table* symbol_table::lookup_struct(std::string identifier){
    for (int i=0;i<structs.size();i++){
        if (structs.at(i)->name==identifier){
            return structs.at(i);
        }
    }
    if (parent){
        return parent->lookup_struct(identifier);
    }

    return nullptr;

}

symbol* symbol_table::lookup_field(std::string field_name, std::string struct_ref){
    for (int i=0;i<struct_fields.size();i++){
        symbol* field=struct_fields.at(i);
        
        if (!struct_ref.empty()){
            if (field->name==field_name && field->memberof_struct->name==struct_ref)return field;

        }
        else{
            if (field->name==field_name)return field;
            
        }
        

    }

    if (parent!=nullptr)return parent->lookup_field(field_name,struct_ref);
    
    return nullptr;

}

symbol* symbol_table::lookup_label(std::string label){
    for (int i=0;i<labels.size();i++){
        if (labels.at(i)->name==label)return labels.at(i);
    }
    
    if (parent!=nullptr)return parent->lookup_label(label);

    return nullptr;
}

void symbol_table::inc_locals_count(){
    if (type==symbol_table_type::GLOBAL)return;

    if (type==symbol_table_type::FUNCTION){
        locals_count++;

    }
    else{
        parent->inc_locals_count();
    }
    
}

void symbol_table::increase_total_size(int size){
    if (type==symbol_table_type::FUNCTION || type==symbol_table_type::STRUCT || type==symbol_table_type::CONSTRUCTOR || type==symbol_table_type::METHOD){
        total_size+=size;
    }
    else if (parent){
        parent->increase_total_size(size);
    }
}

void symbol_table::push(symbol* s){
    table.push_back(s);
    if (s->type==symbol_type::LOCAL){ 
        increase_total_size(s->size);

    }
    
}

symbol_table* symbol_table::push_constructor(symbol* s){
    symbol_table* cur=this;

    while (cur->type!=symbol_table_type::GLOBAL){
        cur=cur->parent;
    }

    cur->constructors.push_back(s);

    return cur;

}

symbol_table* symbol_table::push_method(symbol* s){
    symbol_table* cur=this;
    while (cur->type!=symbol_table_type::GLOBAL)cur=cur->parent;

    cur->methods.push_back(s);

    return cur;

}

inline std::string symbol::get_nasm_label(){
    if (type==symbol_type::CONSTRUCTOR){
        return "??"+name+std::to_string(constructor_table->param_count);
    }
    if (type==symbol_type::METHOD){
        return "??"+name+std::to_string(method_table->param_count);
    }
    return '?'+name;
}

void symbol_table::push_params(std::vector<token> &params){
    int offset;

    if (type==symbol_table_type::CONSTRUCTOR || type==symbol_table_type::METHOD)offset=24;
    else offset=16;
    
    for (int i=0;i<params.size();i++){
        param_count++;

        symbol* s=new symbol(symbol_type::LOCAL);
        s->offset=offset;
        s->name=params[i].value;
        
        offset+=8;

        table.push_back(s);

    }
    
}

void symbol_table::push_struct(symbol_table* struct_table){
    symbol_table* cur=this;
    while (cur->type!=symbol_table_type::GLOBAL)cur=cur->parent;

    cur->structs.push_back(struct_table);

    for (int i=0;i<struct_table->table.size();i++){
        symbol* field=struct_table->table.at(i);
        field->memberof_struct=struct_table;
        cur->struct_fields.push_back(field);

    }

}

void symbol_table::push_label(symbol* s){
    labels.push_back(s);

}

symbol* symbol_table::lookup_constructor(std::string struct_name, int argcount){
    symbol_table* cur=this;
    while (cur->type!=symbol_table_type::GLOBAL)cur=cur->parent;

    for (int i=0;i<cur->constructors.size();i++){
        if (cur->constructors.at(i)->name==struct_name && 
        cur->constructors.at(i)->constructor_table->param_count==argcount){
            return cur->constructors.at(i);
        }
        
    }
    return nullptr;
    
}

symbol* symbol_table::lookup_method(std::string name, std::string struct_name, int argcount){
    symbol_table* cur=this;

    while (cur->type!=symbol_table_type::GLOBAL)cur=cur->parent;

    for (int i=0;i<cur->methods.size();i++){
        if (!struct_name.empty()){
            if (cur->methods.at(i)->name==name && cur->methods.at(i)->memberof_struct->name==struct_name && 
            cur->methods.at(i)->method_table->param_count==argcount){
                return cur->methods.at(i);
            }

        }
        else{
            if (cur->methods.at(i)->name==name && cur->methods.at(i)->method_table->param_count==argcount){
                return cur->methods.at(i);
            }
        }
    }
    
    return nullptr;

}