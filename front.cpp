// 20232068 이현택 프로그래밍언어론 과제
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <cctype>
#include <vector>
#include <memory>

using namespace std;

// 전역 변수 선언

/*
토큰 종류 정의
ID : 식별자, CONST : 숫자, ASSIGN_OP : 대입 연산자(:=), SEMICOLON(;)
ADD_OP : 덧셈/뺄셈 연산자(+,-), MULT_OP : 곱셈/나눗셈 연산자(*,/)
LEFT_PAREN : 왼쪽 괄호, RIGHT_PAREN : 오른쪽
END_OF_FILE : 파일 끝, UNKNOWN : 알 수 없는 토큰
*/
enum TokenType
{
    ID,
    CONST,
    ASSIGN_OP,
    SEMICOLON,
    ADD_OP,
    MULT_OP,
    LEFT_PAREN,
    RIGHT_PAREN,
    END_OF_FILE,
    UNKNOWN
};

/*
어휘 분석기 전역 변수
token_string : 현재 토큰의 문자열 ("operand1", "123", ...)
next_token : 현재 토큰의 타입 (ID, CONST, ...)
input_line : 현재 읽고 있는 한 줄 ("operand1 := 3;")
char_index : 현재 줄에서 읽고 있는 위치 (0부터 시작)
input_file : 입력 파일 스트림
*/
string token_string;
TokenType next_token;
string input_line;
size_t char_index = 0;
ifstream input_file;

/*
심볼 테이블
symbol_table : 변수명 -> 값 매핑 ("operand1" -> 3.0)
is_defined : 변수 정의 여부 ("operand1" -> true/false)
*/
map<string, double> symbol_table;
map<string, bool> is_defined;

/*
토큰 카운터 (각 문장에서 id, const, op 개수 세기 위한 변수)
*/
int id_count, const_count, op_count;

/*
에러 처리
has_parsing_error : 구문 분석 에러 발생 여부
has_evaluation_error : 평가 에러 발생 여부
parsing_error_messages : 구문 분석 에러 메시지 리스트
referenced_undefined_vars : 참조된 정의되지 않은 변수 리스트
*/
bool has_parsing_error = false;
bool has_evaluation_error = false;
vector<string> parsing_error_messages;
vector<string> referenced_undefined_vars;

// AST 노드 구조

/*
ASTNode : 추상 구문 트리의 기본 노드 클래스 (부모 노드)
*/
struct ASTNode
{
    virtual ~ASTNode() {}          // 가상 소멸자
    virtual double evaluate() = 0; // 순수 가상 함수 (각 자식 노드들에서 override하여 구현)
};

/*
NumberNode : 숫자 노드
*/
struct NumberNode : public ASTNode
{
    double value;
    NumberNode(double v) : value(v) {}           // v를 value에 저장
    double evaluate() override { return value; } // 저장된 값(value) 반환
};

/*
IdentNode : 식별자 노드
*/
struct IdentNode : public ASTNode
{
    string name;                     // 식별자 명(변수 명) 저장
    IdentNode(string n) : name(n) {} // n을 name에 저장
    double evaluate() override       // 심볼 테이블에서 변수 값 반환
    {
        if (!is_defined[name]) // 정의 되지 않은 변수 참조 시 에러 처리
        {
            has_evaluation_error = true;
            return 0;
        }
        return symbol_table[name];
    }
};

/*
BinaryOpNode : 이항 연산자 노드
*/

struct BinaryOpNode : public ASTNode
{
    char op;                   // 연산자 저장
    shared_ptr<ASTNode> left;  // 왼쪽 피연산자 노드
    shared_ptr<ASTNode> right; // 오른쪽 피연산자 노드

    BinaryOpNode(char o, shared_ptr<ASTNode> l, shared_ptr<ASTNode> r)
        : op(o), left(l), right(r) {}

