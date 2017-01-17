#include "config-generator.h"

#include <crete/common.h>
#include <crete/test_case.h>

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <assert.h>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace fs = boost::filesystem;

const static map<string, string> target_exe_guest_path = {
        {"ffprobe", "/home/test/tests/ffmpeg-3.1.2/coverage/ffprobe"},
        {"ffmpeg", "/home/test/tests/ffmpeg-3.1.2/coverage/ffmpeg"}
};

// Potential usage:
//      1. check an argument is a path to file or not
//      2. fetch concrete value of symbolic file to make seed test case
const static map<string, string> target_exe_host_path = {
        {"ffprobe", "/home/chenbo/crete/ffmpeg-test/coverage/ffmpeg-3.1.2"},
        {"ffpmpeg", "/home/chenbo/crete/ffmpeg-test/coverage/ffmpeg-3.1.2"}
};

const static string seed_dir_name = "seeds";

const static set<string> coreutil_programs = {
        "base64", "basename", "cat", "cksum", "comm", "cut", "date", "df", "dircolors",
        "dirname", "echo", "env", "expand", "expr", "factor", "fmt", "fold", "head",
        "hostid", "id", "join", "logname", "ls", "nl", "od", "paste", "pathchk", "pinky",
        "printenv", "printf", "pwd", "readlink", "seq", "shuf", "sleep", "sort", "stat",
        "sum", "sync", "tac", "tr", "tsort", "uname", "unexpand", "uptime", "users",
        "wc", "whoami", "who"};

const static string coreutil_guest_path = "/home/test/tests/coreutils-6.10/exec/src/";

struct TestCaseCompare
{
    bool operator()(const crete::TestCase& rhs, const crete::TestCase& lhs);
};

bool TestCaseCompare::operator()(const crete::TestCase &rhs, const crete::TestCase &lhs)
{
    stringstream ssr, ssl;
    rhs.write(ssr);
    lhs.write(ssl);

    return ssr.str() < ssl.str();
}

///////////////////////////////////////////
void CreteTest::add_seed(const string& seed)
{
    m_seeds.insert(seed);
}

uint64_t CreteTest::get_seed_size() const
{
    return m_seeds.size();
}

void CreteTest::print_all_seeds() const
{
    uint64_t count = 0;
    for(set<string>::const_iterator it = m_seeds.begin();
            it != m_seeds.end(); ++it) {
        fprintf(stderr, "[%lu] %s\n",
                count++, it->c_str());
    }
}

void CreteTest::gen_crete_test(bool inject_one_concolic)
{
    gen_config();
    consistency_check();

    if(inject_one_concolic) {
        crete::config::Arguments args = m_crete_config.get_arguments();

        for(crete::config::Arguments::iterator it = args.begin();
                it != args.end(); ++it) {
            if(it->value.find('-') == 0)
                continue;

            crete::config::Argument current_arg = *it;

            // TODO:xxx Impose minimum size to 8 bytes for concolic argument
            current_arg.concolic = true;
            if(current_arg.size < 8)
            {
                current_arg.size = 8;
                current_arg.value.resize(current_arg.size, 0);
            }

            m_crete_config.set_argument(current_arg);
            gen_crete_test_internal();

            // Reset to the original arg
            m_crete_config.set_argument(*it);
        }
    } else {
        crete::config::Arguments args = m_crete_config.get_arguments();

        for(crete::config::Arguments::iterator it = args.begin();
                it != args.end(); ++it) {
            if(it->value.find('-') == 0)
                continue;

            crete::config::Argument current_arg = *it;

            // TODO:xxx Impose minimum size to 8 bytes for concolic argument
            if(current_arg.size < 8)
            {
                current_arg.size = 8;
                current_arg.value.resize(current_arg.size, 0);
            }

            current_arg.concolic = true;
            m_crete_config.set_argument(current_arg);
        }

        gen_crete_test_internal();
    }
}

void CreteTest::set_outputDir(string outputDirectory)
{
    assert(!outputDirectory.empty());
    m_outputDirectory = outputDirectory;
}

