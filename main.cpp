#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

path operator""_p(const char* data, std::size_t sz) {
    return path(data, data + sz);
}

bool ProcessFile(istream& input, ostream& output, const path& current_file, const vector<path>& include_directories, int line_number = 1);


bool Preprocess(const path& in_file, const path& out_file, const vector<path>& include_directories) {
    ifstream input(in_file);
    if (!input.is_open()) {
        return false;
    }

    ofstream output(out_file);
    if (!output.is_open()) {
        return false;
    }

    return ProcessFile(input, output, in_file, include_directories);
}

bool ProcessFile(istream& input, ostream& output, const path& current_file, const vector<path>& include_directories, int line_number) {
    static const regex include_local_regex(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static const regex include_system_regex(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    string line;
    while (getline(input, line)){
        smatch match;
        if (regex_match(line, match, include_local_regex) || regex_match(line, match, include_system_regex)) {
            path include_path = string(match[1]);
            ifstream include_file;
            if (regex_match(line, match, include_local_regex)) {
                include_file.open(current_file.parent_path() / include_path);
            }
            if (!include_file.is_open()) {
                for (const auto& dir : include_directories) {
                    include_file.open(dir / include_path);
                    if (include_file.is_open()) break;
                }
            }
            if (!include_file.is_open()) {
                cout << "unknown include file "s << include_path.string() << " at file "s << current_file.string() << " at line "s << line_number << endl;
                return false;
            }
            if (!ProcessFile(include_file, output, current_file.parent_path() / include_path, include_directories, 1)) {
                return false;
            }
        }
        else {
            output << line << endl;
        }
        ++line_number;
    }
    return true;
}

string GetFileContents(string file) {
    ifstream stream(file);

    // конструируем string по двум итераторам
    return { (istreambuf_iterator<char>(stream)), istreambuf_iterator<char>() };
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
            "#include \"dir1/b.h\"\n"
            "// text between b.h and c.h\n"
            "#include \"dir1/d.h\"\n"
            "\n"
            "int SayHello() {\n"
            "    cout << \"hello, world!\" << endl;\n"
            "#   include<dummy.txt>\n"
            "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
            "#include \"subdir/c.h\"\n"
            "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
            "#include <std1.h>\n"
            "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
            "#include \"lib/std2.h\"\n"
            "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert((!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
        { "sources"_p / "include1"_p,"sources"_p / "include2"_p })));

    ostringstream test_out;
    test_out << "// this comment before include\n"
        "// text from b.h before include\n"
        "// text from c.h before include\n"
        "// std1\n"
        "// text from c.h after include\n"
        "// text from b.h after include\n"
        "// text between b.h and c.h\n"
        "// text from d.h before include\n"
        "// std2\n"
        "// text from d.h after include\n"
        "\n"
        "int SayHello() {\n"
        "    cout << \"hello, world!\" << endl;\n"s;

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