    double evaluate() override // 왼쪽과 오른쪽 evaluate() 호출 후 연산자에 따라 계산
    {
        double l = left->evaluate();
        double r = right->evaluate();
        switch (op)
        {
        case '+':
            return l + r;
        case '-':
            return l - r;
        case '*':
            return l * r;
        case '/':
            return r != 0 ? l / r : 0;
        default:
            return 0;
        }
    }
};

// 어휘 분석, 구문 분석 함수 사전 정의

/*
어휘 분석 함수(lexical(), get_next_line())및 구문 분석 함수(expression(), term(), factor()) 선언
*/
void lexical();
void get_next_line();
shared_ptr<ASTNode> expression();
shared_ptr<ASTNode> term();
shared_ptr<ASTNode> factor();

/*
skip_whitespace : 공백 문자 건너뛰기 (pdf 내 조건대로 ASCII 32 이하 문자 처리)
*/
void skip_whitespace()
{
    while (char_index < input_line.length() &&
           (unsigned char)input_line[char_index] <= 32)
    {
        char_index++;
    }
}

// 어휘 분석 구현

/*
lexical : 어휘 분석 메인 함수
*/
void lexical()
{
    skip_whitespace(); // 공백 문자 건너뛰기
    token_string = ""; // 현재 토큰 문자열 초기화

    if (char_index >= input_line.length()) // 줄 끝 도달 시 처리
    {
        next_token = END_OF_FILE;
        return;
    }

    char ch = input_line[char_index];

    // 식별자 인식(첫 글자 알파벳 or _ -> 이후 알파벳, 숫자, _ 계속)
    if (isalpha(ch) || ch == '_')
    {
        while (char_index < input_line.length() &&
               (isalnum(input_line[char_index]) || input_line[char_index] == '_'))
        {
            token_string += input_line[char_index++];
        }
        next_token = ID;
        id_count++;
    }
    // 숫자 인식 (숫자로 시작, 소수점 포함 가능)
    else if (isdigit(ch))
    {
        while (char_index < input_line.length() &&
               (isdigit(input_line[char_index]) || input_line[char_index] == '.'))
        {
            token_string += input_line[char_index++];
        }
        next_token = CONST;
        const_count++;
    }
    // 대입 연산자 (:=) 인식
    else if (ch == ':' && char_index + 1 < input_line.length() &&
             input_line[char_index + 1] == '=')
    {
        token_string = ":=";
        char_index += 2;
        next_token = ASSIGN_OP;
    }
    // 덧셈/뺄셈 연산자 인식
    else if (ch == '+' || ch == '-')
    {
        token_string = ch;
        char_index++;
        next_token = ADD_OP;
        op_count++;
    }
    // 곱셈/나눗셈 연산자 인식
    else if (ch == '*' || ch == '/')
    {
        token_string = ch;
        char_index++;
        next_token = MULT_OP;
        op_count++;
    }
    // 괄호, 세미콜론 인식
    else if (ch == '(')
    {
        token_string = ch;
        char_index++;
        next_token = LEFT_PAREN;
    }
    else if (ch == ')')
    {
        token_string = ch;
        char_index++;
        next_token = RIGHT_PAREN;
    }
    else if (ch == ';')
    {
        token_string = ch;
        char_index++;
        next_token = SEMICOLON;
    }
    else // 알수 없는 토큰 인식
    {
        token_string = ch;
        char_index++;
        next_token = UNKNOWN;
    }
}

/*
get_next_line : 파일에서 다음 줄 읽기 및 주석 처리
*/
void get_next_line()
{
    if (getline(input_file, input_line))
    {
        char_index = 0;
        size_t comment_pos = input_line.find("//"); // 주석 처리
        if (comment_pos != string::npos)
        {
            input_line = input_line.substr(0, comment_pos);
        }
    }
    else
    {
        input_line = "";
        char_index = 0;
    }
}

// 구문 분석 구현