void CreteTest::gen_config()
{
    assert(!m_seeds.empty());

    istringstream iss(*(m_seeds.begin()));
    vector<string> test(vector<string>(istream_iterator<string>{iss},
            istream_iterator<string>{}));

//    cerr << "init_config(): \n";
//    for(vector<string>::const_iterator it = test.begin();
//            it != test.end(); ++it) {
//        cerr << *it << ' ';
//    }
//    cerr << endl;

    assert(!test.empty());

    // 1. set executable path "./executable"
    string exec_name = test[0];
    m_target_exec = exec_name;
    assert(target_exe_guest_path.find(exec_name) != target_exe_guest_path.end());
    const string target_path = target_exe_guest_path.find(exec_name)->second;
    m_crete_config.set_executable(target_path);

    // 2. add arguments: skip the executable itself
    for(uint64_t j = 1; j < test.size(); ++j){
        config::Argument arg;
        arg.index = j;

        arg.size = test[j].size();

        arg.value = test[j];
        arg.value.resize(arg.size);

        m_crete_config.add_argument(arg);
    }

    // 3. Impose concrete stdin
    config::STDStream config_stdin;
    config_stdin.concolic = false;
    config_stdin.size = 1;
    config_stdin.value.resize(config_stdin.size, 0);
    m_crete_config.set_stdin(config_stdin);
}

// Check the consistency between m_crete_config and m_seeds
// Also adjust m_crete_config.args size
void CreteTest::consistency_check()
{
    boost::filesystem::path exec_path = m_crete_config.get_executable();
    crete::config::Arguments config_args = m_crete_config.get_arguments();

    assert(m_target_exec == exec_path.filename().string());
    for(set<string>::const_iterator it = m_seeds.begin();
            it != m_seeds.end(); ++it) {
        istringstream iss(*it);
        vector<string> test(vector<string>(istream_iterator<string>{iss},
                istream_iterator<string>{}));
        assert(!test.empty());

        // target executable should match
        if(test[0] != m_target_exec) {
            fprintf(stderr, "seed_exec_name = %s, crete_config_exec_path = %s\n",
                    test[0].c_str(), m_target_exec.c_str());
            assert(0);
        }
        // Configuration does not contain argv[0]
        assert(config_args.size() == (test.size() - 1));

        // All other args: if it starts with "-", they should be the same
        // Otherwise, they can be different
        for(uint64_t i = 0; i < config_args.size(); ++i) {
            assert(!config_args[i].concolic);
            assert(config_args[i].index < test.size());

            string config_arg_value = config_args[i].value;
            string seed_arg_value = test[config_args[i].index];

            if(config_arg_value.find('-') == 0){
                assert(config_arg_value.size() >= seed_arg_value.size());
                seed_arg_value.resize(config_arg_value.size(), 0);
                assert(config_arg_value == seed_arg_value);
            } else {
                assert(seed_arg_value.find('-') != 0);
            }

            assert(config_args[i].size == config_arg_value.size());
            // the size of each arg in config should be the the largest length of
            // the arg in all seeds
            if(config_args[i].size < seed_arg_value.size()) {
                config_args[i].size = seed_arg_value.size();
                config_args[i].value.resize(config_args[i].size, 0);
                m_crete_config.set_argument(config_args[i]);
//                cerr << "arg size is changed\n";
            }
        }
    }

//    cerr << "consistency_check() done\n";
}

static vector<vector<string> > tokenize(const char *filename);

CreteTests::CreteTests(const char *input_file)
{
    initBaseOutputDirectory();

    if(input_file)
        parse_cmdline_tests(input_file);
}

// Input file for command-line tests:
//  1. One test should be defined in one line
//  2. Each test should start with a (full/relative) path to the target binary
void CreteTests::parse_cmdline_tests(const char *input_file)
{
    cerr << "parse_cmdline_tests()\n";
    vector<vector<string> > tokenized = tokenize(input_file);

    set<string> target_executbales;

    for(uint64_t i = 0; i < tokenized.size(); ++i)
    {
        if(tokenized[i].empty())
            continue;

        boost::filesystem::path p_executable(tokenized[i][0]);
        string exe_name = p_executable.filename().string();

        stringstream pattern;
        stringstream all_str;
        pattern << exe_name << ' ';
        all_str << exe_name << ' ';
        for(uint64_t j = 1; j < tokenized[i].size(); ++j) {
            if(tokenized[i][j].find('-') == 0) {
                pattern << tokenized[i][j] << ' ';
            } else {
                pattern << "xxx" << ' ';
            }

            all_str << tokenized[i][j] << ' ';
        }

        if(m_crete_tests.find(pattern.str()) == m_crete_tests.end()) {
            assert(!m_outputDirectory.empty());
            m_crete_tests[pattern.str()] = CreteTest();
        }

        m_crete_tests[pattern.str()].add_seed(all_str.str());
        m_crete_tests[pattern.str()].set_outputDir(m_outputDirectory);
    }

    cerr << "m_crete_tests.size() = " << m_crete_tests.size() << endl;
}

