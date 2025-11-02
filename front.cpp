#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <cctype>
#include <vector>
#include <memory>

using namespace std;

// Token types
enum TokenType {
    ID, CONST, ASSIGN_OP, SEMICOLON, ADD_OP, MULT_OP,
    LEFT_PAREN, RIGHT_PAREN, END_OF_FILE, UNKNOWN
};

// Global variables for lexical analyzer
string token_string;
TokenType next_token;
string input_line;
size_t char_index = 0;
ifstream input_file;

// Symbol table to store variable values
map<string, double> symbol_table;
map<string, bool> is_defined;

// Counters for each statement
int id_count, const_count, op_count;

// Error flags
bool has_parsing_error = false;
bool has_evaluation_error = false;
vector<string> parsing_error_messages;
vector<string> referenced_undefined_vars;

// AST Node structures
struct ASTNode {
    virtual ~ASTNode() {}
    virtual double evaluate() = 0;
};

struct NumberNode : public ASTNode {
    double value;
    NumberNode(double v) : value(v) {}
    double evaluate() override { return value; }
};

struct IdentNode : public ASTNode {
    string name;
    IdentNode(string n) : name(n) {}
    double evaluate() override {
        if (!is_defined[name]) {
            has_evaluation_error = true;
            return 0;
        }
        return symbol_table[name];
    }
};

struct BinaryOpNode : public ASTNode {
    char op;
    shared_ptr<ASTNode> left;
    shared_ptr<ASTNode> right;

    BinaryOpNode(char o, shared_ptr<ASTNode> l, shared_ptr<ASTNode> r)
        : op(o), left(l), right(r) {}

    double evaluate() override {
        double l = left->evaluate();
        double r = right->evaluate();
        switch(op) {
            case '+': return l + r;
            case '-': return l - r;
            case '*': return l * r;
            case '/': return r != 0 ? l / r : 0;
            default: return 0;
        }
    }
};

// Function prototypes
void lexical();
void get_next_line();
shared_ptr<ASTNode> expression();
shared_ptr<ASTNode> term();
shared_ptr<ASTNode> factor();

// Skip whitespace
void skip_whitespace() {
    while (char_index < input_line.length() &&
           (unsigned char)input_line[char_index] <= 32) {
        char_index++;
    }
}

// Lexical analyzer
void lexical() {
    skip_whitespace();
    token_string = "";

    if (char_index >= input_line.length()) {
        next_token = END_OF_FILE;
        return;
    }

    char ch = input_line[char_index];

    // Identifier
    if (isalpha(ch) || ch == '_') {
        while (char_index < input_line.length() &&
               (isalnum(input_line[char_index]) || input_line[char_index] == '_')) {
            token_string += input_line[char_index++];
        }
        next_token = ID;
        id_count++;
    }
    // Number
    else if (isdigit(ch)) {
        while (char_index < input_line.length() &&
               (isdigit(input_line[char_index]) || input_line[char_index] == '.')) {
            token_string += input_line[char_index++];
        }
        next_token = CONST;
        const_count++;
    }
    // Assignment operator :=
    else if (ch == ':' && char_index + 1 < input_line.length() &&
             input_line[char_index + 1] == '=') {
        token_string = ":=";
        char_index += 2;
        next_token = ASSIGN_OP;
    }
    // Add operators
    else if (ch == '+' || ch == '-') {
        token_string = ch;
        char_index++;
        next_token = ADD_OP;
        op_count++;
    }
    // Multiply operators
    else if (ch == '*' || ch == '/') {
        token_string = ch;
        char_index++;
        next_token = MULT_OP;
        op_count++;
    }
    // Parentheses and semicolon
    else if (ch == '(') {
        token_string = ch;
        char_index++;
        next_token = LEFT_PAREN;
    }
    else if (ch == ')') {
        token_string = ch;
        char_index++;
        next_token = RIGHT_PAREN;
    }
    else if (ch == ';') {
        token_string = ch;
        char_index++;
        next_token = SEMICOLON;
    }
    else {
        token_string = ch;
        char_index++;
        next_token = UNKNOWN;
    }
}

// Get next line from input
void get_next_line() {
    if (getline(input_file, input_line)) {
        char_index = 0;
        size_t comment_pos = input_line.find("//");
        if (comment_pos != string::npos) {
            input_line = input_line.substr(0, comment_pos);
        }
    } else {
        input_line = "";
        char_index = 0;
    }
}

// <factor> → <left_paren> <expression> <right_paren> | <ident> | <const>
shared_ptr<ASTNode> factor() {
    if (next_token == LEFT_PAREN) {
        lexical();
        shared_ptr<ASTNode> node = expression();
        if (next_token != RIGHT_PAREN) {
            has_parsing_error = true;
            parsing_error_messages.push_back("닫는 괄호가 누락됨");
            return make_shared<NumberNode>(0);
        }
        lexical();
        return node;
    }
    else if (next_token == ID) {
        string var_name = token_string;
        if (!is_defined[var_name]) {
            referenced_undefined_vars.push_back(var_name);
        }
        lexical();
        return make_shared<IdentNode>(var_name);
    }
    else if (next_token == CONST) {
        double value = stod(token_string);
        lexical();
        return make_shared<NumberNode>(value);
    }
    else {
        has_parsing_error = true;
        parsing_error_messages.push_back("예상치 못한 토큰: " + token_string);
        return make_shared<NumberNode>(0);
    }
}