/*
factor():
<factor> → <left_paren> <expression> <right_paren> | <ident> | <const> 구현
*/
shared_ptr<ASTNode> factor()
{
    // "(" 발견 시 재귀적으로 expression() 호출, ")" 없으면 에러 처리
    if (next_token == LEFT_PAREN)
    {
        lexical();
        shared_ptr<ASTNode> node = expression();
        if (next_token != RIGHT_PAREN)
        {
            has_parsing_error = true;
            parsing_error_messages.push_back("닫는 괄호가 누락됨");
            return make_shared<NumberNode>(0);
        }
        lexical();
        return node;
    }
    // 식별자 발견 시 변수명 저장, 정의되지 않았으면 referenced_undefined_vars에 추가, 이후 IdentNode 반환
    else if (next_token == ID)
    {
        string var_name = token_string;
        if (!is_defined[var_name])
        {
            referenced_undefined_vars.push_back(var_name);
        }
        lexical();
        return make_shared<IdentNode>(var_name);
    }
    // 숫자 발견 시 문자열을 double로 변환 후 NumberNode 생성
    else if (next_token == CONST)
    {
        double value = stod(token_string);
        lexical();
        return make_shared<NumberNode>(value);
    }
    // 그 외 에러 처리
    else
    {
        has_parsing_error = true;
        parsing_error_messages.push_back("예상치 못한 토큰: " + token_string);
        return make_shared<NumberNode>(0);
    }
}

/*
term():
<term> → <factor> <factor_tail>
<factor_tail> → <mult_op> <factor> <factor_tail> | ε 구현
*/
shared_ptr<ASTNode> term()
{
    shared_ptr<ASTNode> node = factor(); // factor() 호출로 첫 번째 피연산자

    while (next_token == MULT_OP) // * or / 계속 나오는 동안
    {
        char op = token_string[0]; // 연산자 저장
        lexical();                 // 다음 토큰 읽기

        if (next_token == MULT_OP || next_token == ADD_OP) // 중복 연산자 체크
        {
            has_parsing_error = true;
            parsing_error_messages.push_back("중복 연산자(" + string(1, op) + ") 발생");
            lexical();
        }

        shared_ptr<ASTNode> right = factor();              // 다음 factor() 호출
        node = make_shared<BinaryOpNode>(op, node, right); // BinaryOpNode 생성 (왼쪽에 이전 결과, 오른쪽에 새 factor)
    }

    return node;
}

/*
expression():
<expression> → <term> <term_tail>
<term_tail> → <add_op> <term> <term_tail> | ε 구현
*/
shared_ptr<ASTNode> expression()
{
    shared_ptr<ASTNode> node = term(); // term() 호출

    while (next_token == ADD_OP) // + or - 계속 나오는 동안
    {
        char op = token_string[0]; // 연산자 저장
        lexical();                 // 다음 토큰 읽기

        if (next_token == ADD_OP || next_token == MULT_OP) // 중복 연산자 체크
        {
            has_parsing_error = true;
            parsing_error_messages.push_back("중복 연산자(" + string(1, op) + ") 발생");
            lexical();
        }

        shared_ptr<ASTNode> right = term();                // 다음 term() 호출
        node = make_shared<BinaryOpNode>(op, node, right); // BinaryOpNode 생성 (왼쪽에 이전 결과, 오른쪽에 새 term)
    }

    return node;
}

/*
print_error_and_return() : 에러 출력 함수
*/
void print_error_and_return(const string &original_line)
{
    cout << original_line << endl;
    cout << "ID: " << id_count << "; CONST: " << const_count
         << "; OP: " << op_count << ";" << endl;
    for (const auto &msg : parsing_error_messages)
    {
        cout << "(ERROR) " << msg << endl;
    }
}