//#define CRETE_DEBUG
void CreteTests::initBaseOutputDirectory()
{
    fs::path cwd = fs::current_path();

    for (int i = 0; ; i++) {
        ostringstream dirName;
        dirName << "crete-config-" << i;

        fs::path dirPath(cwd);
        dirPath /= dirName.str();

        bool exists = fs::exists(dirPath);

        if(!exists) {
            m_outputDirectory = dirPath.string();
            break;
        }
    }

#if defined(CRETE_DEBUG)
    cerr << "Output directory = \" " << m_outputDirectory << "\"\n";
    printf("printf: Output directory = \" %s\".\n", m_outputDirectory.c_str());
#endif // defined(CRETE_DEBUG)

    fs::path outDir(m_outputDirectory);

    if (!fs::create_directories(outDir)) {
#if defined(CRETE_DEBUG)
        cerr << "Could not create output directory " << outDir.string() << '\n';
#endif // defined(CRETE_DEBUG)
        exit(-1);
    }

    if(!fs::exists(outDir)) {
        assert(0);
    }

    fs::path dumpLast("crete-config-last");

    if ((unlink(dumpLast.string().c_str()) < 0) && (errno != ENOENT)) {
        perror("ERROR: Cannot unlink crete-config-last");
        exit(1);
    }

    if (symlink(m_outputDirectory.c_str(), dumpLast.string().c_str()) < 0) {
        perror("ERROR: Cannot make symlink crete-config-last");
        exit(1);
    }
}

fs::path CreteTest::addConfigOutputDir(string dir_name)
{
    assert(!m_outputDirectory.empty());
    assert(!dir_name.empty());

    fs::path outDir(m_outputDirectory);
    outDir /= dir_name;
    if (!fs::create_directories(outDir)) {
        exit(-1);
    }

    if(!fs::exists(outDir)) {
        assert(0);
    }

    fs::path seeds_folder = outDir / seed_dir_name;


    if (!fs::create_directories(seeds_folder)) {
        exit(-1);
    }

    if(!fs::exists(seeds_folder)) {
        assert(0);
    }

    return outDir;
}

void CreteTest::gen_crete_test_internal()
{
    static uint64_t config_count = 0;

    // Create folder
    stringstream ss;
    ss << "auto." << m_target_exec << "." << config_count++;
    fs::path outDir = addConfigOutputDir(ss.str());

    // Generate xml
    ss << ".xml";
    fs::path out_config_file = outDir / ss.str();
    boost::property_tree::ptree pt_config;
    m_crete_config.write_mini(pt_config);
    boost::property_tree::write_xml(
            out_config_file.string(), pt_config, std::locale(),
            boost::property_tree::xml_writer_make_settings<std::string>('\t', 1));

    // Generate serialized guest-config for crete-replay
    ofstream ofs((outDir / "crete-guest-config.serialized").string().c_str());
    boost::archive::text_oarchive oa(ofs);
    oa << m_crete_config;

    // Generate seeds
    gen_crete_test_seeds(outDir / seed_dir_name);
}

// FIXME: xxx Only generate test with concolic arguments
//          (not work with with concolic file and concolic stdin)
void CreteTest::gen_crete_test_seeds(fs::path seeds_folder)
{
    crete::config::Arguments config_args = m_crete_config.get_arguments();

    // Get index within Arguments for concolic args
    vector<uint64_t> set_conoclic_arg_index;
    for(uint64_t i = 0; i < config_args.size(); ++i) {
        if(!config_args[i].concolic)
            continue;

        set_conoclic_arg_index.push_back(i);
    }

    // Generate a seed test case based on each m_seeds
    set<crete::TestCase, TestCaseCompare> seed_set;
    for(set<string>::const_iterator it = m_seeds.begin();
            it != m_seeds.end(); ++it) {
        istringstream iss(*it);
        vector<string> test(vector<string>(istream_iterator<string>{iss},
                istream_iterator<string>{}));
        assert(!test.empty());

        crete::TestCase tc;

        for(uint64_t i = 0; i < set_conoclic_arg_index.size(); ++i) {
            crete::config::Argument concolic_arg =
                    config_args[set_conoclic_arg_index[i]];
            string seed_arg_value = test[concolic_arg.index];

            crete::TestCaseElement elem;

            std::stringstream ss_name;
            ss_name << "argv_" << concolic_arg.index;
            string name = ss_name.str();
            elem.name = std::vector<uint8_t>(name.begin(), name.end());
            elem.name_size = name.size();

            elem.data = std::vector<uint8_t>(seed_arg_value.begin(),
                    seed_arg_value.end());
            if(elem.data.size() < concolic_arg.size) {
                elem.data.resize(concolic_arg.size, 0);
            }

            elem.data_size = elem.data.size();

            tc.add_element(elem);
        }

        seed_set.insert(tc);
    }

    uint64_t seed_count = 1;
    for(set<crete::TestCase>::iterator it = seed_set.begin();
            it != seed_set.end(); ++it) {
        stringstream current_seed_path;
        current_seed_path << seeds_folder.string() << "/" << seed_count++;
        ofstream out(current_seed_path.str());
        it->write(out);
    }
}

