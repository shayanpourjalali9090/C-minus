#include <vector>
#include <iostream>
#include <string>
#include <cstdlib>
#include <stdio.h>
#include <fstream>
#include <cstdint>
#include <filesystem>

#include "utils.h"
#include "token.h"
#include "ast.h"
#include "symbol.h"
#include "x86.h"

std::string input_file,output_file;

void print_help(){
    printf("C- Compiler\n\nUsage: cm [INPUT_FILE] [OUTPUT_FILE] [OPTIONS]...\n\nOptions:\n\t--help\n\t    Print help information.\n\t-a\n\t    Output asm file only\n\n");

}

std::string get_file_as_string(std::string file_name){
    std::ifstream s(file_name);
    std::string ret,line;
    
    while (getline(s,line)){
        ret=ret+line+"\n";

    }

    return ret;

}

std::string get_dir(std::string filename){
    int i;
    for (i=filename.size()-1;i>=0 && filename[i]!='/';i--);
    if (filename[i]!='/')return "";
    return filename.substr(0,i+1);

}

std::string get_file_as_string(std::string dir, std::string filename){
    if (dir.empty())return get_file_as_string(filename);
    return get_file_as_string(dir+"/"+filename);
}

token_list lex(std::string &file){
    token_list list;
    std::string buf;

    int line_number=1;

    for (uint32_t i=0;i<file.length();++i){
        char c=file[i];

        if (!is_delimiter(c)){
            buf+=c;
        }
        else{
            if (!buf.empty()){
                token t(buf);
                t.line_number=line_number;

                list.push(t);
                buf.clear();
            }

            switch (c){
                case ' ':{
                    continue;
                }
                case '\n':{
                    line_number++;
                    continue;
                }
                case '\t':{
                    continue;
                }
                case ',':{
                    token t2(token_type::DELIMITER);
                    t2.value=",";
                    t2.line_number=line_number;
                    list.push(t2);
                    continue;
                }
                case '"':{
                    i++;
                    
                    while (i<file.length() && file[i]!='"'){
                        if (file[i]=='\n'){
                            i++;
                            continue;
                        }
                        buf+=file[i++];
                    }

                    if (i==file.length()){
                        emit_error(error::LEXICAL_ERROR,"Expected closing \"",line_number);
                    }

                    token t2(token_type::STRING,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();

                    continue;
                }
                case '\'':{
                    i++;
                    if (i==file.length()){
                        emit_error(error::LEXICAL_ERROR,"Expected a character after '",line_number);
                    }

                    if (file[i]=='\\'){
                        i++;
                        
                        if (i==file.length())
                            emit_error(error::LEXICAL_ERROR,"Expected character after \\",line_number);
                        
                        char special=file[i];
                        
                        if (is_numeric(special)){
                            while (i<file.length() && file[i]!='\''){
                                buf+=file[i++];
                            }
                            if (i==file.length()){
                                emit_error(error::LEXICAL_ERROR,"Expected closing '",line_number);
                            }

                            token t2(token_type::NUMBER,buf);
                            t2.line_number=line_number;
                            list.push(t2);

                            buf.clear();
                            

                        }
                        else{
                            int num;
                            switch (special){
                                case 'n':{
                                    num='\n';
                                    break;
                                }
                                case 't':{
                                    num='\t';
                                    break;
                                }
                                case '\'':{
                                    num='\'';
                                    break;
                                }
                                case '"':{
                                    num='"';
                                    break;
                                }
                                case '\\':{
                                    num='\\';
                                    break;
                                }
                                //....................................................................
                            }
                            buf=std::to_string(num);

                            i++;
                            if (i==file.length() || file[i]!='\''){
                                emit_error(error::LEXICAL_ERROR,"Expected closing '",line_number);
                            }

                            token t2(token_type::NUMBER,buf);
                            t2.line_number=line_number;
                            list.push(t2);
                            buf.clear();
                            
                            
                        }

                    }
                    else{
                        buf+=file[i++];

                        if (i==file.length() || file[i]!='\''){
                            emit_error(error::LEXICAL_ERROR,"Expected closing '",line_number);
                        }
                        token t2(token_type::NUMBER,std::to_string((int)buf[0]));
                        t2.line_number=line_number;
                        list.push(t2);
                    
                        buf.clear();
                        
                    }

                    continue;

                }
                case '+':{
                    buf='+';
                    if (i!=file.length()-1){
                        if (file[i+1]=='+' || file[i+1]=='='){
                            buf+=file[i+1];
                            i++;

                        }

                    }
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '-':{
                    buf='-';
                    if (i!=file.length()-1){
                        if (file[i+1]=='-' || file[i+1]=='=' || file[i+1]=='>'){
                            buf+=file[i+1];
                            i++;
                        }
                    }
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '*':{
                    buf='*';
                    if (i!=file.length()-1){
                        if (file[i+1]=='='){
                            buf+=file[i+1];
                            i++;
                        }
                    }
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '/':{
                    buf='/';
                    if (i!=file.length()-1){
                        if (file[i+1]=='='){
                            buf+=file[i+1];
                            i++;
                        }
                        else if (file[i+1]=='/'){
                            while (i<file.length() && file[i]!='\n'){
                                i++;
                            }
                            buf.clear();
                            if (file[i]=='\n')line_number++;
                            continue;
                        }
                        else if (file[i+1]=='*'){
                            i+=2;
                            int tmp_line_number=line_number;
                            while (i<file.length()-1 && (file[i]!='*' || file[i+1]!='/')){
                                if (file[i]=='\n')line_number++;
                                i++;
                            }
                            if (i==file.length()-1){
                                emit_error(error::LEXICAL_ERROR, "Expected closing */",tmp_line_number);
                            }
                            i++;
                            buf.clear();
                            continue;
                        }
                    }
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '>':{
                    buf='>';
                    if (i!=file.length()-1){
                        if (file[i+1]=='=' || file[i+1]=='>'){
                            buf+=file[i+1];
                            i++;
                        }
                        if (file[i]=='>' && i<file.length()-1 && file[i+1]=='='){
                            buf+='=';
                            i++;
                        }
                    }
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '<':{
                    buf='<';
                    if (i!=file.length()-1){
                        if (file[i+1]=='<' || file[i+1]=='='){
                            buf+=file[i+1];
                            i++;
                        }
                        if (file[i]=='<' && i<file.length()-1 && file[i+1]=='='){
                            buf+='=';
                            i++;
                        }
                    }
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '=':{
                    buf='=';
                    if (i!=file.length()-1){
                        if (file[i+1]=='=' || file[i+1]=='>'){
                            buf+=file[i+1];
                            i++;
                        }
                    }
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '@':{
                    buf='@';

                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '~':{
                    buf='~';

                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '&':{
                    buf='&';
                    if (i!=file.length()-1){
                        if (file[i+1]=='&' || file[i+1]=='='){
                            buf+=file[i+1];
                            i++;
                        }
                    }
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '|':{
                    buf='|';
                    if (i!=file.length()-1){
                        if (file[i+1]=='|' || file[i+1]=='='){
                            buf+=file[i+1];
                            i++;
                        }
                    }
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '^':{
                    buf='^';
                    if (i!=file.length()-1){
                        if (file[i+1]=='='){
                            buf+=file[i+1];
                            i++;
                        }
                    }
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case ':':{
                    buf=':';
                    
                    token t2(token_type::DELIMITER,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case ';':{
                    buf=';';
                    token t2(token_type::DELIMITER,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '%':{
                    buf='%';
                    if (i!=file.length()-1){
                        if (file[i+1]=='='){
                            buf+=file[i+1];
                            i++;
                        }
                    }
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '!':{
                    buf='!';
                    if (i!=file.length()-1){
                        if (file[i+1]=='='){
                            buf+=file[i+1];
                            i++;
                        }
                    }
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '?':{
                    buf='?';
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '(':{
                    buf='(';
                    token t2(token_type::DELIMITER,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case ')':{
                    buf=')';
                    
                    token t2(token_type::DELIMITER,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '[':{
                    buf='[';
                    token t2(token_type::DELIMITER,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case ']':{
                    buf=']';
                    
                    token t2(token_type::DELIMITER,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '{':{
                    buf='{';
                    
                    token t2(token_type::DELIMITER,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '}':{
                    buf='}';
                    
                    token t2(token_type::DELIMITER,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '$':{
                    buf='$';
                    
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                case '.':{
                    buf='.';
                    
                    token t2(token_type::OPERATOR,buf);
                    t2.line_number=line_number;
                    list.push(t2);

                    buf.clear();
                    continue;

                }
                default:break;

            }

        }

    }
    if (!buf.empty()){
        token t(buf);

        list.push(t);

    }
    return list;

}

ast_node* parse_statement(token_list &list);
ast_node* parse_expression(token_list &list, bool statement);
ast_node* parse_logical_or(token_list &list);
ast_node* parse_shift(token_list &list);

ast_node* parse_primary(token_list &list){
    // std::cout << "In parse primary: " << list.peek().to_string() << '\n';
    if (list.match(token_type::NUMBER)){
        ast_node* node=new ast_node(ast_node_type::NUMBER);
        
        node->value=list++.value; //advance past number literal

        return node;
    }
    else if (list.match(token_type::IDENTIFIER)){
        ast_node* node=new ast_node(ast_node_type::IDENTIFIER);

        node->identifier=list++.value;
        
        if (list.match("(")){
            int tmp_line_number=list.peek().line_number;
            list++; //advance past '('
            node->type=ast_node_type::FUNCTION_CALL;
            
            while (!list.end() && !list.match(")")){
                ast_node* param=parse_expression(list,false);
                node->args.push(param);

                if (list.match(")"))break;
                list.expect(",");

            }

            list.expect(")");

        }

        return node;

    }
    else if (list.match("this")){
        ast_node* node=new ast_node(ast_node_type::THIS);
        list++;
        return node;
    }
    else if (list.match("sizeof")){
        ast_node* node=new ast_node(ast_node_type::SIZEOF);
        list++;

        list.expect("(");
        token t=list.expect(token_type::IDENTIFIER);
        list.expect(")");

        node->identifier=t.value;
        return node;
    }
    else if (list.match(token_type::REGISTER)){
        ast_node* node=new ast_node(ast_node_type::REGISTER);
        node->value=list++.value;

        return node;
    }
    else if (list.match("(")){
        list++;

        ast_node* node=parse_expression(list,false);

        list.expect(")");

        return node;
    }
    else if (list.match("true") || list.match("false")){
        ast_node* node=new ast_node(ast_node_type::NUMBER);
        node->value=list.match("true")?"1":"0";
        list++;
        return node;
    }
    else if (list.match("NULL")){
        ast_node* node=new ast_node(ast_node_type::NUMBER);
        node->value="0";
        list++;
        return node;
    }
    else if (list.match(token_type::STRING)){
        ast_node* node=new ast_node(ast_node_type::STRING);
        node->value=list.expect(token_type::STRING).value;
        return node;
    }
    // printf("Returning nullptr\n");
    return nullptr;

}

ast_node* parse_member_access(token_list &list){
    ast_node* node=parse_primary(list);

    if (node && (node->type==ast_node_type::IDENTIFIER || node->type==ast_node_type::FUNCTION_CALL || node->type==ast_node_type::THIS) && list.match("@")){
        list++;
        token t=list++;
        if (t.type!=token_type::IDENTIFIER){
            emit_error(error::SYNTAX_ERROR,"Expected identifier after '@', to specify struct",t.line_number);
        }
        node->struct_ref=t.value;

    }
    if (list.match(".") || list.match("->")){
        ast_node* node2=new ast_node(ast_node_type::MEMBER_ACCESS);
        int lineno=list.peek().line_number;
        node2->value=list++.value;

        node2->left=node;
        if (!node2->left || (node2->left->type!=ast_node_type::IDENTIFIER && node2->left->type!=ast_node_type::THIS)){
            emit_error(error::SYNTAX_ERROR,"Expected identifier in member access",lineno);
        }

        node2->right=parse_member_access(list);

        node=node2;
    }

    return node;
}

ast_node* parse_array_access(token_list &list){
    ast_node* node=parse_member_access(list);

    if (node && list.match("[") && (node->type==ast_node_type::IDENTIFIER || node->type==ast_node_type::MEMBER_ACCESS)){
        ast_node* node2=new ast_node(ast_node_type::ARRAY_ACCESS);

        do{
            list.expect("[");
            ast_node* index=parse_expression(list,false);
            list.expect("]");

            node2->indexes.push(index);

        }while (list.match("["));

        node2->left=node;
        node=node2;

    }

    return node;
}

ast_node* parse_ptr_access(token_list &list){
    ast_node* node=parse_array_access(list);

    if (list.match("$")){
        ast_node* node2=new ast_node(ast_node_type::PTR_ACCESS);

        list.expect("$");
        node2->element_size_reference=parse_primary(list);

        do{
            list.expect("[");
            ast_node* index=parse_expression(list,false);
            node2->indexes.push(index);
            list.expect("]");

        }while(list.match("["));

        node2->left=node;
        node=node2;

    }

    return node;

}

ast_node* parse_postfix(token_list &list){
    ast_node* node=parse_ptr_access(list);

    if (list.match("++") || list.match("--")){
        ast_node* node2=new ast_node(ast_node_type::EXPRESSION);
        node2->value=list++.value;

        node2->left=node;
        node=node2;
    }

    return node;
}

ast_node* parse_unary(token_list &list){
    if (list.match("+") || list.match("-") || list.match("!") || 
        list.match("~") || list.match("@") || list.match("++") || 
        list.match("--") || list.match("*")){
        ast_node* node=new ast_node(ast_node_type::UNARY);

        node->value=list++.value;
        
        node->right=parse_unary(list);

        return node;

    }

    return parse_postfix(list);

}

ast_node* parse_multiplicative(token_list &list){
    ast_node* node=parse_unary(list);

    while (list.match("*") || list.match("/") || list.match("%")){
        ast_node* new_node=new ast_node(ast_node_type::EXPRESSION);
        new_node->value=list++.value;

        new_node->left=node;
        new_node->right=parse_unary(list);
        
        node=new_node;
    }

    return node;
}

ast_node* parse_additive(token_list &list){
    ast_node* node=parse_multiplicative(list);

    while (list.match("+") || list.match("-")){
        ast_node* new_node=new ast_node(ast_node_type::EXPRESSION);
        new_node->value=list++.value;

        new_node->left=node;
        new_node->right=parse_multiplicative(list);
        
        node=new_node;
    }

    return node;
}

ast_node* parse_shift(token_list &list){
    ast_node* node=parse_additive(list);

    while (list.match(">>") || list.match("<<")){
        ast_node* new_node=new ast_node(ast_node_type::EXPRESSION);
        new_node->value=list++.value;

        new_node->left=node;
        new_node->right=parse_additive(list);
        
        node=new_node;
    }

    return node;
}

ast_node* parse_comparison(token_list &list){
    ast_node* node=parse_shift(list);

    while (list.match(">") || list.match(">=") || list.match("<") || list.match("<=")){
        ast_node* new_node=new ast_node(ast_node_type::EXPRESSION);
        new_node->value=list++.value;

        new_node->left=node;
        new_node->right=parse_shift(list);
        
        node=new_node;
    }
    
    return node;
}

ast_node* parse_equality(token_list &list){
    ast_node* node=parse_comparison(list);

    while (list.match("==") || list.match("!=")){
        ast_node* new_node=new ast_node(ast_node_type::EXPRESSION);
        new_node->value=list++.value;

        new_node->left=node;
        new_node->right=parse_comparison(list);
        
        node=new_node;
    }

    return node;
}

ast_node* parse_bitwise_and(token_list &list){
    ast_node* node=parse_equality(list);

    while (list.match("&")){
        ast_node* new_node=new ast_node(ast_node_type::EXPRESSION);
        new_node->value=list++.value;

        new_node->left=node;
        new_node->right=parse_equality(list);
        
        node=new_node;
    }

    return node;
}

ast_node* parse_xor(token_list &list){
    ast_node* node=parse_bitwise_and(list);

    while (list.match("^")){
        ast_node* new_node=new ast_node(ast_node_type::EXPRESSION);
        new_node->value=list++.value;

        new_node->left=node;
        new_node->right=parse_bitwise_and(list);
        
        node=new_node;
    }

    return node;
}

ast_node* parse_bitwise_or(token_list &list){
    ast_node* node=parse_xor(list);

    while (list.match("|")){
        ast_node* new_node=new ast_node(ast_node_type::EXPRESSION);
        new_node->value=list++.value;

        new_node->left=node;
        new_node->right=parse_xor(list);
        
        node=new_node;
    }
    
    return node;
}

ast_node* parse_logical_and(token_list &list){
    ast_node* node=parse_bitwise_or(list);

    while (list.match("&&")){
        ast_node* new_node=new ast_node(ast_node_type::EXPRESSION);
        new_node->value=list++.value;

        new_node->left=node;
        new_node->right=parse_logical_or(list);
        
        node=new_node;
    }

    return node;
}

ast_node* parse_logical_or(token_list &list){
    ast_node* node=parse_logical_and(list);

    while (list.match("||")){
        ast_node* new_node=new ast_node(ast_node_type::EXPRESSION);
        new_node->value=list++.value;

        new_node->left=node;
        new_node->right=parse_logical_and(list);
        
        node=new_node;
    }

    return node;
}

ast_node* parse_ternary(token_list &list){
    ast_node* node=parse_logical_or(list);

    if (list.match("?")){
        ast_node* new_node=new ast_node(ast_node_type::EXPRESSION);
        new_node->value=list++.value;

        new_node->condition=node;
        new_node->true_node=parse_expression(list,false);
        list.expect(":");
        new_node->false_node=parse_expression(list,false);
        node=new_node;
    }

    return node;
}

ast_node* parse_equals(token_list &list){
    ast_node *node=parse_ternary(list);

    if (list.match("=") || list.match("+=") || list.match("-=") || 
        list.match("*=") || list.match("/=") || list.match("^=") || 
        list.match("&=") || list.match("|=") || list.match("%=") || 
        list.match(">>=") || list.match("<<=") || list.match("=>")){
        ast_node* new_node=new ast_node(ast_node_type::EXPRESSION);

        new_node->value=list++.value;

        new_node->left=node;
        new_node->right=parse_equals(list);

        node=new_node;

    }

    return node;
}

ast_node* parse_chain(token_list &list){
    ast_node* node=parse_equals(list);

    if (list.match(",")){
        ast_node* node2=new ast_node(ast_node_type::EXPRESSION);
        node2->value=list++.value;

        node2->left=node;
        node2->right=parse_chain(list);

        node=node2;

    }

    return node;

}

ast_node* parse_expression(token_list &list, bool statement){
    ast_node* node=statement?parse_chain(list):parse_equals(list);
    return node;
}

ast_node* parse_array(token_list &list){
    ast_node* node=new ast_node(ast_node_type::ARRAY);
    list.expect("{");

    do{
        ast_node* element;

        if (list.match("{")){
            element=parse_array(list);
        }
        else{
            element=parse_expression(list,false);
        }
        node->elements.push(element);

        if (list.match("}")){
            break;
        }
        list.expect(",");
        

    }while(!list.match("}"));

    list.expect("}");

    return node;

}

ast_node* parse_declaration(token_list &list){
    list++;
    
    ast_node* node=new ast_node(ast_node_type::DECLARATION);

    node->identifier=list.expect(token_type::IDENTIFIER).value;

    if (list.match("$")){
        list++;

        int lineno=list.peek().line_number;

        node->type=ast_node_type::ARRAY_DECLARATION;
        node->element_size_reference=parse_expression(list,false);

        if (node->element_size_reference==nullptr || 
            (node->element_size_reference->type!=ast_node_type::NUMBER && node->element_size_reference->type!=ast_node_type::SIZEOF)){
            emit_error(error::SYNTAX_ERROR,"Element size reference must be constant in declaration",lineno);
        }

        int i=0;
        bool first_size_empty=false;
        do{
            list.expect("[");

            if (list.match("]")){
                if (i){
                    emit_error(error::SYNTAX_ERROR,"Empty size not allowed here",list.peek().line_number);
                }

                list.expect("]");
                first_size_empty=true;

                if (list.match("["))continue;
                else break;

            }

            int lineno2=list.peek().line_number;

            ast_node* index=parse_expression(list,false);
            if (index->type!=ast_node_type::NUMBER){
                emit_error(error::SYNTAX_ERROR,"Array size(s) must be constant in declaration", lineno2);
            }

            node->indexes.push(index);
            list.expect("]");

            i++;

        }while (list.match("["));

        if (list.match(";")){
            if (first_size_empty){
                emit_error(error::SYNTAX_ERROR,"Empty array size implies a declaration",list.peek().line_number);
            }
            list.expect(";");
            return node;
        }
        
        list.expect("=");

        node->right=parse_array(list);

        list.expect(";");

        if (first_size_empty){
            ast_node* firstsize=new ast_node(ast_node_type::NUMBER);
            firstsize->value=std::to_string(node->right->elements.size());

            node->indexes.insert(0,firstsize);
        }

        return node;

    }
    else if (list.match("@")){
        list++;
        node->struct_ref=list.expect(token_type::IDENTIFIER).value;
    }

    if (list.match(";")){
        list.expect(";");
        return node;
    }

    list.expect("=");

    node->right=parse_expression(list,false);

    list.expect(";");

    return node;

}

ast_node* parse_block(token_list &list){
    ast_node* node=new ast_node(ast_node_type::BLOCK);
    list++;
    
    while (!list.end() && !list.match("}")){
        ast_node* statement=parse_statement(list);
        if (statement!=nullptr)node->children.push(statement);
    }

    list.expect("}");

    return node;

}

ast_node* parse_function(token_list &list){
    ast_node* node=new ast_node(ast_node_type::FUNCTION_DEFINITION);
    list++;

    node->identifier=list.expect(token_type::IDENTIFIER).value;

    list.expect("(");

    while (!list.end() && !list.match(")")){
        token param=list.expect(token_type::IDENTIFIER);
        if (list.match("@")){
            list++;
            param.struct_ref=list.expect(token_type::IDENTIFIER).value;

        }
        node->params.push_back(param);

        if (list.match(")"))break;

        list.expect(",");

    }

    list.expect(")");

    list.expect("{");

    while (!list.end() && !list.match("}")){
        ast_node* statement=parse_statement(list);

        if (statement!=nullptr)node->children.push(statement);

    }

    list.expect("}");

    return node;
    
}

ast_node* parse_return(token_list &list){
    ast_node* node=new ast_node(ast_node_type::RETURN);

    list.expect("return");

    if (!list.match(";")){
        node->right=parse_expression(list,false);
    }

    list.expect(";");

    return node;

}

ast_node* parse_break(token_list &list){
    ast_node* node=new ast_node(ast_node_type::BREAK);
    list.expect("break");
    list.expect(";");

    return node;
}

ast_node* parse_continue(token_list &list){
    ast_node* node=new ast_node(ast_node_type::CONTINUE);
    list.expect("continue");
    list.expect(";");

    return node;
}

ast_node* parse_while(token_list &list){
    ast_node* node=new ast_node(ast_node_type::LOOP);
    list.expect("while");

    list.expect("(");

    node->condition=parse_expression(list,false);

    if (list.match(";")){
        list++;
        node->expressions=parse_expression(list,true);
    }
    list.expect(")");

    node->right=parse_statement(list);

    return node;

}

ast_node* parse_if(token_list &list){
    ast_node* node=new ast_node(ast_node_type::IF);
    list.expect("if");

    list.expect("(");

    node->condition=parse_expression(list,false);

    list.expect(")");

    node->right=parse_statement(list);

    if (list.match("else")){
        ast_node* else_node=new ast_node(ast_node_type::ELSE);
        list.expect("else");

        else_node->right=parse_statement(list);

        node->else_node=else_node;

    }

    return node;
        
}

void parse_asm_mem_operand(token_list &list, ast_node* node, bool operand1){
    list++;
    std::string &operand=operand1?node->operand1:node->operand2;
    operand='[';
    do{
        token t=list++;
        if (t.type==token_type::OPERATOR){
            if (operand.size()==1 || (t.value!="+" && t.value!="*" && t.value!="-")){
                emit_error(error::SYNTAX_ERROR, "Operator '"+t.value+"' not allowed here",t.line_number);
            }
            operand+=t.value;
            t=list++;
        }
        if (t.type!=token_type::REGISTER && t.type!=token_type::NUMBER){
            emit_error(error::SYNTAX_ERROR,"'"+token_type_to_string(t.type)+"' not allowed here",t.line_number);
        }

        operand+=t.value;

    }while (!list.end() && !list.match("]"));

    list.expect("]");

    operand+=']';

}

ast_node* parse_asm_statement(token_list &list){
    ast_node* node=new ast_node(ast_node_type::ASM);

    std::string keyword=list++.value;
    int instruction_length=get_asm_instruction_length(keyword);

    node->opcode=keyword;

    if (instruction_length==1){
        list.expect(";");
        return node;
    }

    bool is_mem=false;

    if (list.match("[")){
        is_mem=true;
        parse_asm_mem_operand(list,node,true);

    }
    else if (is_register(list.peek().value) || list.peek().type==token_type::NUMBER || is_asm_keyword(list.peek().value)){
        node->operand1=list++.value;

    }
    else{
        node->operand1_node=parse_expression(list,false);

    }

    if (instruction_length==2){
        list.expect(";");
        return node;
    }

    list.expect(",");

    if (list.match("[")){
        if (is_mem || node->operand1.empty()){
            emit_error(error::SYNTAX_ERROR,"Cannot have two memory operands",list.peek().line_number);
        }
        parse_asm_mem_operand(list,node,false);
    }
    else if (is_register(list.peek().value) || list.peek().type==token_type::NUMBER || is_asm_keyword(list.peek().value)){
        node->operand2=list++.value;
    }
    else{
        node->operand2_node=parse_expression(list,false);

    }

    list.expect(";");

    return node;

}

ast_node* parse_constructor(token_list &list){
    ast_node* node=new ast_node(ast_node_type::CONSTRUCTOR);

    list++;

    list.expect("(");

    while (!list.match(")")){
        token t=list.expect(token_type::IDENTIFIER);
        if (list.match("@")){
            list++;
            t.struct_ref=list.expect(token_type::IDENTIFIER).value;
        }
        node->params.push_back(t);
        if (list.match(")"))break;
        list.expect(",");

    }
    
    list.expect(")");

    list.expect("{");

    while (!list.match("}")){
        ast_node* statement=parse_statement(list);
        if (statement!=nullptr)node->children.push(statement);

    }

    list.expect("}");

    return node;

}

ast_node* parse_destructor(token_list &list){
    ast_node* node=new ast_node(ast_node_type::DESTRUCTOR);

    list++;
    list.expect("(");
    list.expect(")");
    list.expect("{");

    while (!list.match("}")){
        ast_node* statement=parse_statement(list);
        if (statement!=nullptr){
            node->children.push(statement);

        }


    }    

    list.expect("}");
    return node;

}

ast_node* parse_struct(token_list &list){
    list++; //advance past let
    list++; //advance past struct

    ast_node* node=new ast_node(ast_node_type::STRUCT);
    node->identifier=list.expect(token_type::IDENTIFIER).value;

    list.expect("{");

    while (!list.match("}")){
        ast_node* decl;
        if (list.match("constructor")){
            decl=parse_constructor(list);
            
        }
        else if (list.match("destructor")){
            decl=parse_destructor(list);
        }
        else{
            decl=parse_statement(list);
        }

        if (decl==nullptr)continue;

        if (decl->type!=ast_node_type::ARRAY_DECLARATION && decl->type!=ast_node_type::DECLARATION && 
            decl->type!=ast_node_type::STRUCT_DECLARATION && decl->type!=ast_node_type::CONSTRUCTOR &&
            decl->type!=ast_node_type::FUNCTION_DEFINITION && decl->type!=ast_node_type::DESTRUCTOR){
            emit_error(error::SYNTAX_ERROR,"Illegal statement within struct '"+node->identifier+"' definition");
        }
        
        if (decl->right){
            emit_error(error::SYNTAX_ERROR,"Assignments not allowed in struct definition");
        }

        node->children.push(decl);
        
        
    }
    list.expect("}");
    
    return node;

}

ast_node* parse_label(token_list &list){
    ast_node* node=new ast_node(ast_node_type::LABEL);
    node->identifier=list++.value;
    list.expect(":");

    return node;

}

ast_node* parse_goto(token_list &list){
    ast_node* node=new ast_node(ast_node_type::GOTO);
    list.expect("goto");

    node->identifier=list.expect(token_type::IDENTIFIER).value;

    list.expect(";");

    return node;

}

ast_node* parse_struct_declaration(token_list &list){
    
    ast_node *node = new ast_node(ast_node_type::STRUCT_DECLARATION);
    
    list++;

    node->value = list.expect(token_type::IDENTIFIER).value;
    
    node->identifier = list.expect(token_type::IDENTIFIER).value;
    
    if (list.match("(")){
        node->construct=true;
        list++;
        
        while (!list.match(")")){
            ast_node* arg=parse_expression(list,false);
            node->args.push(arg);
            if (list.match(")"))break;
            list.expect(",");
        }

        list.expect(")");

    }

    list.expect(";");

    return node;
}

ast_node* parse_destroy(token_list &list){
    ast_node* node=new ast_node(ast_node_type::DESTROY);
    list++;

    node->right=parse_member_access(list);

    list.expect(";");

    return node;

}

ast_node* parse_statement(token_list &list){
    if (list.match(token_type::KEYWORD)){
        if (list.match("let")){
            if (list.lookahead().value=="struct"){
                return parse_struct(list);
            }
            return parse_declaration(list);
        }
        if (list.match("func")){
            return parse_function(list);
        }
        if (list.match("return")){
            return parse_return(list);
        }
        if (list.match("while")){
            return parse_while(list);
        }
        if (list.match("break")){
            return parse_break(list);
        }
        if (list.match("continue")){
            return parse_continue(list);
        }
        if (list.match("if")){
            return parse_if(list);
        }
        if (list.match("struct")){
            return parse_struct_declaration(list);
        }
        if (list.match("goto")){
            return parse_goto(list);
        }
        if (list.match("destroy")){
            return parse_destroy(list);
        }

    }
    else if (list.match(token_type::ASM)){
        return parse_asm_statement(list);
    }
    else if (list.match("{")){
        return parse_block(list);
    }
    else{
        if (list.match(token_type::IDENTIFIER) && list.lookahead().value==":" && list.lookahead().type==token_type::DELIMITER){
            return parse_label(list);
        }
        ast_node* node=parse_expression(list,true);
        list.expect(";");
        
        return node;
    }

}

ast parse(token_list &list){
    ast tree;

    while (!list.end()){
        ast_node* node=parse_statement(list);
        
        if (node!=nullptr)tree.push(node);
    }
    
    return tree;
    
}

void build_symbol_table(ast_node* node, symbol_table* table){
    if (node==nullptr)return;

    if (node->type==ast_node_type::DECLARATION){
        symbol* s=new symbol(node->identifier);
        s->size=8;

        if (!node->struct_ref.empty()){
            s->struct_ref=node->struct_ref;
        }
        
        if (table->type!=symbol_table_type::GLOBAL){
            s->type=symbol_type::LOCAL;

            if (table->type==symbol_table_type::STRUCT){
                s->offset=table->cur_offset;
                table->cur_offset+=s->size;

            }
            else{
                table->cur_offset-=s->size;
                s->offset=table->cur_offset;

            }
            

        }
        else{
            s->type=symbol_type::GLOBAL;
        }
        
        table->push(s);
        
        
    }
    else if (node->type==ast_node_type::ARRAY_DECLARATION){
        symbol* s=new symbol(node->identifier);
        s->is_array=true;

        if (table->type==symbol_table_type::GLOBAL){
            s->type=symbol_type::GLOBAL;
        }
        else{
            s->type=symbol_type::LOCAL;
        }

        if (node->element_size_reference->type==ast_node_type::SIZEOF){
            symbol_table* strct=table->lookup_struct(node->element_size_reference->identifier);
            if (strct==nullptr){
                emit_error(error::SEMANTIC_ERROR, "No such struct '"+node->element_size_reference->identifier+"'");
            }
            s->element_size=strct->total_size;
            
        }
        else{
            s->element_size=std::atoi((node->element_size_reference->value).c_str());
        }

        s->size=s->element_size;

        for (int i=0;i<node->indexes.size();i++){
            int sz=std::atoi((node->indexes.get(i)->value).c_str());
            s->size*=sz;
            s->array_sizes.push_back(sz);

        }

        if (table->type!=symbol_table_type::GLOBAL){
            if (table->type==symbol_table_type::STRUCT){
                s->offset=table->cur_offset;
                table->cur_offset+=s->size;
            }
            else{
                table->cur_offset-=s->size;
                s->offset=table->cur_offset;
            }
        }

        table->push(s);

    }
    else if (node->type==ast_node_type::LABEL){
        if (table->type==symbol_table_type::GLOBAL){
            emit_error(error::SEMANTIC_ERROR,"'"+node->identifier+"' label is global");
        }
        symbol* s=new symbol(symbol_type::LABEL);
        s->name=node->identifier;

        table->push_label(s);

    }
    else if (node->type==ast_node_type::STRUCT){
        symbol_table* struct_table=new symbol_table(symbol_table_type::STRUCT,table,0);
        struct_table->name=node->identifier;
        
        for (int i=0;i<node->children.size();i++){
            build_symbol_table(node->children.get(i),struct_table);
        }

        table->push_struct(struct_table);

    }
    else if (node->type==ast_node_type::DESTRUCTOR){
        if (table->type!=symbol_table_type::STRUCT){
            emit_error(error::SEMANTIC_ERROR,"Destructor outside struct");
        }
        if (table->destructor!=nullptr){
            emit_error(error::SEMANTIC_ERROR,"Cannot have more than one destructor per struct");
        }

        symbol* s=new symbol(symbol_type::DESTRUCTOR);
        s->name=table->name;
        table->destructor=s;

        symbol_table* global=table->push_destructor(s);
        symbol_table* destructor_table=new symbol_table(symbol_table_type::DESTRUCTOR,global,0);

        destructor_table->parent_struct=table;
        s->destructor_node=node;
        node->table=destructor_table;
        s->destructor_table=destructor_table;

        for (int i=0;i<node->children.size();i++){
            build_symbol_table(node->children.get(i),destructor_table);

        }

    }
    else if (node->type==ast_node_type::CONSTRUCTOR){
        if (table->type!=symbol_table_type::STRUCT){
            emit_error(error::SEMANTIC_ERROR,"Constructor outside struct");
        }

        symbol* s=new symbol(symbol_type::CONSTRUCTOR);
        s->name=table->name;

        symbol_table* global=table->push_constructor(s);
        symbol_table* constructor_table=new symbol_table(symbol_table_type::CONSTRUCTOR,global,0);

        constructor_table->parent_struct=table;
        s->constructor_node=node;
        node->table=constructor_table;
        s->constructor_table=constructor_table;
        
        for (int i=0;i<node->children.size();i++){
            build_symbol_table(node->children.get(i),constructor_table);

        }
        
        node->table->push_params(node->params);

    }
    else if (node->type==ast_node_type::STRUCT_DECLARATION){
        symbol* s=new symbol(node->identifier);
        s->is_struct=true;
        s->parent_struct=table->lookup_struct(node->value);
        if (s->parent_struct==nullptr){
            emit_error(error::SEMANTIC_ERROR,"Unknown struct: '"+node->value+"'");
        }

        s->size=s->parent_struct->total_size;

        if (table->type==symbol_table_type::GLOBAL){
            s->type=symbol_type::GLOBAL;
            
        }
        else{
            s->type=symbol_type::LOCAL;

            if (table->type==symbol_table_type::STRUCT){
                s->offset=table->cur_offset;
                table->cur_offset+=s->size;

            }
            else{
                table->cur_offset-=s->size;
                s->offset=table->cur_offset;

            }

        }

        table->push(s);


    }
    else if (node->type==ast_node_type::FUNCTION_DEFINITION){
        if (table->type==symbol_table_type::STRUCT){
            symbol* s=new symbol(node->identifier);
            s->type=symbol_type::METHOD;
            s->memberof_struct=s->parent_struct=table;

            symbol_table* global=table->push_method(s);
            node->table=new symbol_table(symbol_table_type::METHOD,global,0);
            s->method_table=node->table;
            s->method_table->parent_struct=table;

            s->method_node=node;
            
            for (int i=0;i<node->children.size();i++){
                build_symbol_table(node->children.get(i),s->method_table);
            }

            s->method_table->push_params(node->params);

            return;

        }

        symbol* s=new symbol(node->identifier);
        s->type=symbol_type::FUNCTION;
        table->push(s);
    
        node->table=new symbol_table(symbol_table_type::FUNCTION,table,0);
        for (int i=0;i<node->children.size();i++)
            build_symbol_table(node->children.get(i),node->table);
        
        node->table->push_params(node->params);

    }
    else if (node->type==ast_node_type::BLOCK){
        if (table->type==symbol_table_type::GLOBAL){
            emit_error(error::SEMANTIC_ERROR,"Unexpected block statement");

        }
        node->table=new symbol_table(symbol_table_type::BLOCK,table,table->cur_offset);
        for (int i=0;i<node->children.size();i++)
            build_symbol_table(node->children.get(i),node->table);

    }
    else if (node->type==ast_node_type::LOOP){
        build_symbol_table(node->right,table);

    }
    else if (node->type==ast_node_type::IF){
        build_symbol_table(node->right,table);
        if (node->else_node)
            build_symbol_table(node->else_node->right,table);
        
    }
    

}
#ifndef EXIT
#define EXIT std::exit(0);
#endif

void generate_expression(asm_program &program, asm_function* function, symbol_table* table, ast_node* node, bool get_address);

symbol* generate_member_access(asm_program &program, asm_function* function, symbol_table* table, ast_node* node){
    std::string struct_ref;

    if (node->left->type==ast_node_type::THIS){
        function->push(instruction("LEA","RAX","[RBP+16]"));
        symbol_table* tmp=table->lookup_parent_struct();
        if (tmp==nullptr){
            emit_error(error::CODE_GENERATION_ERROR,"'this' keyword outside method/constructor");
        }
        struct_ref=tmp->name;

    }
    else{
        symbol *s = table->lookup(node->left->identifier);
        if (s==nullptr){
            emit_error(error::CODE_GENERATION_ERROR,"Unknown symbol '"+node->left->identifier+"'");
        }
        if (s->parent_struct!=nullptr){
            struct_ref=s->parent_struct->name;
        }
        else {
            struct_ref=s->struct_ref;
        }
        function->push(instruction("LEA", "RAX", "[" + s->get_address() + "]"));

        if (s->type == symbol_type::GLOBAL && s->is_struct){
            function->push(instruction("MOV", "RAX", "[RAX]"));
        }
    }
    
    ast_node* cur=node;
    
    if (cur->value=="->"){
        function->push(instruction("MOV","RAX","[RAX]"));
    }

    cur=cur->right;

    while (cur->type==ast_node_type::MEMBER_ACCESS){
        symbol* field;
        if (!struct_ref.empty()){
            field=table->lookup_field(cur->left->identifier,struct_ref);
        }
        else{
            field=table->lookup_field(cur->left->identifier,cur->left->struct_ref);
        }
        
        if (field==nullptr){
            emit_error(error::CODE_GENERATION_ERROR,"Unknown struct field: '"+cur->left->identifier+"'");
        }

        if (field->parent_struct != nullptr){
            struct_ref = field->parent_struct->name;
        }
        else{
            struct_ref = field->struct_ref;
        }

        function->push(instruction("ADD","RAX",std::to_string(field->offset)));

        if (cur->value=="->"){
            function->push(instruction("MOV","RAX","[RAX]"));
        }

        cur=cur->right;

    }

    if (cur->type==ast_node_type::FUNCTION_CALL){
        symbol* s;
        if (!struct_ref.empty()){
            s=table->lookup_method(cur->identifier,struct_ref,cur->args.size());
        }
        else{
            s=table->lookup_method(cur->identifier,cur->struct_ref,cur->args.size());
        }

        if (s==nullptr){
            emit_error(error::CODE_GENERATION_ERROR,"Unknown method '"+cur->identifier+"'");

        }

        for (int i=cur->args.size()-1;i>=0;i--){
            function->push(instruction("PUSH","RAX"));
            generate_expression(program,function,table,cur->args.get(i),false);
            function->push(instruction("MOV","RBX","RAX"));
            function->push(instruction("POP","RAX"));
            function->push(instruction("PUSH","RBX"));

        }

        function->push(instruction("PUSH","RAX"));
        function->push(instruction("CALL",s->get_address()));

        function->push(instruction("ADD","RSP",std::to_string(8+cur->args.size()*8)));

        return s;

    }
    symbol* field;
    if (!struct_ref.empty()){
        field=table->lookup_field(cur->identifier,struct_ref);
    }
    else{
        field=table->lookup_field(cur->identifier,cur->struct_ref);
    }

    if (field==nullptr){
        emit_error(error::CODE_GENERATION_ERROR, "Unknown struct field: '" + cur->identifier + "'");
    }

    function->push(instruction("ADD","RAX",std::to_string(field->offset)));
    
    return field;

}

void generate_expression(asm_program &program, asm_function* function, symbol_table* table, ast_node* node, bool get_address){
    if (node==nullptr)return;

    if (node->type==ast_node_type::NUMBER){
        function->push(instruction("MOV","RAX",node->value));
    }
    else if (node->type==ast_node_type::STRING){
        std::string str=program.push_string(node->value);
        function->push(instruction("MOV","RAX",str));
    }
    else if (is_binary_operator(node->value) && node->type==ast_node_type::EXPRESSION){
        generate_expression(program,function,table,node->left, false);
        function->push(instruction("PUSH","RAX"));
        function->push(instruction("PUSH","R8"));
        generate_expression(program,function,table,node->right, false);
        function->push(instruction("POP","R8"));
        function->push(instruction("MOV","RBX","RAX"));
        function->push(instruction("POP","RAX"));

        
        if (node->value=="+"){
            function->push(instruction("ADD","RAX","RBX"));

        }
        else if (node->value=="-"){
            function->push(instruction("SUB","RAX","RBX"));

        }
        else if (node->value=="*"){
            function->push(instruction("IMUL","RAX","RBX"));
        }
        else if (node->value=="/" || node->value=="%"){
            function->push(instruction("CQO"));
            function->push(instruction("IDIV","RBX"));
            
            if (node->value=="%"){
                function->push(instruction("MOV","RAX","RDX"));
            }

        }
        else if (node->value==">>"){
            function->push(instruction("MOV","RCX","RBX"));
            function->push(instruction("SHR","RAX","CL"));
        }
        else if (node->value=="<<"){
            function->push(instruction("MOV","RCX","RBX"));
            function->push(instruction("SHL","RAX","CL"));
        }
        else if (node->value=="=="){
            function->push(instruction("CMP","RAX","RBX"));
            function->push(instruction("SETE","AL"));
            function->push(instruction("MOVZX","RAX","AL"));
        }
        else if (node->value=="!="){
            function->push(instruction("CMP","RAX","RBX"));
            function->push(instruction("SETNE","AL"));
            function->push(instruction("MOVZX","RAX","AL"));
        }
        else if (node->value=="<"){
            function->push(instruction("CMP","RAX","RBX"));
            function->push(instruction("SETL","AL"));
            function->push(instruction("MOVZX","RAX","AL"));
        }
        else if (node->value=="<="){
            function->push(instruction("CMP","RAX","RBX"));
            function->push(instruction("SETLE","AL"));
            function->push(instruction("MOVZX","RAX","AL"));
        }
        else if (node->value==">"){
            function->push(instruction("CMP","RAX","RBX"));
            function->push(instruction("SETG","AL"));
            function->push(instruction("MOVZX","RAX","AL"));
        }
        else if (node->value==">="){
            function->push(instruction("CMP","RAX","RBX"));
            function->push(instruction("SETGE","AL"));
            function->push(instruction("MOVZX","RAX","AL"));
        }
        else if (node->value=="^"){
            function->push(instruction("XOR","RAX","RBX"));
        }
        else if (node->value=="&"){
            function->push(instruction("AND","RAX","RBX"));
        }
        else if (node->value=="|"){
            function->push(instruction("OR","RAX","RBX"));
        }


    }
    else if (node->value==","){
        generate_expression(program,function,table,node->left,false);
        generate_expression(program,function,table,node->right,false);

    }
    else if (node->value=="=>"){
        if (node->right->type!=ast_node_type::FUNCTION_CALL){
            emit_error(error::CODE_GENERATION_ERROR,"Expected constructor call after '=>'");
        }

        symbol* id=nullptr;

        if (node->left->type==ast_node_type::MEMBER_ACCESS){
            id=generate_member_access(program,function,table,node->left);

        }
        else if (node->left->type==ast_node_type::IDENTIFIER){
            id=table->lookup(node->left->identifier);
            if (id==nullptr){
                emit_error(error::CODE_GENERATION_ERROR,"Unknown symbol '"+node->left->identifier+"'");
            }
            generate_expression(program,function,table,node->left,true);

        }

        if (id!=nullptr){
            if (!id->struct_ref.empty()){
                function->push(instruction("MOV","RAX","[RAX]"));

            }
            else if (!id->is_struct){
                emit_error(error::CODE_GENERATION_ERROR, "'"+id->name+"' does not belong to or reference a struct");

            }
        }
        else{
            generate_expression(program,function,table,node->left,false);

        }

        symbol* s=table->lookup_constructor(node->right->identifier,node->right->args.size());

        if (s==nullptr){
            emit_error(error::CODE_GENERATION_ERROR,"No constructor for struct '"+
                node->right->identifier+"' with arg count "+
                std::to_string(node->right->args.size()));
        }

        for (int i=node->right->args.size()-1;i>=0;i--){
            ast_node* arg=node->right->args.get(i);

            function->push(instruction("PUSH","RAX"));

            generate_expression(program,function,table,arg,false);

            function->push(instruction("MOV","RBX","RAX"));
            function->push(instruction("POP","RAX"));
            function->push(instruction("PUSH","RBX"));

        }

        function->push(instruction("PUSH","RAX"));

        function->push(instruction("CALL",s->get_address()));

    }
    else if (node->type==ast_node_type::REGISTER){
        if (node->value!="RAX"){
            if (!is_reg64(node->value)){
                function->push(instruction("MOVZX","RAX",node->value));
            }
            else{
                function->push(instruction("MOV","RAX",node->value));
            }
            
        }
        
    }
    else if (node->type==ast_node_type::UNARY){

        if (node->value=="*"){
            generate_expression(program,function,table,node->right,false);

            if (!get_address){
                function->push(instruction("MOV","RAX","[RAX]"));
            }
            else{
                function->push(instruction("MOV","R8","8"));
            }

        }
        else if (node->value=="+"){
            generate_expression(program,function,table,node->right,false);
        }
        else if (node->value=="!"){
            generate_expression(program,function,table,node->right,false);
            function->push(instruction("CMP","RAX","0"));
            function->push(instruction("SETE","AL"));
            function->push(instruction("MOVZX","RAX","AL"));
        }
        else if (node->value=="~"){
            generate_expression(program,function,table,node->right,false);
            function->push(instruction("NOT","RAX"));
        }
        else if (node->value=="-"){
            generate_expression(program,function,table,node->right,false);
            function->push(instruction("NEG","RAX"));
        }
        else if (node->value=="@"){
            generate_expression(program,function,table,node->right,true);
            function->push(instruction("MOV","R8","8"));

        }
        else if(node->value=="++" || node->value=="--"){
            std::string opc=node->value=="++"?"ADD":"SUB";

            generate_expression(program,function,table,node->right,true);

            std::string _2=function->create_local_label();
            std::string _4=function->create_local_label();
            std::string _8=function->create_local_label();
            std::string donel=function->create_local_label();

            function->push(instruction("MOV","RBX","1"));

            function->push(instruction("CMP","R8","2"));
            function->push(instruction("JGE",_2));
            function->push(instruction(opc,"[RAX]","BL"));
            function->push(instruction("JMP",donel));
            function->push_local_label(_2);
            function->push(instruction("CMP","R8","4"));
            function->push(instruction("JGE",_4));
            function->push(instruction(opc,"[RAX]","BX"));
            function->push(instruction("JMP",donel));
            function->push_local_label(_4);
            function->push(instruction("CMP","R8","8"));
            function->push(instruction("JGE",_8));
            function->push(instruction(opc,"[RAX]","EBX"));
            function->push(instruction("JMP",donel));
            function->push_local_label(_8);
            function->push(instruction(opc,"[RAX]","RBX"));
            function->push_local_label(donel);
            function->push(instruction("MOV","R8","8"));

            function->push(instruction("MOV","RAX","[RAX]"));

        }

    }
    else if (node->value=="&&"){
        std::string donel=function->create_local_label();

        function->push(instruction("MOV","RBX","0"));
        function->push(instruction("PUSH","RBX"));

        generate_expression(program,function,table,node->left,false);
        function->push(instruction("CMP","RAX","0"));
        function->push(instruction("JE",donel));
        generate_expression(program,function,table,node->right,false);
        function->push(instruction("CMP","RAX","0"));
        function->push(instruction("JE",donel));
        function->push(instruction("POP","RBX"));
        function->push(instruction("MOV","RBX","1"));
        function->push(instruction("PUSH","RBX"));

        function->push_local_label(donel);
        function->push(instruction("POP","RBX"));
        function->push(instruction("MOV","RAX","RBX"));

    }
    else if (node->value=="||"){
        std::string donel=function->create_local_label();

        function->push(instruction("MOV","RBX","1"));
        function->push(instruction("PUSH","RBX"));

        generate_expression(program,function,table,node->left,false);
        function->push(instruction("CMP","RAX","0"));
        function->push(instruction("JNE",donel));

        generate_expression(program,function,table,node->right,false);
        function->push(instruction("CMP","RAX","0"));
        function->push(instruction("JNE",donel));
        function->push(instruction("POP","RBX"));
        function->push(instruction("MOV","RBX","0"));
        function->push(instruction("PUSH","RBX"));

        function->push_local_label(donel);
        function->push(instruction("POP","RBX"));
        function->push(instruction("MOV","RAX","RBX"));

    }
    else if (node->value=="++" || node->value=="--"){
        std::string opc=node->value=="++"?"ADD":"SUB";

        generate_expression(program,function,table,node->left,false);

        function->push(instruction("PUSH","RAX"));

        generate_expression(program,function,table,node->left,true);

        std::string _2=function->create_local_label();
        std::string _4=function->create_local_label();
        std::string _8=function->create_local_label();
        std::string donel=function->create_local_label();

        function->push(instruction("MOV","RBX","1"));

        function->push(instruction("CMP","R8","2"));
        function->push(instruction("JGE",_2));
        function->push(instruction(opc,"[RAX]","BL"));
        function->push(instruction("JMP",donel));
        function->push_local_label(_2);
        function->push(instruction("CMP","R8","4"));
        function->push(instruction("JGE",_4));
        function->push(instruction(opc,"[RAX]","BX"));
        function->push(instruction("JMP",donel));
        function->push_local_label(_4);
        function->push(instruction("CMP","R8","8"));
        function->push(instruction("JGE",_8));
        function->push(instruction(opc,"[RAX]","EBX"));
        function->push(instruction("JMP",donel));
        function->push_local_label(_8);
        function->push(instruction(opc,"[RAX]","RBX"));
        function->push_local_label(donel);
        function->push(instruction("MOV","R8","8"));

        function->push(instruction("POP","RAX"));

    }
    else if (node->type==ast_node_type::PTR_ACCESS){
        generate_expression(program,function,table,node->left,false);
        function->push(instruction("PUSH","RAX"));

        generate_expression(program,function,table,node->element_size_reference,false);

        function->push(instruction("MOV","R8","RAX"));

        function->push(instruction("POP","RAX")); //RAX contains address

        for (int i=0;i<node->indexes.size();i++){
            function->push(instruction("PUSH","RAX"));
            function->push(instruction("PUSH","R8"));

            generate_expression(program,function,table,node->indexes.get(i),false);

            function->push(instruction("POP","R8"));

            function->push(instruction("MOV","RBX","RAX"));
            function->push(instruction("POP","RAX"));

            if (i<node->indexes.size()-1){
                function->push(instruction("MOV","RAX","[RAX+RBX*8]"));

            }
            else{
                function->push(instruction("IMUL","RBX","R8"));
                function->push(instruction("LEA","RAX","[RAX+RBX]"));

            }

        }

        if (!get_address){
            std::string _2=function->create_local_label();
            std::string _4=function->create_local_label();
            std::string _8=function->create_local_label();
            std::string donel=function->create_local_label();

            function->push(instruction("CMP","R8","2"));
            function->push(instruction("JGE",_2));
            function->push(instruction("MOV","AL","[RAX]"));
            function->push(instruction("MOVZX","RAX","AL"));
            function->push(instruction("JMP",donel));
            function->push_local_label(_2);
            function->push(instruction("CMP","R8","4"));
            function->push(instruction("JGE",_4));
            function->push(instruction("MOV","AX","[RAX]"));
            function->push(instruction("MOVZX","RAX","AX"));
            function->push(instruction("JMP",donel));
            function->push_local_label(_4);
            function->push(instruction("CMP","R8","8"));
            function->push(instruction("JGE",_8));
            function->push(instruction("MOV","EAX","[RAX]"));
            function->push(instruction("MOVZX","RAX","EAX"));
            function->push(instruction("JMP",donel));
            function->push_local_label(_8);
            function->push(instruction("MOV","RAX","[RAX]"));
            function->push_local_label(donel);
            function->push(instruction("MOV","R8","8"));

        }
            
    }
    else if (node->type==ast_node_type::ARRAY_ACCESS){
        symbol* s;

        if (node->left->type==ast_node_type::IDENTIFIER){
            s=table->lookup(node->left->identifier);
            if (s==nullptr){
                emit_error(error::CODE_GENERATION_ERROR,"Unknown symbol '"+node->left->identifier+"'");
            }
            if (!s->is_array){
                emit_error(error::CODE_GENERATION_ERROR,s->name+" is not an array");
            }

            if (s->type==symbol_type::GLOBAL){
                function->push(instruction("MOV","RAX","["+s->get_address()+"]"));

            }
            else{
                function->push(instruction("LEA","RAX","["+s->get_address()+"]"));

            }

        }
        else{
            if (node->left->type!=ast_node_type::MEMBER_ACCESS){
                emit_error(error::CODE_GENERATION_ERROR,"Expected array but got '"+node_type_to_string(node->left->type)+"'");
            }

            s=generate_member_access(program,function,table,node->left);

            if (!s->is_array){
                emit_error(error::CODE_GENERATION_ERROR,s->name+" is not an array");
            }

        }

        function->push(instruction("MOV","R8",std::to_string(s->element_size)));
        function->push(instruction("XOR","RCX","RCX"));

        for (int i=0;i<node->indexes.size();i++){
            ast_node* index=node->indexes.get(i);

            function->push(instruction("PUSH","RAX"));
            function->push(instruction("PUSH","RCX"));
            function->push(instruction("PUSH","R8"));

            generate_expression(program,function,table,index,false);

            function->push(instruction("POP","R8"));
            function->push(instruction("POP","RCX"));
            function->push(instruction("MOV","RBX","RAX"));
            function->push(instruction("POP","RAX"));

            for (int j=i+1;j<s->array_sizes.size();j++){
                int arr_size=s->array_sizes.at(j);

                function->push(instruction("IMUL","RBX",std::to_string(arr_size)));

            }

            function->push(instruction("IMUL","RBX","R8"));
            function->push(instruction("ADD","RCX","RBX"));

        }

        function->push(instruction("ADD","RAX","RCX"));

        std::string reg;
        if (s->element_size<2)reg="AL";
        else if (s->element_size<4)reg="AX";
        else if (s->element_size<8)reg="EAX";
        else reg="RAX";

        if (!get_address){
            function->push(instruction("MOV",reg,"[RAX]"));
            if (s->element_size<8){
                function->push(instruction("MOVZX","RAX",reg));
            }
            function->push(instruction("MOV","R8","8"));

        }

    }
    else if (node->type==ast_node_type::MEMBER_ACCESS){
        symbol* s=generate_member_access(program,function,table,node);

        if (s->type!=symbol_type::METHOD){
            if (!get_address){
                function->push(instruction("MOV","RAX","[RAX]"));

            }
            else{
                function->push(instruction("MOV","R8","8"));

            }
        }
        

    }
    else if (node->type==ast_node_type::SIZEOF){
        symbol* s=table->lookup(node->identifier);
        if (s!=nullptr){
            function->push(instruction("MOV","RAX",std::to_string(s->size)));
            return;
        }
        symbol_table* strct=table->lookup_struct(node->identifier);
        if (strct==nullptr){
            emit_error(error::CODE_GENERATION_ERROR,"Unknown symbol: '"+node->identifier+"'");
        }

        function->push(instruction("MOV","RAX",std::to_string(strct->total_size)));
        
    }
    else if (node->value=="=" || node->value=="+=" || node->value=="-=" || 
        node->value=="^=" || node->value=="&=" || node->value=="|="){
            std::string opc;
            if (node->value=="=")opc="MOV";
            else if (node->value=="+=")opc="ADD";
            else if (node->value=="-=")opc="SUB";
            else if (node->value=="^=")opc="XOR";
            else if (node->value=="|=")opc="OR";
            else if (node->value=="&=")opc="AND";

            generate_expression(program,function,table,node->left,true);

            function->push(instruction("PUSH","RAX"));
            function->push(instruction("PUSH","R8"));

            generate_expression(program,function,table,node->right,false);

            function->push(instruction("POP","R8"));
            function->push(instruction("MOV","RBX","RAX"));
            function->push(instruction("POP","RAX"));

            std::string _2=function->create_local_label();
            std::string _4=function->create_local_label();
            std::string _8=function->create_local_label();
            std::string donel=function->create_local_label();

            function->push(instruction("CMP","R8","2"));
            function->push(instruction("JGE",_2));
            function->push(instruction(opc,"[RAX]","BL"));
            function->push(instruction("JMP",donel));
            function->push_local_label(_2);
            function->push(instruction("CMP","R8","4"));
            function->push(instruction("JGE",_4));
            function->push(instruction(opc,"[RAX]","BX"));
            function->push(instruction("JMP",donel));
            function->push_local_label(_4);
            function->push(instruction("CMP","R8","8"));
            function->push(instruction("JGE",_8));
            function->push(instruction(opc,"[RAX]","EBX"));
            function->push(instruction("JMP",donel));
            function->push_local_label(_8);
            function->push(instruction(opc,"[RAX]","RBX"));
            function->push_local_label(donel);
            function->push(instruction("MOV","R8","8"));

            function->push(instruction("MOV","RAX","RBX"));
            
        }
    else if (node->value=="/=" || node->value=="%="){
        generate_expression(program,function,table,node->left,false);

        function->push(instruction("PUSH","RAX"));
        function->push(instruction("PUSH","R8"));

        generate_expression(program,function,table,node->right,false);

        function->push(instruction("POP","R8"));
        function->push(instruction("MOV","RBX","RAX"));

        function->push(instruction("POP","RAX"));

        function->push(instruction("CQO"));

        function->push(instruction("IDIV","RBX"));

        function->push(instruction("PUSH","RBX"));

        if (node->value=="%="){
            function->push(instruction("MOV","RAX","RDX"));
        }

        function->push(instruction("MOV","RBX","RAX"));

        generate_expression(program,function,table,node->left,true);

        std::string _2=function->create_local_label();
        std::string _4=function->create_local_label();
        std::string _8=function->create_local_label();
        std::string donel=function->create_local_label();

        function->push(instruction("CMP","R8","2"));
        function->push(instruction("JGE",_2));
        function->push(instruction("MOV","[RAX]","BL"));
        function->push(instruction("JMP",donel));
        function->push_local_label(_2);
        function->push(instruction("CMP","R8","4"));
        function->push(instruction("JGE",_4));
        function->push(instruction("MOV","[RAX]","BX"));
        function->push(instruction("JMP",donel));
        function->push_local_label(_4);
        function->push(instruction("CMP","R8","8"));
        function->push(instruction("JGE",_8));
        function->push(instruction("MOV","[RAX]","EBX"));
        function->push(instruction("JMP",donel));
        function->push_local_label(_8);
        function->push(instruction("MOV","[RAX]","RBX"));
        function->push_local_label(donel);
        function->push(instruction("MOV","R8","8"));

        function->push(instruction("POP","RAX"));

    }
    else if (node->value==">>=" || node->value=="<<=" || node->value=="*="){

        generate_expression(program,function,table,node->left,false);

        function->push(instruction("PUSH","RAX"));

        generate_expression(program,function,table,node->right,false);

        function->push(instruction("MOV","RBX","RAX"));
        function->push(instruction("POP","RAX"));

        if (node->value=="*="){
            function->push(instruction("IMUL","RAX","RBX"));
        }
        else if (node->value==">>="){
            function->push(instruction("MOV","RCX","RBX"));
            function->push(instruction("SHR","RAX","CL"));
        }
        else{
            function->push(instruction("MOV","RCX","RBX"));
            function->push(instruction("SHL","RAX","CL"));

        }

        function->push(instruction("PUSH","RBX"));
        function->push(instruction("MOV","RBX","RAX"));

        generate_expression(program,function,table,node->left,true);

        std::string _2=function->create_local_label();
        std::string _4=function->create_local_label();
        std::string _8=function->create_local_label();
        std::string donel=function->create_local_label();

        function->push(instruction("CMP","R8","2"));
        function->push(instruction("JGE",_2));
        function->push(instruction("MOV","[RAX]","BL"));
        function->push(instruction("JMP",donel));
        function->push_local_label(_2);
        function->push(instruction("CMP","R8","4"));
        function->push(instruction("JGE",_4));
        function->push(instruction("MOV","[RAX]","BX"));
        function->push(instruction("JMP",donel));
        function->push_local_label(_4);
        function->push(instruction("CMP","R8","8"));
        function->push(instruction("JGE",_8));
        function->push(instruction("MOV","[RAX]","EBX"));
        function->push(instruction("JMP",donel));
        function->push_local_label(_8);
        function->push(instruction("MOV","[RAX]","RBX"));
        function->push_local_label(donel);
        function->push(instruction("MOV","R8","8"));

        function->push(instruction("POP","RAX"));

    }
    else if (node->type==ast_node_type::FUNCTION_CALL){
        symbol* s=table->lookup(node->identifier);
        if (s==nullptr){
            emit_error(error::CODE_GENERATION_ERROR,"Unknown function '"+node->identifier+"'");
        }
        for (int i=node->args.size()-1;i>=0;i--){
            ast_node* arg=node->args.get(i);

            generate_expression(program, function, table, arg, false);

            function->push(instruction("PUSH","RAX"));
        }

        function->push(instruction("LEA","RAX",sformat("[%s]",s->get_address().c_str())));

        function->push(instruction("CALL","RAX"));

        if (node->args.size()){
            function->push(instruction("ADD","RSP",std::to_string(node->args.size()*8)));
        }

    }
    else if (node->value=="?"){
        std::string elif=function->create_local_label();
        std::string donel=function->create_local_label();

        generate_expression(program,function,table,node->condition,false);

        function->push(instruction("CMP","RAX","0"));
        function->push(instruction("JE",elif));
        generate_expression(program,function,table,node->true_node,false);
        function->push(instruction("JMP",donel));
        function->push_local_label(elif);
        generate_expression(program,function,table,node->false_node,false);
        function->push_local_label(donel);

    }
    else if (node->type==ast_node_type::THIS){
        function->push(instruction("MOV","RAX","[RBP+16]"));

    }
    else if (node->type==ast_node_type::IDENTIFIER){
        symbol* s=table->lookup(node->identifier);
        if (s==nullptr){
            emit_error(error::CODE_GENERATION_ERROR,sformat("Unknown symbol '%s'",node->identifier.c_str()));
        }

        std::string address=s->get_address();

        function->push(instruction("LEA","RAX","["+address+"]"));

        if (s->type==symbol_type::GLOBAL && (s->is_array || s->is_struct)){
            function->push(instruction("MOV","RAX","[RAX]"));

        }

        if (!get_address){
            function->push(instruction("MOV","RAX","[RAX]"));
        }
        else{
            function->push(instruction("MOV","R8","8"));
        }

    }

}

void generate_array(asm_program &program, asm_function* function, ast &indexes, int index, int esr){
    int cur_size=std::stoi(indexes.get(index)->value);

    bool last=index==indexes.size()-1;

    data_size dsize;
    if (last){
        if (esr<2)dsize=data_size::BYTE;
        else if (esr<4)dsize=data_size::WORD;
        else if (esr<8)dsize=data_size::DWORD;
        else if (esr==8)dsize=data_size::QWORD;
        else {
            cur_size=1+((cur_size*esr)/8);
            dsize=data_size::QWORD;
        }
    }
    else{
        dsize=data_size::QWORD;
    }

    std::string array=program.push_array(dsize,cur_size);

    for (int i=0;i<cur_size && !last;i++){
        generate_array(program,function,indexes,index+1,esr);
        function->push(instruction("MOV",sformat("[%s+%d*8]",array.c_str(),i),"RAX"));

    }

    function->push(instruction("MOV","RAX",array));

}

void generate_fill_array(asm_program &program, asm_function* function, symbol_table* table, symbol* s, ast_node* node, int i){
    std::string reg;
    if (s->element_size<2)reg="BL";
    else if (s->element_size<4)reg="BX";
    else if (s->element_size<8)reg="EBX";
    else reg="RBX";

    for (int j=0;j<node->elements.size();j++){
        ast_node* element=node->elements.get(j);

        if (element->type==ast_node_type::ARRAY){
            function->push(instruction("PUSH","RAX"));
            generate_fill_array(program,function,table,s,element,i+1);
            function->push(instruction("POP","RAX"));
        }
        else{
            function->push(instruction("PUSH","RAX"));

            generate_expression(program,function,table,element,false);
            
            function->push(instruction("MOV","RBX","RAX"));
            function->push(instruction("POP","RAX"));
            function->push(instruction("MOV","[RAX]",reg));

        }
        function->push(instruction("MOV","RBX",std::to_string(s->element_size)));
        
        for (int k=i+1;k<s->array_sizes.size();k++){
            function->push(instruction("IMUL","RBX",std::to_string(s->array_sizes.at(k))));
        }
        
        function->push(instruction("ADD","RAX","RBX"));
    }

}

void generate_array_declaration(asm_program &program, asm_function* function, symbol_table* table, ast_node* node){
    symbol* s=table->lookup(node->identifier);

    if (s->type==symbol_type::GLOBAL){
        program.push_global(s->name);

        std::string array=program.push_array(data_size::BYTE,s->size);

        function->push(instruction("MOV","RAX",array));
        function->push(instruction("MOV","["+s->get_address()+"]","RAX"));

    }
    else{
        function->push(instruction("LEA","RAX","["+s->get_address()+"]"));

    }

    if (node->right){
        if (node->right->children.size()==1 && node->right->children.get(0)->type==ast_node_type::NUMBER){
            std::string val=node->right->children.get(0)->value;
            int esr=s->element_size;
            std::string szb=esr==1?"B":esr==2?"W":esr==4?"D":esr==8?"Q":"B";
            bool unusual=esr!=1 && esr!=2 && esr!=4 && esr!=8;

            function->push(instruction("MOV","RDI","RAX"));
            function->push(instruction("MOV","RAX",val));
            function->push(instruction("MOV","RCX",std::to_string(unusual?s->size:s->size/esr)));
            function->push(instruction("REP","STOS"+szb));
            
        }
        else{
            generate_fill_array(program,function,table,s,node->right,0);
        }
    }

}

void generate_declaration(asm_program &program, asm_function* function, symbol_table* table, ast_node* node){
    symbol* s=table->lookup(node->identifier);
    
    if (s->type==symbol_type::GLOBAL){
        program.push_global(s->name);
    }

    generate_expression(program,function,table,node->right,false);

    function->push(instruction("MOV",sformat("[%s]",s->get_address().c_str()),"RAX"));

}

void generate_destruction(asm_function* function, symbol_table* table){
    for (int i=0;i<table->to_destroy.size();i++){
        symbol* s=table->to_destroy.at(i);
        
        symbol_table* parent_struct=table->to_destroy.at(i)->parent_struct;

        symbol* destructor=parent_struct->destructor;

        if (destructor==nullptr)continue;
        
        function->push(instruction("LEA","RAX","["+s->get_address()+"]"));
        function->push(instruction("PUSH","RAX"));
        function->push(instruction("CALL",destructor->get_address()));
        function->push(instruction("ADD","RSP","8"));

        
    }

}

void generate_statement(asm_program &program, asm_function* function, symbol_table* table, ast_node* node);

void generate_block(asm_program &program, asm_function* function, ast_node* node){
    for (int i=0;i<node->children.size();i++){
        ast_node* statement=node->children.get(i);
        generate_statement(program,function,node->table,statement);
    }
    generate_destruction(function,node->table);

}

void generate_epilogue(asm_function* function){
    function->push(instruction("MOV","RSP","RBP"));
    function->push(instruction("POP","RBP"));
    function->push(instruction("RET"));

}

void generate_return(asm_program &program, asm_function* function, symbol_table* table, ast_node* node){
    symbol_table* tmp=table;
    while (tmp->type!=symbol_table_type::GLOBAL){
        generate_destruction(function,tmp);
        tmp=tmp->parent;

    }
    generate_expression(program,function,table,node->right,false);
    generate_epilogue(function);

}

std::vector<std::string> continue_labels;
std::vector<std::string> break_labels;

void generate_loop(asm_program &program, asm_function* function, symbol_table* table, ast_node* node){
    std::string start=function->create_local_label();
    std::string end=function->create_local_label();
    std::string continue_label=function->create_local_label();

    continue_labels.push_back(continue_label);
    break_labels.push_back(end);

    function->push_local_label(start);

    generate_expression(program,function,table,node->condition,false);
    
    function->push(instruction("CMP","RAX","0"));
    function->push(instruction("JE",end));

    generate_statement(program,function,table,node->right);

    function->push_local_label(continue_label);
    generate_expression(program,function,table,node->expressions,false);
    function->push(instruction("JMP",start));

    function->push_local_label(end);
    
    continue_labels.pop_back();
    break_labels.pop_back();

}

void generate_break(asm_function* function){
    if (break_labels.empty())emit_error(error::CODE_GENERATION_ERROR,"Break outside loop");

    function->push(instruction("JMP",break_labels.at(break_labels.size()-1)));
    
}

void generate_continue(asm_function* function){
    if (continue_labels.empty())emit_error(error::CODE_GENERATION_ERROR,"Continue outside loop");

    function->push(instruction("JMP",continue_labels.at(continue_labels.size()-1)));

}

void generate_if(asm_program &program, asm_function* function, symbol_table* table, ast_node* node){
    std::string endl=function->create_local_label();
    std::string notif=function->create_local_label();
    
    generate_expression(program,function,table,node->condition,false);

    function->push(instruction("CMP","RAX","0"));
    function->push(instruction("JE",notif));
    
    generate_statement(program,function,table,node->right);
    
    function->push(instruction("JMP",endl));

    function->push_local_label(notif);

    if (node->else_node)generate_statement(program,function,table,node->else_node->right);

    function->push_local_label(endl);

}

void pushaq(asm_function* function){
    function->push(instruction("PUSH","RAX"));
    function->push(instruction("PUSH","RBX"));
    function->push(instruction("PUSH","RCX"));
    function->push(instruction("PUSH","RDX"));
    function->push(instruction("PUSH","RDI"));
    function->push(instruction("PUSH","RSI"));
    function->push(instruction("PUSH","RBP"));
    function->push(instruction("PUSH","RSP"));
    function->push(instruction("PUSH","R8"));
    function->push(instruction("PUSH","R9"));
    function->push(instruction("PUSH","R10"));
    function->push(instruction("PUSH","R11"));
    function->push(instruction("PUSH","R12"));
    function->push(instruction("PUSH","R13"));
    function->push(instruction("PUSH","R14"));

}

void popaq(asm_function* function){
    function->push(instruction("POP","R14"));
    function->push(instruction("POP","R13"));
    function->push(instruction("POP","R12"));
    function->push(instruction("POP","R11"));
    function->push(instruction("POP","R10"));
    function->push(instruction("POP","R9"));
    function->push(instruction("POP","R8"));
    function->push(instruction("POP","RSP"));
    function->push(instruction("POP","RBP"));
    function->push(instruction("POP","RSI"));
    function->push(instruction("POP","RDI"));
    function->push(instruction("POP","RDX"));
    function->push(instruction("POP","RCX"));
    function->push(instruction("POP","RBX"));
    function->push(instruction("POP","RAX"));

}

void generate_label(asm_program &program, asm_function* function, symbol_table* table, ast_node* node){
    symbol* s=table->lookup_label(node->identifier);

    function->push(instruction(s->get_address()+":"));

}

void generate_asm(asm_program &program, asm_function* function, symbol_table* table, ast_node* node){
    std::string opcode=node->opcode;

    int instruction_length=get_asm_instruction_length(opcode);

    if (instruction_length==1){
        function->push(instruction(opcode));
        return;
    }

    if (instruction_length==2){
        if (node->operand1_node==nullptr){
            function->push(instruction(opcode,node->operand1));
        }
        else{
            if (isjmp(opcode) && node->operand1_node->type==ast_node_type::IDENTIFIER && 
            table->lookup_label(node->operand1_node->identifier)!=nullptr){
                symbol* label=table->lookup_label(node->operand1_node->identifier);
                function->push(instruction(opcode,label->get_address()));
                return;

            }
            generate_expression(program,function,table,node->operand1_node,false);
            function->push(instruction(opcode,"RAX"));
        }
        return;
    }

    std::string op1,op2;

    if (node->operand1_node==nullptr){
        op1=node->operand1;

    }
    else{
        pushaq(function);
        generate_expression(program,function,table,node->operand1_node,true);
        function->push(instruction("MOV","R15","RAX"));
        popaq(function);
        op1="[R15]";

    }
    if (node->operand2_node==nullptr){
        op2=node->operand2;
    }
    else{
        pushaq(function);
        generate_expression(program,function,table,node->operand2_node,false);
        function->push(instruction("MOV","R15","RAX"));
        popaq(function);
        op2="R15";

    }

    function->push(instruction(opcode,op1,op2));

}

void generate_goto(asm_program &program, asm_function* function, symbol_table* table, ast_node* node){
    symbol* s=table->lookup_label(node->identifier);
    if (s==nullptr){
        emit_error(error::CODE_GENERATION_ERROR,"Unknown label '"+node->identifier+"'");
    }

    function->push(instruction("JMP",s->get_address()));

}

void generate_destroy(asm_program &program, asm_function* function, symbol_table* table, ast_node* node){
    symbol* s;
    
    if (node->right->type==ast_node_type::IDENTIFIER){
        
        s=table->lookup(node->right->identifier);
        if (s==nullptr){
            emit_error(error::CODE_GENERATION_ERROR,"Unknown symbol '"+node->right->identifier+"'");
        }

        generate_expression(program,function,table,node->right,true);

    }
    else if (node->right->type==ast_node_type::MEMBER_ACCESS){
        s=generate_member_access(program,function,table,node->right);

    }
    else{
        emit_error(error::CODE_GENERATION_ERROR,"Expected symbol after destroy");

    }

    if (s->is_struct){
        if (s->memberof_struct==nullptr)return;
        
        symbol* s2=table->lookup_struct(s->parent_struct->name)->destructor;
        
        if (s2!=nullptr){
            function->push(instruction("PUSH","RAX"));
            function->push(instruction("CALL",s2->get_address()));
            function->push(instruction("ADD","RSP","8"));

        }

    }
    else if (!s->struct_ref.empty()){
        if (table->lookup_struct(s->struct_ref)==nullptr){
            emit_error(error::CODE_GENERATION_ERROR,"Unknown struct '"+s->struct_ref+"'");
        }

        symbol* s2=table->lookup_struct(s->struct_ref)->destructor;

        if (s2!=nullptr){
            function->push(instruction("MOV","RAX","[RAX]"));
            function->push(instruction("PUSH","RAX"));
            function->push(instruction("CALL",s2->get_address()));
            function->push(instruction("ADD","RSP","8"));

        }
        
    }
    else{
        emit_error(error::CODE_GENERATION_ERROR, "'"+s->name+"' does not belong to or reference a struct");
    }

}

void generate_struct_declaration(asm_program &program, asm_function* function, symbol_table* table, ast_node* node);

void generate_statement(asm_program &program, asm_function* function, symbol_table* table, ast_node* node){
    // printf("GENERATING STATEMENT: %s\n",node->value.c_str());
    if (node==nullptr)return;

    if (node->type==ast_node_type::DECLARATION){
        generate_declaration(program,function,table,node);
    }
    else if (node->type==ast_node_type::ARRAY_DECLARATION){
        generate_array_declaration(program,function,table,node);
    }
    else if (node->type==ast_node_type::STRUCT_DECLARATION){
        generate_struct_declaration(program,function,table,node);
    }
    else if (node->type==ast_node_type::BLOCK){
        generate_block(program,function,node);
    }
    else if (node->type==ast_node_type::RETURN){
        generate_return(program,function,table,node);
    }
    else if (node->type==ast_node_type::LOOP){
        generate_loop(program,function,table,node);
    }
    else if (node->type==ast_node_type::CONTINUE){
        generate_continue(function);
    }
    else if (node->type==ast_node_type::BREAK){
        generate_break(function);
    }
    else if (node->type==ast_node_type::IF){
        generate_if(program,function,table,node);
    }
    else if (node->type==ast_node_type::ASM){
        generate_asm(program,function,table,node);
    }
    else if (node->type==ast_node_type::LABEL){
        generate_label(program,function,table,node);
    }
    else if (node->type==ast_node_type::GOTO){
        generate_goto(program,function,table,node);
    }
    else if (node->type==ast_node_type::DESTROY){
        generate_destroy(program,function,table,node);
    }
    else{
        generate_expression(program,function,table,node,false);
    }

}

void generate_function(asm_program &program, ast_node* node){
    asm_function* function=new asm_function(node->identifier);

    symbol_table* table=node->table;

    function->push(instruction("PUSH","RBP"));
    function->push(instruction("MOV","RBP","RSP"));

    if (table->total_size){
        function->push(instruction("SUB","RSP",std::to_string(table->total_size)));
    }
    
    for (int i=0;i<node->children.size();i++){
        ast_node* statement=node->children.get(i);

        generate_statement(program,function,table,statement);

    }

    generate_destruction(function,table);

    generate_epilogue(function);

    program.push_function(function);

}

void generate_struct_declaration(asm_program &program, asm_function* function, symbol_table* table, ast_node* node){
    symbol* s=table->lookup(node->identifier);

    if (s->type==symbol_type::GLOBAL){
        program.push_global(node->identifier);

        std::string arrname=program.push_array(data_size::BYTE,s->size);

        function->push(instruction("MOV","RAX",arrname));
        function->push(instruction("MOV","["+s->get_address()+"]","RAX"));

    }
    else{
        function->push(instruction("LEA","RAX","["+s->get_address()+"]"));
    }

    if (node->construct){
        if (s->type==symbol_type::LOCAL){
            table->push_to_destroy(s);
        }
        symbol* s2=table->lookup_constructor(s->parent_struct->name,node->args.size());

        if (s2==nullptr){
            emit_error(error::CODE_GENERATION_ERROR,"No constructor for struct '"+s->parent_struct->name+"' with arg count "+std::to_string(node->args.size()));
        }

        for (int i=node->args.size()-1;i>=0;i--){
            function->push(instruction("PUSH","RAX"));
            generate_expression(program,function,table,node->args.get(i),false);
            function->push(instruction("MOV","RBX","RAX"));
            function->push(instruction("POP","RAX"));
            function->push(instruction("PUSH","RBX"));

        }

        function->push(instruction("PUSH","RAX"));

        function->push(instruction("CALL",s2->get_address()));

        function->push(instruction("ADD","RSP",std::to_string(8+node->args.size()*8)));

    }

}

void generate_constructor(asm_program &program, symbol* s, ast_node* node){
    asm_function* constructor=new asm_function("?"+s->name+std::to_string(s->constructor_table->param_count));

    symbol_table* table=node->table;

    constructor->push(instruction("PUSH","RBP"));
    constructor->push(instruction("MOV","RBP","RSP"));

    if (table->total_size){
        constructor->push(instruction("SUB","RSP",std::to_string(table->total_size)));
    }

    for (int i=0;i<node->children.size();i++){
        ast_node* statement=node->children.get(i);
        generate_statement(program,constructor,table,statement);

    }

    generate_destruction(constructor,table);

    generate_epilogue(constructor);

    program.push_function(constructor);
    
}

void generate_destructor(asm_program &program, symbol* s, ast_node* node){
    asm_function* destructor=new asm_function("??"+s->name);

    symbol_table* table=node->table;

    destructor->push(instruction("PUSH","RBP"));
    destructor->push(instruction("MOV","RBP","RSP"));

    if (table->total_size){
        destructor->push(instruction("SUB","RSP",std::to_string(table->total_size)));
    }

    for (int i=0;i<node->children.size();i++){
        ast_node* statement=node->children.get(i);
        generate_statement(program,destructor,table,statement);

    }

    generate_destruction(destructor,table);

    generate_epilogue(destructor);

    program.push_function(destructor);
    
}

void generate_method(asm_program &program, symbol* s, ast_node* node){
    asm_function* method=new asm_function("?"+s->parent_struct->name+s->name+std::to_string(s->method_table->param_count));

    symbol_table* table=node->table;

    method->push(instruction("PUSH","RBP"));
    method->push(instruction("MOV","RBP","RSP"));

    if (table->total_size){
        method->push(instruction("SUB","RSP",std::to_string(table->total_size)));
    }

    for (int i=0;i<node->children.size();i++){
        ast_node* statement=node->children.get(i);
        generate_statement(program,method,table,statement);

    }

    generate_destruction(method,table);

    generate_epilogue(method);

    program.push_function(method);

}

void generate_code(asm_program &program, symbol_table* table, ast &tree){
    program._start.push(instruction("MOV","R8","8"));

    {

        if (table->lookup("main")==nullptr){
            emit_error(error::CODE_GENERATION_ERROR,"No main function");
        }
    }
    
    for (int i=0;i<table->constructors.size();i++){
        generate_constructor(program,table->constructors.at(i),table->constructors.at(i)->constructor_node);

    }

    for (int i=0;i<table->destructors.size();i++){
        generate_destructor(program,table->destructors.at(i),table->destructors.at(i)->destructor_node);

    }

    for (int i=0;i<table->methods.size();i++){
        generate_method(program,table->methods.at(i),table->methods.at(i)->method_node);

    }

    for (int i=0;i<tree.size();i++){
        ast_node* node=tree.get(i);

        if (node->type==ast_node_type::DECLARATION){
            generate_declaration(program,&program._start,table,node);

        }
        else if (node->type==ast_node_type::ARRAY_DECLARATION){
            generate_array_declaration(program,&program._start,table,node);

        }
        else if (node->type==ast_node_type::STRUCT_DECLARATION){
            generate_struct_declaration(program,&program._start,table,node);

        }
        else if (node->type==ast_node_type::FUNCTION_DEFINITION){
            generate_function(program, node);
            
        }

    }

    program._start.push(instruction("MOV","RAX","[RSP]"));
    program._start.push(instruction("LEA","RBX","[RSP+8]"));

    program._start.push(instruction("PUSH","RBX"));
    program._start.push(instruction("PUSH","RAX"));
    program._start.push(instruction("CALL","?main"));

    program._start.push(instruction("ADD","RSP","16"));
    program._start.push(instruction("MOV","RDI","RAX"));
    program._start.push(instruction("MOV","RAX","60"));
    program._start.push(instruction("SYSCALL"));

}

std::vector<std::filesystem::path> paths;

bool paths_contains(std::filesystem::path name){
    for (int i=0;i<paths.size();i++){
        if (paths.at(i)==name)return true;
    }
    return false;
}

std::string find_include_file(std::string file){
    std::filesystem::path test1="/usr/include/cm/"+file;
    std::filesystem::path test2="/usr/local/include/cm/"+file;

    if (std::filesystem::exists(test1))return test1.string();
    if (std::filesystem::exists(test2))return test2.string();

    return "";

}

std::string expand(std::filesystem::path path){
    std::string src=get_file_as_string(path.string());

    token_list list=lex(src);

    std::filesystem::path dir=path.parent_path();
    
    std::string out;

    while (list.seti("#include")){
        list++;
        std::filesystem::path inc;
        std::filesystem::path canonical;
        std::string name;

        if (list.match("<")){
            list++;
            int lno=list.peek().line_number;
            std::string tmp;

            while (!list.match(">")){
                tmp+=list++.value;
            }
            
            list.expect(">");

            std::string include_file=find_include_file(tmp);
            if (include_file.empty()){
                emit_error(error::PREPROCESSING_ERROR,"No such include file: '"+tmp+"'",lno);
            }

            inc=include_file;
            

        }
        else{
            name=list.peek().value;
            inc=dir/name;

        }
        std::error_code ec;
        canonical=std::filesystem::canonical(inc,ec);
        if (ec){
            emit_error(error::PREPROCESSING_ERROR,"No such include file: '"+inc.string()+"'");
        }
        if (!paths_contains(canonical)){
            paths.push_back(canonical);
            out+=expand(canonical);
        }

    }

    return out+src;

}

void get_includes(std::string &filestr){
    std::error_code ec;
    std::filesystem::path p=std::filesystem::canonical(input_file,ec);
    if (ec){
        emit_error(error::PREPROCESSING_ERROR,"No such file: '"+input_file+"'");
    }
    paths.push_back(p);
    filestr=expand(p);

}

void remove_includes(token_list &list){
    while (list.seti("#include")){
        int i=list.i;
        list.remove(i);
        if (list.at(i).value=="<"){
            while (list.at(i).value!=">"){
                list.remove(i);
            }
            
        }
        list.remove(i);
    }
    list.seti(0);
}

int main(int argc, char** argv){
    std::cout << "MUST HAVE NASM INSTALLED!" << std::endl << std::endl;

    bool asm_only=false;

    char* whoami=argv[0];
    
    for (int i=1;i<argc;i++){
        if ((std::string)argv[i]=="--help"){
            print_help();
            return 0;
        }
        if ((std::string)argv[i]=="-a"){
            asm_only=true;
            continue;

        }

    }

    if (argc<3){
        std::cerr << "Incorrect number of args!" << std::endl;
        return 1;
        
    }

    input_file=argv[1],output_file=argv[2];

    std::string file;

    get_includes(file);

    token_list tokens=lex(file);
    remove_includes(tokens);
    // return 0;
    ast tree=parse(tokens);
    // tree.print(0);
    // return 0;
    symbol_table* global_table=new symbol_table(symbol_table_type::GLOBAL);

    for (int i=0;i<tree.size();i++){
        build_symbol_table(tree.get(i),global_table);
    
    }
    asm_program program;
    // return 0;
    generate_code(program,global_table,tree);

    std::string nasm_string=program.to_string();

    std::string nasmstr=output_file+".asm";

    std::ofstream nasm_file(nasmstr);

    nasm_file << nasm_string;

    nasm_file.close();

    std::string objstr=output_file+".o";

    std::string cmd1="nasm -f elf64 "+nasmstr+" -o "+objstr;
    std::string cmd2="ld "+objstr+" -o "+output_file;
    std::string cmd3="rm "+nasmstr+" "+objstr;

    if (!asm_only){
        std::system(cmd1.c_str());
        std::system(cmd2.c_str());
        std::system(cmd3.c_str());

    }
    
    return 0;
}