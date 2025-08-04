#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <cctype> // toupper()
#include <iomanip>
#include <unordered_map>
#include <map>

using namespace std;

// ───────────────────────────────────────────────────────────────
//  Fixed‑length string alias
// ───────────────────────────────────────────────────────────────
typedef char STRING20[20];     // up to 19 visible chars + null

void str_to20(const string &src, STRING20 dst) {
    strncpy(dst, src.c_str(), 19);
    dst[19] = '\0';
}

// ───────────────────────────────────────────────────────────────
//  Helper – case‑insensitive normalisation
// ───────────────────────────────────────────────────────────────
string upper_copy(string s) {
    for (char &c : s)
        c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return s;
}

int ascii_sum(string s) {
    int sum = 0;
    for (unsigned char c : s)
        sum += c;
    return sum;
}

// ───────────────────────────────────────────────────────────────
//  Delimiter / white‑space predicates
// ───────────────────────────────────────────────────────────────
string DELIMS = ",+-*/:;?' .=#@";
bool is_delim(char c) { return DELIMS.find(c) != string::npos; }
bool is_ws(char c)    { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

// ───────────────────────────────────────────────────────────────
//  Fixed tables (Table1‑4)
// ───────────────────────────────────────────────────────────────
class FixedTable
{
    vector<string> items; // item 0 = position 1
public:
    void load(const string &path) {
        ifstream in(path);
        if (!in)
            throw runtime_error("Cannot open " + path);

        string word;
        while (in >> word)
            items.push_back(upper_copy(word));
    }

    //! return 0‑based index or -1 if not find in table
    int find(const string &tok) const {
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i] == tok)
                return static_cast<int>(i);
        return -1;
    }

    const string &operator[](size_t i) const {
        return items[i];
    }
};

// ───────────────────────────────────────────────────────────────
//  Simple open‑addressed hashing table (Table5‑7)
// ───────────────────────────────────────────────────────────────
class HashTable100
{
    vector<string> slot;
public:
    HashTable100() : slot(100) {}

    //!find existing or insert new token, returning its slot index (0‑based)
    int locate(const string &tok) {
        int idx = ascii_sum(tok) % 100;
        for (int i = 0; i < 100; ++i) {
            int p = (idx + i) % 100;
            if (slot[p].empty()) {
                slot[p] = tok;      // insert
                return p;
            }
            if (slot[p] == tok)
                return p;           // found existing
        }
        throw runtime_error("Hash tables are full !!!");
    }
};

// ───────────────────────────────────────────────────────────────
//  Global tables
// ───────────────────────────────────────────────────────────────
FixedTable fixedTbl[4];         // Table1‑4
HashTable100 symTbl;            // Table5 – symbols / identifiers
HashTable100 numTbl;            // Table6 – numbers (integer / real)
HashTable100 strTbl;            // Table7 – strings (in quotes)

// ───────────────────────────────────────────────────────────────
//  Opcode table for pass‑2 code generation
// ───────────────────────────────────────────────────────────────
static const unordered_map<string, int> OPCODE = {
    {"ADD", 0x18},   {"ADDF", 0x58}, {"ADDR", 0x90}, {"AND", 0x40},
    {"CLEAR", 0xB4}, {"COMP", 0x28}, {"COMPF", 0x88}, {"COMPR", 0xA0},
    {"DIV", 0x24},   {"DIVF", 0x64}, {"DIVR", 0x9C}, {"FIX", 0xC4},
    {"FLOAT", 0xC0}, {"HIO", 0xF4},  {"J", 0x3C},    {"JEQ", 0x30},
    {"JGT", 0x34},   {"JLT", 0x38},  {"JSUB", 0x48}, {"LDA", 0x00},
    {"LDB", 0x68},   {"LDCH", 0x50}, {"LDF", 0x70}, {"LDL", 0x08},
    {"LDS", 0x6C},   {"LDT", 0x74},  {"LDX", 0x04}, {"LPS", 0xD0},
    {"MUL", 0x20},   {"MULF", 0x60}, {"MULR", 0x98}, {"NORM", 0xC8},
    {"OR", 0x44},    {"RD", 0xD8},   {"RMO", 0xAC}, {"RSUB", 0x4C},
    {"SHIFTL", 0xA4},{"SHIFTR", 0xA8},{"SIO", 0xF0}, {"SSK", 0xEC},
    {"STA", 0x0C},   {"STB", 0x78},  {"STCH", 0x54}, {"STF", 0x80},
    {"STI", 0xD4},   {"STL", 0x14},  {"STS", 0x7C}, {"STSW", 0xE8},
    {"STT", 0x84},   {"STX", 0x10},  {"SUB", 0x1C}, {"SUBF", 0x5C},
    {"SUBR", 0x94},  {"SVC", 0xB0},  {"TD", 0xE0},  {"TIO", 0xF8},
    {"TIX", 0x2C},   {"TIXR", 0xB8}, {"WD", 0xDC}
};