void CreteTests::gen_crete_tests(bool inject_one_concolic)
{
    for(map<string, CreteTest>::iterator it = m_crete_tests.begin();
            it != m_crete_tests.end(); ++it) {
        it->second.gen_crete_test(inject_one_concolic);
    }
}


struct SymArgsConfig
{
    uint64_t m_min_sym_num;
    uint64_t m_max_sym_num;
    uint64_t m_sym_size;

    SymArgsConfig(uint64_t min, uint64_t max, uint64_t size)
    :m_min_sym_num(min), m_max_sym_num(max), m_sym_size(size) {}

    ~SymArgsConfig(){};

    void print(const string& name) const
    {
        fprintf(stderr, "%s: m_min_sym_num = %lu, m_max_sym_num =%lu, m_sym_size = %lu\n",
                name.c_str(), m_min_sym_num, m_max_sym_num, m_sym_size);
    }
};

const SymArgsConfig KLEE_SYM_ARGS_CONFIG_1(0, 1, 10);
const SymArgsConfig KLEE_SYM_ARGS_CONFIG_2(0, 2, 2);
const uint64_t KLEE_SYM_FILE_SIZE  = 8;
const uint64_t KLEE_SYM_STDIN_SIZE = 8;

struct ParsedSymArgs
{
    vector<uint64_t> m_sym_args;
    uint64_t m_sym_file_size;
    uint64_t m_sym_stdin_size;

    ParsedSymArgs(): m_sym_args(vector<uint64_t>()),
            m_sym_file_size(KLEE_SYM_FILE_SIZE),
            m_sym_stdin_size(KLEE_SYM_STDIN_SIZE) {}
};

// Rules: sym-args from config1 always come before the ones from config2
static vector<ParsedSymArgs> parse_symArgsConfig(const SymArgsConfig& config1,
        const SymArgsConfig& config2)
{
//    config1.print("config1");
//    config2.print("config2");

    vector<ParsedSymArgs> ret;
    for(uint64_t count1 = config1.m_min_sym_num;
            count1 <= config1.m_max_sym_num; ++count1)
    {
        for(uint64_t count2 = config2.m_min_sym_num;
                count2 <= config2.m_max_sym_num; ++count2)
        {
            ParsedSymArgs parsed;

            for(uint64_t i = 0; i < count1; ++i)
            {
                parsed.m_sym_args.push_back(config1.m_sym_size);
            }

            for(uint64_t i = 0; i < count2; ++i)
            {
                parsed.m_sym_args.push_back(config2.m_sym_size);
            }

            if(!parsed.m_sym_args.empty())
            {
                ret.push_back(parsed);
            }
        }
    }

    return ret;
}

static void printf_parsed_config(const vector<ParsedSymArgs>& configs)
{
    uint64_t count = 1;
    for(vector<ParsedSymArgs>::const_iterator it = configs.begin();
            it != configs.end(); ++it) {
        fprintf(stderr, "config-%lu: sym-args [", count++);
        for(uint64_t i = 0; i < it->m_sym_args.size(); ++i)
        {
            fprintf(stderr, "%lu ", it->m_sym_args[i]);
        }
        fprintf(stderr, "], sym-file [%lu], sym-stdin[%lu]\n",
                it->m_sym_file_size, it->m_sym_stdin_size);
    }
}