/*
statement():
<statement> → <ident> <assignment_op> <expression> 구현
*/
void statement()
{
    // 카운터, 플래그 초기화
    id_count = const_count = op_count = 0;
    has_parsing_error = has_evaluation_error = false;
    parsing_error_messages.clear();
    referenced_undefined_vars.clear();

    string original_line = input_line; // 원본 라인 저장 (출력용)
    lexical();

    // 식별자 확인
    if (next_token != ID)
    {
        has_parsing_error = true;
        parsing_error_messages.push_back("식별자가 필요함");
        print_error_and_return(original_line);
        return;
    }

    string var_name = token_string;
    lexical();

    // 대입 연산자(:=) 확인
    if (next_token != ASSIGN_OP)
    {
        has_parsing_error = true;
        parsing_error_messages.push_back("대입 연산자(:=)가 필요함");
        print_error_and_return(original_line);
        return;
    }

    lexical();
    shared_ptr<ASTNode> expr_tree = expression(); // expression() 호출로 AST 구축

    // 토큰 카운트 출력
    cout << original_line << endl;
    cout << "ID: " << id_count << "; CONST: " << const_count
         << "; OP: " << op_count << ";" << endl;

    // 파싱 / 평가 분리 로직
    if (has_parsing_error) // 구문 에러 처리
    {
        for (const auto &msg : parsing_error_messages)
        {
            cout << "(ERROR) " << msg << endl;
        }
        is_defined[var_name] = false;
    }
    else // 구문 성공 시
    {
        cout << "(OK)" << endl;

        // 정의되지 않은 변수 체크
        bool has_undefined = false;
        for (const auto &var : referenced_undefined_vars)
        {
            if (!is_defined[var])
            {
                has_undefined = true;
                break;
            }
        }

        // 모든 변수 정의 됨 -> 평가 실행
        if (!has_undefined)
        {
            double result = expr_tree->evaluate();
            if (!has_evaluation_error)
            {
                symbol_table[var_name] = result;
                is_defined[var_name] = true;
            }
            else
            {
                is_defined[var_name] = false;
            }
        }
        else
        {
            // 정의 되지 않은 변수 존재 -> Unknown 처리
            is_defined[var_name] = false;
        }
    }
}

/*
statemnets():
<statements> → <statement> <stmts_tail> 구현
*/
void statements()
{
    statement(); // 첫 번째 문장 처리

    // 세미콜론으로 구분된 문장들 처리
    while (next_token == SEMICOLON)
    {
        lexical();
        get_next_line();
        if (!input_line.empty())
        {
            statement();
        }
    }

    // 남은 문장들 처리
    get_next_line();
    while (!input_line.empty())
    {
        statement();
        if (next_token == SEMICOLON)
        {
            lexical();
        }
        get_next_line();
    }
}

/*
program():
<program> → <statements> 구현
*/
void program()
{
    // 문장들 처리
    get_next_line();
    if (!input_line.empty()) // 빈 파일이 아니면 모든 문장 읽기
    {
        statements();
    }

    // Result 출력
    cout << "Result ==> ";
    bool first = true;                  // 첫 번째 변수인지 추적
    for (const auto &pair : is_defined) // 모든 변수 순회
    {
        if (!first) // 첫 번째 변수가 아니면 세미콜론 출력
            cout << "; ";
        first = false;
        cout << pair.first << ": "; // 변수명, 콜론 출력
        if (pair.second)            // 변수가 정의되어 있으면
        {
            double val = symbol_table[pair.first]; // 값 가져오기
            if (val == (int)val)                   // 정수인지 확인 후 정수 or 실수로 출력
            {
                cout << (int)val;
            }
            else
            {
                cout << val;
            }
        }
        else // 정의되지 않았으면 Unknown 출력
        {
            cout << "Unknown";
        }
    }
    cout << ";" << endl;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "Usage: " << argv[0] << " <input_file>" << endl;
        return 1;
    }

    input_file.open(argv[1]);
    if (!input_file.is_open())
    {
        cerr << "Error: Cannot open file " << argv[1] << endl;
        return 1;
    }

    program();
    input_file.close();
    return 0;
}