// ───────────────────────────────────────────────────────────────
//  Token
// ───────────────────────────────────────────────────────────────
struct Token {
    STRING20 lexeme{};  // original characters（in caps）
    int table = 0;      // 1..7
    int index = 0;      // 1‑based index inside that table (5~7 is 0 based)
};

// ───────────────────────────────────────────────────────────────
//  Utilities for classification
// ───────────────────────────────────────────────────────────────
static inline bool is_number(const string &s) {
    bool dot = false;
    for (char c : s) {
        if (c == '.') {
            if (dot)
                return false; // two dots → not number
            dot = true;
            continue;
        }
        if (!isdigit(static_cast<unsigned char>(c)))
            return false;
    }
    return !s.empty();
}

// convenience – string view of token
string tokstr(const Token &t) { return string(t.lexeme); }

// ───────────────────────────────────────────────────────────────
//  Representation of each source line for pass1/2
// ───────────────────────────────────────────────────────────────
struct LineRec {
    int lineNo = 0;
    int loc = -1;            // -1 if no location (comment / END)
    string label;
    string opcode;
    string operand;
    string obj;              // object code generated in pass2
    string comment;          // for pure comment lines (starting '.')
    bool isComment = false;
};

// helper – left trim
string ltrim(const string &s) {
    size_t p = s.find_first_not_of(" \t\r");
    return (p == string::npos) ? string() : s.substr(p);
}

// parse tokens into label/opcode/operand fields
void parse_line(const vector<Token> &toks, LineRec &rec) {
    size_t i = 0;
    if (toks.size() >= 2 && toks[0].table == 5 && (toks[1].table == 1 || toks[1].table == 2)) {
        rec.label = tokstr(toks[0]);
        i = 1;
    }
    if (i < toks.size())
        rec.opcode = tokstr(toks[i++]);
    for (; i < toks.size(); ++i)
        rec.operand += tokstr(toks[i]);
}

// ───────────────────────────────────────────────────────────────
//  Pass1 – build symbol table & record line info
// ───────────────────────────────────────────────────────────────
vector<LineRec> pass1(const vector<string> &raw, const vector<vector<Token>> &tokLines,
                      map<string, int> &symtab, vector<string> &errors)
{
    vector<LineRec> lines;
    int locctr = 0;
    bool started = false;
    int lineNo = 5;

    for (size_t idx = 0; idx < raw.size(); ++idx) {
        string trimmed = ltrim(raw[idx]);
        if (trimmed.empty())
            continue; // skip blank lines completely

        LineRec rec;
        rec.lineNo = lineNo;
        lineNo += 5;

        if (!trimmed.empty() && trimmed[0] == '.') {
            rec.isComment = true;
            rec.comment = trimmed;
            lines.push_back(rec);
            continue;
        }

        parse_line(tokLines[idx], rec);

        if (rec.opcode == "START") {
            locctr = stoi(rec.operand, nullptr, 16);
            rec.loc = locctr;
            if (!rec.label.empty())
                symtab[rec.label] = locctr;
            lines.push_back(rec);
            started = true;
            continue;
        }

        if (rec.opcode == "END") {
            rec.loc = -1;
            lines.push_back(rec);
            break;
        }

        if (!started)
            locctr = 0; // default start

        rec.loc = locctr;
        if (!rec.label.empty())
            symtab[rec.label] = locctr;

        int inc = 0;
        if (OPCODE.count(rec.opcode))
            inc = 3;
        else if (rec.opcode == "WORD")
            inc = 3;
        else if (rec.opcode == "RESW")
            inc = 3 * stoi(rec.operand);
        else if (rec.opcode == "RESB")
            inc = stoi(rec.operand);
        else if (rec.opcode == "BYTE") {
            if (rec.operand.size() >= 3 && rec.operand[0] == 'C' && rec.operand[1] == '\'')
                inc = static_cast<int>(rec.operand.size()) - 3;
            else if (rec.operand.size() >= 3 && rec.operand[0] == 'X' && rec.operand[1] == '\'')
                inc = (static_cast<int>(rec.operand.size()) - 3) / 2;
        }
        else if (!rec.opcode.empty()) {
            errors.push_back("Syntax error on line " + to_string(rec.lineNo) + ": unknown opcode '" + rec.opcode + "'");
        }

        locctr += inc;
        lines.push_back(rec);
    }

    return lines;
}