void CreteTests::gen_crete_tests_coreutils()
{
    vector<ParsedSymArgs> parsed_configs = parse_symArgsConfig(KLEE_SYM_ARGS_CONFIG_1, KLEE_SYM_ARGS_CONFIG_2);
    printf_parsed_config(parsed_configs);

    uint64_t config_count = 1;
    // Generate all configs for each gnu coreutil progs
    for(set<string>::const_iterator it = coreutil_programs.begin();
            it != coreutil_programs.end(); ++ it) {
        for(uint64_t i = 0; i < parsed_configs.size(); ++i)
        {
            const ParsedSymArgs& parsed_config =  parsed_configs[i];
            config::RunConfiguration crete_config;
            crete_config.set_executable(coreutil_guest_path + *it);

            // 1. sym-args
            const vector<uint64_t>& sym_args = parsed_config.m_sym_args;
            for(uint64_t j = 0; j < sym_args.size(); ++j)
            {
                config::Argument arg;

                arg.index = j + 1; // Offset by one b/c of argv[0]
                arg.size = sym_args[j];
                arg.value.resize(arg.size, 0);
                arg.concolic = true;
                crete_config.add_argument(arg);
            }

            // 2. sym-stdin
            config::STDStream config_stdin;
            config_stdin.size = parsed_config.m_sym_stdin_size;
            config_stdin.value.resize(config_stdin.size, 0);
            config_stdin.concolic = true;
            crete_config.set_stdin(config_stdin);

            // 3. sym-file
            // 3.1 w/o sym-file
            {
                stringstream config_name;
                config_name << "auto." << *it << "."  << config_count++  << ".xml" ;
                fs::path out_config_file = fs::path(m_outputDirectory) / config_name.str();
                cerr << out_config_file.string() << endl;

                boost::property_tree::ptree pt_config;
                crete_config.write_mini(pt_config);
                boost::property_tree::write_xml(
                        out_config_file.string(), pt_config, std::locale(),
                        boost::property_tree::xml_writer_make_settings<std::string>('\t', 1));

            }

            // 3.2 with sym-file and the last arg is the name of symfile
            {
                config::File config_file;
                config_file.path = fs::path(CRETE_RAMDISK_PATH) / "input.data";
                config_file.size = parsed_config.m_sym_file_size;
                config_file.concolic = true;
                crete_config.add_file(config_file);

                const crete::config::Arguments config_args = crete_config.get_arguments();
                config::Argument arg;
                arg.index = config_args.back().index;
                arg.value = config_file.path.string();
                arg.size = arg.value.size();
                arg.concolic = false;
                crete_config.set_argument(arg);

                stringstream config_name;
                config_name << "auto." << *it << "."  << config_count++  << ".xml" ;
                fs::path out_config_file = fs::path(m_outputDirectory) / config_name.str();
                cerr << out_config_file.string() << endl;

                boost::property_tree::ptree pt_config;
                crete_config.write_mini(pt_config);
                boost::property_tree::write_xml(
                        out_config_file.string(), pt_config, std::locale(),
                        boost::property_tree::xml_writer_make_settings<std::string>('\t', 1));
            }
        }
    }

    return;
}

///////////////////////////////////////////

// Divide file line by line and tokenize each file into
// string by spaces
vector<vector<string> > tokenize(const char *filename) {
    ifstream ifs(filename);

    if(!ifs.good()){
        fprintf(stderr, "invalid input file %s.\n",
                filename);
        exit(-1);
    }

    vector<string> all_lines;

    string line;
    while(getline(ifs, line)){
        all_lines.push_back(line);
    }

    vector<vector<string> > tokenized;
    for(uint64_t i = 0; i < all_lines.size(); ++i){
        istringstream iss(all_lines[i]);
        tokenized.push_back(vector<string>(istream_iterator<string>{iss},
                istream_iterator<string>{}));
    }

    return tokenized;
}

int main (int argc, char **argv)
{
    try
    {
        // TODO: xxx add more generic way of parsing inputs
        if (argc == 1) {
            CreteTests crete_tests(NULL);
            crete_tests.gen_crete_tests_coreutils();
        } else if(argc == 2){
            CreteTests crete_tests(argv[1]);
            crete_tests.gen_crete_tests(true);
        } else if(argc == 3){
            fs::path input(argv[2]);
            config::RunConfiguration crete_config(input);
        } else {
            fprintf(stderr, "invalid inputs.\n");
            return -1;
        }
    }
    catch(...)
    {
        cerr << "[CRETE Config Generator] Exception Info: \n"
                << boost::current_exception_diagnostic_information() << endl;
        return -1;
    }

    return 0;
}