// <term> → <factor> <factor_tail>
// <factor_tail> → <mult_op> <factor> <factor_tail> | ε
shared_ptr<ASTNode> term() {
    shared_ptr<ASTNode> node = factor();

    while (next_token == MULT_OP) {
        char op = token_string[0];
        lexical();

        if (next_token == MULT_OP || next_token == ADD_OP) {
            has_parsing_error = true;
            parsing_error_messages.push_back("중복 연산자(" + string(1, op) + ") 발생");
            op_count--;
            lexical();
        }

        shared_ptr<ASTNode> right = factor();
        node = make_shared<BinaryOpNode>(op, node, right);
    }

    return node;
}

// <expression> → <term> <term_tail>
// <term_tail> → <add_op> <term> <term_tail> | ε
shared_ptr<ASTNode> expression() {
    shared_ptr<ASTNode> node = term();

    while (next_token == ADD_OP) {
        char op = token_string[0];
        lexical();

        if (next_token == ADD_OP || next_token == MULT_OP) {
            has_parsing_error = true;
            parsing_error_messages.push_back("중복 연산자(" + string(1, op) + ") 발생");
            op_count--;
            lexical();
        }

        shared_ptr<ASTNode> right = term();
        node = make_shared<BinaryOpNode>(op, node, right);
    }

    return node;
}

// Print error and return
void print_error_and_return(const string& original_line) {
    cout << original_line << endl;
    cout << "ID: " << id_count << "; CONST: " << const_count
         << "; OP: " << op_count << ";" << endl;
    for (const auto& msg : parsing_error_messages) {
        cout << "(ERROR) " << msg << endl;
    }
}

// <statement> → <ident> <assignment_op> <expression>
void statement() {
    id_count = const_count = op_count = 0;
    has_parsing_error = has_evaluation_error = false;
    parsing_error_messages.clear();
    referenced_undefined_vars.clear();

    string original_line = input_line;
    lexical();

    if (next_token != ID) {
        has_parsing_error = true;
        parsing_error_messages.push_back("식별자가 필요함");
        print_error_and_return(original_line);
        return;
    }

    string var_name = token_string;
    lexical();

    if (next_token != ASSIGN_OP) {
        has_parsing_error = true;
        parsing_error_messages.push_back("대입 연산자(:=)가 필요함");
        print_error_and_return(original_line);
        return;
    }

    lexical();
    shared_ptr<ASTNode> expr_tree = expression();

    cout << original_line << endl;
    cout << "ID: " << id_count << "; CONST: " << const_count
         << "; OP: " << op_count << ";" << endl;

    if (has_parsing_error) {
        for (const auto& msg : parsing_error_messages) {
            cout << "(ERROR) " << msg << endl;
        }
        is_defined[var_name] = false;
    } else {
        cout << "(OK)" << endl;

        // Check if there are any undefined variables
        bool has_undefined = false;
        for (const auto& var : referenced_undefined_vars) {
            if (!is_defined[var]) {
                has_undefined = true;
                break;
            }
        }

        // Only evaluate if all referenced variables are defined
        if (!has_undefined) {
            double result = expr_tree->evaluate();
            if (!has_evaluation_error) {
                symbol_table[var_name] = result;
                is_defined[var_name] = true;
            } else {
                is_defined[var_name] = false;
            }
        } else {
            // Cannot evaluate due to undefined variables
            is_defined[var_name] = false;
        }
    }
}

// <statements> → <statement> <stmts_tail>
void statements() {
    statement();

    while (next_token == SEMICOLON) {
        lexical();
        get_next_line();
        if (!input_line.empty()) {
            statement();
        }
    }

    get_next_line();
    while (!input_line.empty()) {
        statement();
        if (next_token == SEMICOLON) {
            lexical();
        }
        get_next_line();
    }
}

// <program> → <statements>
void program() {
    get_next_line();
    if (!input_line.empty()) {
        statements();
    }

    cout << "Result ==> ";
    bool first = true;
    for (const auto& pair : is_defined) {
        if (!first) cout << "; ";
        first = false;
        cout << pair.first << ": ";
        if (pair.second) {
            double val = symbol_table[pair.first];
            if (val == (int)val) {
                cout << (int)val;
            } else {
                cout << val;
            }
        } else {
            cout << "Unknown";
        }
    }
    cout << ";" << endl;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <input_file>" << endl;
        return 1;
    }

    input_file.open(argv[1]);
    if (!input_file.is_open()) {
        cerr << "Error: Cannot open file " << argv[1] << endl;
        return 1;
    }

    program();
    input_file.close();
    return 0;
}