// ───────────────────────────────────────────────────────────────
//  Pass2 – generate object code
// ───────────────────────────────────────────────────────────────
void pass2(vector<LineRec> &lines, const map<string, int> &symtab, vector<string> &errors) {
    for (auto &rec : lines) {
        if (rec.isComment)
            continue;
        if (rec.opcode == "START" || rec.opcode == "END" || rec.opcode == "RESW" || rec.opcode == "RESB")
            continue; // no object code

        if (rec.opcode == "WORD") {
            int val = stoi(rec.operand);
            stringstream ss;
            ss << uppercase << setw(6) << setfill('0') << hex << val;
            rec.obj = ss.str();
            continue;
        }

        if (rec.opcode == "BYTE") {
            if (rec.operand[0] == 'C') {
                stringstream ss;
                for (size_t i = 2; i < rec.operand.size() - 1; ++i)
                    ss << uppercase << setw(2) << setfill('0') << hex
                       << (int)static_cast<unsigned char>(rec.operand[i]);
                rec.obj = ss.str();
            }
            else if (rec.operand[0] == 'X') {
                rec.obj = rec.operand.substr(2, rec.operand.size() - 3);
                for (auto &c : rec.obj)
                    c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
            }
            continue;
        }

        auto it = OPCODE.find(rec.opcode);
        if (it == OPCODE.end())
            continue; // unknown mnemonic

        int op = it->second;
        string operand = rec.operand;
        bool indexed = false;
        size_t pos = operand.find(",X");
        if (pos != string::npos) {
            indexed = true;
            operand = operand.substr(0, pos);
        }

        int addr = 0;
        if (!operand.empty()) {
            auto sit = symtab.find(operand);
            if (sit != symtab.end()) {
                addr = sit->second;
            }
            else if (is_number(operand)) {
                addr = stoi(operand); // treat as decimal constant
            }
            else {
                errors.push_back("Undefined symbol: '" + operand + "' at line " + to_string(rec.lineNo));
                continue;
            }
        }

        int obj = (op << 16) | (indexed ? 0x8000 : 0) | (addr & 0x7FFF);
        stringstream ss;
        ss << uppercase << setw(6) << setfill('0') << hex << obj;
        rec.obj = ss.str();
    }
}

// determine which table (and position) a token belongs to
static pair<int, int> place_token(const string &tok) {
    // 1—4: fixed tables
    for (int t = 0; t < 4; ++t) {
        int pos = fixedTbl[t].find(tok);
        if (pos != -1)
            return {t + 1, pos + 1};
    }
    // 5: symbol, 6: number, 7: string
    if (!tok.empty() && tok.front() == '\'' && tok.back() == '\'')
        return {7, strTbl.locate(tok)};
    else if (is_number(tok))
        return {6, numTbl.locate(tok)};
    else
        return {5, symTbl.locate(tok)};
}

void flush_buffer(vector<Token>& out, string& buf) { // process buffer contents
    if (buf.empty())
        return;
    string word = upper_copy(buf);
    auto [tbl, idx] = place_token(word);

    Token tk;
    str_to20(word, tk.lexeme);
    tk.table = tbl;
    tk.index = idx;
    out.push_back(tk);
    buf.clear();
}
// ───────────────────────────────────────────────────────────────
//  Core lexical analyser – returns stream of tokens with coords
// ───────────────────────────────────────────────────────────────
vector<Token> lex(const string &line, int lineNo, vector<string> &errors) {
    vector<Token> out;
    string buf; // accumulate non‑delimiter / non‑ws chars
    size_t i = 0, n = line.size();
    while (i < n) {
        char c = line[i];
        if (c == '.') {
            flush_buffer(out, buf);
            break; // comment line, skip the rest
        }
        // handle quoted string
        if (c == '\'') {
            flush_buffer(out, buf);
            string quoted;
            quoted += c;
            ++i;
            bool closed = false;
            while (i < n) {
                quoted += line[i];
                if (line[i] == '\'') {
                    ++i;
                    closed = true;
                    break;
                }
                ++i;
            }
            if (!closed) {
                errors.push_back("Lexical error on line " + to_string(lineNo) + ": missing closing quote");
            }
            auto [tbl, idx] = place_token(quoted);
            Token tk;
            str_to20(upper_copy(quoted), tk.lexeme);
            tk.table = tbl;
            tk.index = idx;
            out.push_back(tk);
            continue;
        }
        if (is_ws(c)) {
            flush_buffer(out, buf);
            ++i;
            continue;
        }
        if (is_delim(c)) {
            flush_buffer(out, buf);
            string delim(1, c);
            auto [tbl, idx] = place_token(delim);
            Token tk;
            str_to20(delim, tk.lexeme);
            tk.table = tbl;
            tk.index = idx;
            out.push_back(tk);
            ++i;
            continue;
        }
        // part of word
        buf += c;
        ++i;
    } // while
    flush_buffer(out, buf);
    return out;
}

// ───────────────────────────────────────────────────────────────
//  Entry
// ───────────────────────────────────────────────────────────────
int main()
{
    // load fixed tables – expect files beside executable
    fixedTbl[0].load("Table1.table");
    fixedTbl[1].load("Table2.table");
    fixedTbl[2].load("Table3.table");
    fixedTbl[3].load("Table4.table");

    vector<string> rawLines;
    vector<vector<Token>> tokLines;
    vector<string> errors;
    ifstream file;
    string filename;
    while(true) {
        cout << "Enter filename: ";
        getline(cin, filename);
        file.open(filename);
        if (!file) {
            cout << "Cannot open file: " << filename << '\n';
        }
        else {
            break;
        }
    }
    string line;
    int lineNo = 5;
    while (getline(file, line)) {
        rawLines.push_back(line);
        tokLines.push_back(lex(line, lineNo, errors));
        lineNo += 5;
    }

    map<string, int> symtab;
    auto lines = pass1(rawLines, tokLines, symtab, errors);
    pass2(lines, symtab, errors);

    cout << left << "Line\tLoc\tSource statement\t\tObject code\n\n";
    ofstream out("mySIC_output.txt");
    out << left << "Line\tLoc\tSource statement\t\tObject code\n\n";
    for (const auto &rec : lines) {
        cout << right << setw(3) << rec.lineNo << '\t';
        out  << right <<setw(3) <<rec.lineNo << '\t';
        if (rec.loc >= 0) {
            stringstream ss;
            ss << uppercase << setw(4) << setfill('0') << hex << rec.loc;
            cout << ss.str();
            out  << ss.str();
        }
        else {
            cout << '\t';
            out << '\t';
        }

        if (rec.isComment) {
            cout << rec.comment;
            out << rec.comment;
        }
        else {
            cout << '\t' << rec.label << '\t' << rec.opcode << '\t' << rec.operand;
            out << '\t' << rec.label << '\t' << rec.opcode << '\t' << rec.operand;
            if (!rec.obj.empty()) {
               if (rec.operand.find(',') != string::npos) {
                    cout << '\t';
                    out << '\t';
               }
               else {
                    cout << "\t\t";
                    out << "\t\t";
               }
                cout <<rec.obj;
                out <<rec.obj;
            }
        }
        cout << '\n';
        out << '\n';
    } // for
     cout << '\n';
     out << '\n';
    if (!errors.empty()) {
        cout << "\nErrors:\n";
        for (const auto &e : errors)
            cout << e << '\n';
    }

    return 0;
}
