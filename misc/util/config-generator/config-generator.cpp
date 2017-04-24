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
#include <map>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

namespace fs = boost::filesystem;

const static map<string, string> target_exe_guest_path = {
        {"ffprobe", "/home/test/tests/ffmpeg-3.1.2/ffprobe"},
        {"ffmpeg", "/home/test/tests/ffmpeg-3.1.2/ffmpeg"},
        {"GenFfs", "/home/test/tests/build/bin/GenFfs"},
        {"GenFv","/home/test/tests/build/bin/GenFv"},
        {"GenFw", "/home/test/tests/build/bin/GenFw"},
        {"GenSec", "/home/test/tests/build/bin/GenSec"},
        {"LzmaCompress", "/home/test/tests/build/bin/LzmaCompress"},
        {"VfrCompile", "/home/test/tests/build/bin/VfrCompile"}
};

// Potential usage:
//      1. check an argument is a path to file or not
//      2. fetch concrete value of symbolic file to make seed test case
const static map<string, string> target_exe_host_path = {
        {"ffprobe", "/home/chenbo/crete/ffmpeg-test/coverage/ffmpeg-3.1.2"},
        {"ffpmpeg", "/home/chenbo/crete/ffmpeg-test/coverage/ffmpeg-3.1.2"},
        {"GenFfs", "/home/chenbo/crete/replay-evals/edk2/BaseTools/Source/C/bin/GenFfs"},
        {"GenFv", "/home/chenbo/crete/replay-evals/edk2/BaseTools/Source/C/bin/GenFv"},
        {"GenFw", "/home/chenbo/crete/replay-evals/edk2/BaseTools/Source/C/bin/GenFw"},
        {"GenSec", "/home/chenbo/crete/replay-evals/edk2/BaseTools/Source/C/bin/GenSec"},
        {"LzmaCompress", "/home/chenbo/crete/replay-evals/edk2/BaseTools/Source/C/bin/LzmaCompress"},
        {"VfrCompile", "/home/chenbo/crete/replay-evals/edk2/BaseTools/Source/C/bin/VfrCompile"}
};

const static vector<string> ffmpeg_file_prefix = {
        {"tests/"},
        {"fate-suite/"},
        {"/home/chenbo/crete/replay-evals/edk2/Build/"}
};

const static string seed_dir_name = "seeds";

const static set<string> coreutil_crete_old = {
        "base64", "basename", "cat", "cksum", "comm", "cut", "date", "df", "dircolors",
        "dirname", "echo", "env", "expand", "expr", "factor", "fmt", "fold", "head",
        "hostid", "id", "join", "logname", "ls", "nl", "od", "paste", "pathchk", "pinky",
        "printenv", "printf", "pwd", "readlink", "seq", "shuf", "sleep", "sort", "stat",
        "sum", "sync", "tac", "tr", "tsort", "uname", "unexpand", "uptime", "users",
        "wc", "whoami", "who"};

const static set<string> coreutil_klee_osdi_new = {
//"base64", "basename", "cat",
"chcon", "chgrp", "chmod", "chown", "chroot", //"cksum", "comm",
"cp", "csplit", //"cut", "date",
"dd", //"df", "dircolors", "dirname",
"du", //"echo", "env", "expand", "expr", "factor",
"false", //"fmt", "fold", "head", "hostid",
"hostname", //"id",
"ginstall", //"join",
"kill", "link", "ln", //"logname", "ls",
"md5sum", "mkdir", "mkfifo", "mknod", "mktemp", "mv", "nice", //"nl",
"nohup", //"od", "paste", "pathchk", "pinky",
"pr", //"printenv", "printf",
"ptx", //"pwd", "readlink",
"rm", "rmdir", "runcon", //"seq",
"setuidgid", "shred", //"shuf", "sleep", "sort",
"split", //"stat",
"stty", //"sum", "sync", "tac",
"tail", "tee", "touch", //"tr", "tsort",
"tty", //"uname", "unexpand",
"uniq", "unlink", //"uptime", "users", "wc", "whoami", "who","yes"
};

const static set<string> coreutil_klee_osdi = {
        "base64", "basename", "cat", "chcon", "chgrp", "chmod", "chown", "chroot", "cksum", "comm", "cp", "csplit", "cut",
        "date", "dd", "df", "dircolors", "dirname", "du", "echo", "env", "expand", "expr", "factor", "false", "fmt", "fold",
        "head", "hostid", "hostname", "id", "ginstall", "join", "kill", "link", "ln", "logname", "ls", "md5sum", "mkdir",
        "mkfifo", "mknod", "mktemp", "mv", "nice", "nl", "nohup", "od", "paste", "pathchk", "pinky", "pr", "printenv", "printf",
        "ptx", "pwd", "readlink", "rm", "rmdir", "runcon", "seq", "setuidgid", "shred", "shuf", "sleep", "sort", "split",
        "stat", "stty", "sum", "sync", "tac", "tail", "tee", "touch", "tr", "tsort", "tty", "uname", "unexpand", "uniq", "unlink",
        "uptime", "users", "wc", "whoami", "who", "yes"};

const static string coreutil_guest_path = "/home/test/tests/coreutils-6.10/exec/src/";

const static string guest_executable_folder =  "/home/test/tests/build/bin/";

const static set<string> dase_expr = {
        // diff
        "cmp", "diff", "diff3", "sdiff",
        // grep
        "egrep", "fgrep", "grep"
};


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
static bool match_ffmpeg_file_prefix(const string& input)
{
    for(uint64_t i = 0; i < ffmpeg_file_prefix.size(); ++ i)
    {
        if(input.find(ffmpeg_file_prefix[i]) == 0)
        {
            return true;
        }
    }

    return false;
}

static void split_seed_into_args_and_files(const string& seed_string,
        vector<string>& args, vector<string>& files)
{
    istringstream iss(seed_string);
    vector<string> seed(vector<string>(istream_iterator<string>{iss},
            istream_iterator<string>{}));
    assert(!seed.empty());

    for(uint64_t i = 0; i < seed.size(); ++ i)
    {
        if(match_ffmpeg_file_prefix(seed[i]))
        {
            files.push_back(seed[i]);
        }

        args.push_back(seed[i]);
    }
}

void CreteTest::add_seed(const string& seed)
{
    if(m_seeds_map.find(seed) == m_seeds_map.end())
    {
        assert(m_seeds_map.size() == m_seeds_args.size());
        assert(m_seeds_map.size() == m_seeds_files.size());

        vector<string> args;
        vector<string> files;
        split_seed_into_args_and_files(seed, args, files);
        m_seeds_args.push_back(args);
        m_seeds_files.push_back(files);

        m_seeds_map.insert(make_pair(seed, m_seeds_map.size()));
    }
}

uint64_t CreteTest::get_seed_size() const
{
    return m_seeds_map.size();
}

void CreteTest::print_all_seeds() const
{
    assert(m_seeds_map.size() == m_seeds_args.size());
    assert(m_seeds_map.size() == m_seeds_files.size());

    for(map<string, uint32_t>::const_iterator it = m_seeds_map.begin();
            it != m_seeds_map.end(); ++it)
    {
        fprintf(stderr, "[%u] %s\n", it->second, it->first.c_str());
    }
}

bool CreteTest::gen_crete_test(bool inject_one_concolic)
{
    bool contains_concolic_element = false;

    gen_config();
    consistency_check();
//    print_all_seeds();

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

            contains_concolic_element = true;
        }
    } else {
        crete::config::Arguments args = m_crete_config.get_arguments();

        /*
        for(crete::config::Arguments::iterator it = args.begin();
                it != args.end(); ++it) {
            // make all arguments as concolic, except:
            // 1. start with "-" (functionality specifier)
            // 2. start with or contain "/" (path)
            if(it->value.find('-') == 0 || it->value.find('/') != string::npos)
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

            contains_concolic_element = true;
        }
        */

        const crete::config::Files& files = m_crete_config.get_files();
        crete::config::Files concolic_files;

        // Make all files as concolic and adjust args for concolic files
        for(uint64_t i = 0; i < files.size(); ++i)
        {
            crete::config::File concolic_file = files[i];
            concolic_file.concolic = true;
            concolic_file.path = CRETE_RAMDISK_PATH / files[i].path.filename();
            concolic_files.push_back(concolic_file);

            crete::config::Argument arg;
            assert(concolic_file.argv_index <= m_crete_config.get_arguments().size());
            assert(concolic_file.argv_index != 0);

            arg.concolic = false;
            arg.index = concolic_file.argv_index;
            arg.value = concolic_file.path.string();
            arg.size = arg.value.size();
            m_crete_config.set_argument(arg);

            contains_concolic_element = true;
        }
        m_crete_config.set_files(concolic_files);

        if(contains_concolic_element)
            gen_crete_test_internal();
    }

    return contains_concolic_element;
}

void CreteTest::set_outputDir(string outputDirectory)
{
    assert(!outputDirectory.empty());
    m_outputDirectory = outputDirectory;
}

uint64_t CreteTest::get_max_seed_arg_size(uint64_t index) const
{
    uint64_t ret = 0;
    for(uint64_t i = 0; i < m_seeds_args.size(); ++i)
    {
        assert(index < m_seeds_args[i].size());
        if(ret < m_seeds_args[i][index].size())
        {
            ret = m_seeds_args[i][index].size();
        }
    }

    return ret;
}

uint64_t CreteTest::get_max_seed_file_size(uint64_t index) const
{
    uint64_t ret = 0;
    for(uint64_t i = 0; i < m_seeds_files.size(); ++i)
    {
        assert(index < m_seeds_files[i].size());
        string file_path = m_seeds_files[i][index];
        assert(match_ffmpeg_file_prefix(file_path));

        assert(target_exe_host_path.find(m_target_exec) != target_exe_host_path.end());

        fs::path full_path;
        if(file_path.find("/") == 0)
        {
            full_path = file_path;
        } else {
            full_path = fs::path(target_exe_host_path.find(m_target_exec)->second) / file_path;
        }

        if(fs::is_regular(full_path))
        {
            uint64_t size = fs::file_size(full_path);
            if(ret < size)
            {
                ret = size;
            }
        } else {
            cerr << "get_max_seed_file_size(): file does not exist " << file_path << endl;
        }
    }

    return ret;
}

void CreteTest::gen_config()
{
    assert(!m_seeds_map.empty());
    assert(m_seeds_map.size() == m_seeds_args.size());
    assert(m_seeds_map.size() == m_seeds_files.size());

    const vector<string>& args = m_seeds_args.front();
    const vector<string>& files = m_seeds_files.front();

//    cerr << "gen_config(): \n";
//    for(vector<string>::const_iterator it = args.begin();
//            it != args.end(); ++it) {
//        cerr << *it << ' ';
//    }
//    for(vector<string>::const_iterator it = files.begin();
//            it != files.end(); ++it) {
//        cerr << *it << ' ';
//    }
//    cerr << endl;


    // 1. set executable path "./executable"
    string exec_name = args[0];
    m_target_exec = exec_name;
    assert(target_exe_guest_path.find(exec_name) != target_exe_guest_path.end());
    const string target_path = target_exe_guest_path.find(exec_name)->second;
    m_crete_config.set_executable(target_path);

    // 2. add arguments and file: skip the executable itself
    for(uint64_t j = 1, file_index = 0; j < args.size(); ++j){
        config::Argument arg;
        arg.index = j;
        arg.size = args[j].size();
        arg.value = args[j];
        assert(arg.value.size() == arg.size);
        arg.concolic = false;

        // Make the argument size be the maximum size of seeds
        assert(arg.size <= get_max_seed_arg_size(j));
        arg.size = get_max_seed_arg_size(j);
        arg.value.resize(arg.size, 0);

        m_crete_config.add_argument(arg);

        if(match_ffmpeg_file_prefix(args[j]))
        {
            assert(file_index < files.size());
            assert(arg.value.find(files[file_index]) != string::npos);

            config::File config_file;

            config_file.path = files[file_index];
            config_file.concolic = false;

            config_file.size = get_max_seed_file_size(file_index);

            //XXX: impose minimum concolic file size 8 bytes
            if(config_file.size < 8)
            {
                config_file.size = 8;
            }
            config_file.data.clear();

            config_file.argv_index = arg.index;

            m_crete_config.add_file(config_file);

            ++file_index;
        }
    }

    // 3. Impose concrete stdin
    config::STDStream config_stdin;
    config_stdin.concolic = false;
    config_stdin.size = 1;
    config_stdin.value.resize(config_stdin.size, 0);
    m_crete_config.set_stdin(config_stdin);
}

// Check the consistency between m_crete_config and m_seeds
void CreteTest::consistency_check() const
{
    // check executable
    const boost::filesystem::path& exec_path = m_crete_config.get_executable();
    assert(m_target_exec == exec_path.filename().string());

    // Check args
    const crete::config::Arguments& config_args = m_crete_config.get_arguments();
    for(vector<vector<string> >::const_iterator it = m_seeds_args.begin();
            it != m_seeds_args.end(); ++it) {
        const vector<string>& test = *it;
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
            assert(config_args[i].size == config_arg_value.size());
            string seed_arg_value = test[config_args[i].index];

            if(config_arg_value.find('-') == 0){
                assert(config_arg_value.size() >= seed_arg_value.size());
                seed_arg_value.resize(config_arg_value.size(), 0);
                assert(config_arg_value == seed_arg_value);
            } else {
                assert(seed_arg_value.find('-') != 0);
                assert(config_arg_value.size() >= seed_arg_value.size());
            }
        }
    }

    // check files
//    const crete::config::Arguments& config_files = m_crete_config.get_files();
//    for(vector<vector<string> > it = m_seeds_files.begin();
//            it != m_seeds_files.end(); ++it) {
//        vector<string> test = *it;
//        assert(config_files.size() == test.size());

//        for(uint64_t i = 0; i < config_files.size(); ++i)
//        {
//            ;
//        }
//    }

//    cerr << "consistency_check() done\n";
}

static vector<vector<string> > tokenize(const char *filename);

CreteTests::CreteTests(const char *input_file)
:m_config_count(0)
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
            if(tokenized[i][j].find('-') == 0 || tokenized[i][j].find('/') != string::npos) {
                pattern << tokenized[i][j] << ' ';
            } else {
//                pattern << "xxx" << ' ';
                pattern << tokenized[i][j] << ' ';
            }

            all_str << tokenized[i][j] << ' ';
        }

        if(m_crete_tests.find(pattern.str()) == m_crete_tests.end()) {
            assert(!m_outputDirectory.empty());
            m_crete_tests[pattern.str()] = CreteTest();
        } else {
            fprintf(stderr, "duplicated pattern: %s\n", pattern.str().c_str());
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

//    cerr << ss.str() << endl;

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

// Naming convention on concolic buffer should stay consistent with run_preload.cpp
void CreteTest::gen_crete_test_seeds(fs::path seeds_folder) const
{
    assert(m_seeds_map.size() == m_seeds_args.size());
    assert(m_seeds_map.size() == m_seeds_files.size());

    crete::config::Arguments config_args = m_crete_config.get_arguments();
    // Get index within Arguments for concolic args
    vector<uint64_t> set_conoclic_arg_index;
    for(uint64_t i = 0; i < config_args.size(); ++i) {
        if(!config_args[i].concolic)
            continue;

        set_conoclic_arg_index.push_back(i);
    }

    // Get index within Files for concolic files
    crete::config::Files config_files= m_crete_config.get_files();
    vector<uint64_t> set_conoclic_file_index;
    for(uint64_t i = 0; i < config_files.size(); ++i) {
        if(!config_files[i].concolic)
            continue;

        set_conoclic_file_index.push_back(i);
    }

    // Generate a seed test case based on each m_seeds
    set<crete::TestCase, TestCaseCompare> seed_set;
    for(uint64_t seed_index = 0; seed_index < m_seeds_map.size(); ++seed_index)
    {
        crete::TestCase tc;

        // Concolic Args
        const vector<string>& args  = m_seeds_args[seed_index];
        for(uint64_t i = 0; i < set_conoclic_arg_index.size(); ++i) {
            const crete::config::Argument& concolic_arg =
                    config_args[set_conoclic_arg_index[i]];
            const string& seed_arg_value = args[concolic_arg.index];

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

        // Symbolic file
        const vector<string>& files = m_seeds_files[seed_index];
        for(uint64_t i = 0; i < set_conoclic_file_index.size(); ++i) {
            crete::config::File concolic_file =
                    config_files[set_conoclic_file_index[i]];
//            assert(files[i] == concolic_file.path.string());

            crete::TestCaseElement elem;

            string filename = concolic_file.path.filename().string();
            elem.name = std::vector<uint8_t>(filename.begin(), filename.end());
            elem.name_size = filename.size();

            string file_path = files[i];
            assert(match_ffmpeg_file_prefix(file_path));
            fs::path full_path;
            if(file_path.find("/") == 0)
            {
                full_path = file_path;
            } else {
                assert(target_exe_host_path.find(m_target_exec) != target_exe_host_path.end());
                full_path = fs::path(target_exe_host_path.find(m_target_exec)->second) / file_path;
            }
            if(fs::is_regular(full_path))
            {
                uint64_t fileSize = fs::file_size(full_path);
                assert(concolic_file.size >= fileSize );

                std::ifstream concrete_file(full_path.string(), std::ios::binary);
                elem.data.clear();
                elem.data.reserve(fileSize);
                elem.data.assign(std::istreambuf_iterator<char>(concrete_file),
                                    std::istreambuf_iterator<char>());
                concrete_file.close();
            } else {
                elem.data.clear();
                elem.data.resize(concolic_file.size, 0);
                cerr << "gen_crete_test_seeds(): file does not exist " << file_path
                        << ", prefix = " << target_exe_host_path.find(m_target_exec)->second << endl;
            }

            if(elem.data.size() < concolic_file.size) {
                elem.data.resize(concolic_file.size, 0);
            }
            elem.data_size = elem.data.size();

            tc.add_element(elem);
        }

        seed_set.insert(tc);
    }

//    fprintf(stderr, "gen_crete_test_seeds(): m_seeds_map.size() = %lu, seed_set.size() = %lu\n",
//            m_seeds_map.size(), seed_set.size());

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
        bool ret = it->second.gen_crete_test(inject_one_concolic);

        if(!ret)
        {
            fprintf(stderr, "Not generate crete-config for current pattern: no concolic elements!\n"
                    "pattern: %s\n", it->first.c_str());
        }
    }
}


struct SymArgsConfig
{
    uint64_t m_min_sym_num;
    uint64_t m_max_sym_num;
    uint64_t m_sym_size;

    SymArgsConfig(uint64_t min, uint64_t max, uint64_t size)
    :m_min_sym_num(min), m_max_sym_num(max), m_sym_size(size) {}

    SymArgsConfig()
    :m_min_sym_num(0), m_max_sym_num(0), m_sym_size(0) {}

    ~SymArgsConfig(){};

    void print(const string& name) const
    {
        fprintf(stderr, "%s: m_min_sym_num = %lu, m_max_sym_num =%lu, m_sym_size = %lu\n",
                name.c_str(), m_min_sym_num, m_max_sym_num, m_sym_size);
    }
};

typedef vector<uint64_t> parsedSymArgs_ty;

struct ParsedSymArgs
{
    vector<uint64_t> m_sym_args;
    vector<uint64_t> m_sym_file_size;
    uint64_t m_sym_stdin_size;

    ParsedSymArgs(vector<uint64_t> sym_file_size, uint64_t sym_stdin_size): m_sym_args(vector<uint64_t>()),
            m_sym_file_size(sym_file_size),
            m_sym_stdin_size(sym_stdin_size) {}
};

class ExprSetup
{
private:
    vector<SymArgsConfig> m_sym_args_configs;
    vector<uint64_t> m_sym_file_size;
    uint64_t m_sym_std_size;

public:
    ExprSetup(SymArgsConfig sym_args_config_1, SymArgsConfig sym_args_config_2,
            vector<uint64_t> sym_file_size, uint64_t sym_std_size)
    :m_sym_args_configs(vector<SymArgsConfig>()),
     m_sym_file_size(sym_file_size), m_sym_std_size(sym_std_size)
    {
        m_sym_args_configs.push_back(sym_args_config_1);
        m_sym_args_configs.push_back(sym_args_config_2);
    }

    ExprSetup(SymArgsConfig sym_args_config_1, vector<uint64_t> sym_file_size, uint64_t sym_std_size)
    :m_sym_args_configs(vector<SymArgsConfig>()),
     m_sym_file_size(sym_file_size), m_sym_std_size(sym_std_size)
    {
        m_sym_args_configs.push_back(sym_args_config_1);
    }

    ~ExprSetup() {};

    // Rules: sym-args from config1 always come before the ones from config2
    vector<ParsedSymArgs> get_ParsedSymArgs() const;

private:
};

static set<parsedSymArgs_ty> get_ParsedSymArgs_internal(const vector<SymArgsConfig>& not_parsed)
{
    if(not_parsed.empty())
    {
        return set<parsedSymArgs_ty>();
    }

    set<parsedSymArgs_ty> parsed_configs = get_ParsedSymArgs_internal(vector<SymArgsConfig>(not_parsed.begin(), not_parsed.end() - 1));

    const SymArgsConfig& last_config = not_parsed.back();
    set<parsedSymArgs_ty> parsed_last_config;

    for(uint64_t i = last_config.m_min_sym_num; i <= last_config.m_max_sym_num; ++i)
    {
        parsed_last_config.insert(parsedSymArgs_ty(i, last_config.m_sym_size));
    }

    if (parsed_last_config.empty())
        return parsed_configs;
    else if(parsed_configs.empty())
        return parsed_last_config;

    assert(!parsed_last_config.empty() && !parsed_configs.empty());

    set<parsedSymArgs_ty> ret;
    for(set<parsedSymArgs_ty>::const_iterator out_it = parsed_configs.begin();
            out_it != parsed_configs.end(); ++out_it) {
        for(set<parsedSymArgs_ty>::const_iterator in_it = parsed_last_config.begin();
                in_it != parsed_last_config.end(); ++in_it) {
            parsedSymArgs_ty temp;
            temp.reserve(out_it->size() + in_it->size());
            temp.insert(temp.end(), out_it->begin(), out_it->end());
            temp.insert(temp.end(), in_it->begin(), in_it->end());
            ret.insert(temp);
        }
    }

    return ret;
}

vector<ParsedSymArgs> ExprSetup::get_ParsedSymArgs() const
{
    set<parsedSymArgs_ty> parsedSymArgs = get_ParsedSymArgs_internal(m_sym_args_configs);
    vector<ParsedSymArgs> ret;

    ParsedSymArgs temp(m_sym_file_size, m_sym_std_size);
    for(set<parsedSymArgs_ty>::const_iterator it = parsedSymArgs.begin();
            it != parsedSymArgs.end(); ++it)
    {
        if(it->empty()) continue;

        temp.m_sym_args = vector<uint64_t>(it->begin(), it->end());
        ret.push_back(temp);
    }

    return ret;
}

// === klee setup ===
// normal: --sym-args 0 1 10 --sym-args 0 2 2 --sym-files 1 8 --sym-stdin 8 --sym-stdout
const ExprSetup KLEE_SETUP_NORMAL(
        SymArgsConfig(0, 1, 10),
        SymArgsConfig(0, 2, 2),
        vector<uint64_t>(1, 8),
        8);

// mknod: --sym-args 0 1 10 --sym-args 0 3 2 --sym-files 1 8 --sym-stdin 8 --sym-stdout
const ExprSetup KLEE_SETUP_MKNOD(
        SymArgsConfig(0, 1, 10),
        SymArgsConfig(0, 3, 2),
        vector<uint64_t>(1, 8),
        8);

// pathchk: --sym-args 0 1 2 --sym-args 0 1 300 --sym-files 1 8 --sym-stdin 8 --sym-stdout
const ExprSetup KLEE_SETUP_PATHCHK(
        SymArgsConfig(0, 1, 2),
        SymArgsConfig(0, 1, 300),
        vector<uint64_t>(1, 8),
        8);

// dd: --sym-args 0 3 10 --sym-files 1 8 --sym-stdin 8 --sym-stdout
const ExprSetup KLEE_SETUP_DD(
        SymArgsConfig(0, 3, 10),
        vector<uint64_t>(1, 8),
        8);

// expr: --sym-args 0 1 10 --sym-args 0 3 2 --sym-stdout
const ExprSetup KLEE_SETUP_EXPR(
        SymArgsConfig(0, 1, 10),
        SymArgsConfig(0, 3, 2),
        vector<uint64_t>(),
        8);

// dircolors: --sym-args 0 3 10 --sym-files 2 12 --sym-stdin 12 --sym-stdout
const ExprSetup KLEE_SETUP_DIRCOLORS(
        SymArgsConfig(0, 3, 10),
        vector<uint64_t>(2, 12),
        12);

// echo: --sym-args 0 4 300 --sym-files 2 30 --sym-stdin 30 --sym-stdout
const ExprSetup KLEE_SETUP_ECHO(
        SymArgsConfig(0, 4, 300),
        vector<uint64_t>(2, 30),
        30);

// od: --sym-args 0 3 10 --sym-files 2 12 --sym-stdin 12 --sym-stdout
const ExprSetup KLEE_SETUP_OD(
        SymArgsConfig(0, 3, 10),
        vector<uint64_t>(2, 12),
        12);

// printf: --sym-args 0 3 10 --sym-files 2 12 --sym-stdin 12 --sym-stdout
const ExprSetup KLEE_SETUP_PRINTF(
        SymArgsConfig(0, 3, 10),
        vector<uint64_t>(2, 12),
        12);

const map<string, const ExprSetup> coreutil_klee_osdi_special {
        {"dd", KLEE_SETUP_DD},
        {"dircolors", KLEE_SETUP_DIRCOLORS},
        {"echo", KLEE_SETUP_ECHO},
        {"expr", KLEE_SETUP_EXPR},
        {"mknod",KLEE_SETUP_MKNOD},
        {"od", KLEE_SETUP_OD},
        {"pathchk", KLEE_SETUP_PATHCHK},
        {"printf", KLEE_SETUP_PRINTF}
};

// ========== basetool setup ===============
const static set<string> baseTools = {
//        "BootSectImage",
        "EfiRom", "GenFfs", "GenFw", "GenSec", "GnuGenBootSector",
        "Split", "VfrCompile", "EfiLdrImage", "GenCrc32", "GenFv", "GenPage", "GenVtf",
//        "LzmaCompress",
        "TianoCompress",
        "VolInfo"
        };

// ************ Special setup **************
// BootSectImage: --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 2 10k
const ExprSetup BASEtOOL_SETUP_BootSectImage(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(2, 10240),
        8);

// EfiLdrImage: --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 2 10k
const ExprSetup BASEtOOL_SETUP_EfiLdrImage(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(2, 10240),
        8);

// ************ normal setup **************
// --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 1 10k
// EfiRom: --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 1 10k
const ExprSetup BASEtOOL_SETUP_EfiRom(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(1, 10240),
        8);

// GenCrc32: --sym-args 0 1 16  --sym-args 0 4 2 -sym-files 1 10k
const ExprSetup BASEtOOL_SETUP_GenCrc32(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(1, 10240),
        8);

// GenFfs:  --sym-args 0 1 16  --sym-args 0 4 2 -sym-files 1 10k
const ExprSetup BASEtOOL_SETUP_GenFfs(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(1, 10240),
        8);
// GenFv: --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 1 10k
const ExprSetup BASEtOOL_SETUP_GenFv(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(1, 10240),
        8);
// GenFw:  --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 1 10k
const ExprSetup BASEtOOL_SETUP_GenFw(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(1, 10240),
        8);
// GenPage:  --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 1 10k
const ExprSetup BASEtOOL_SETUP_GenPage(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(1, 10240),
        8);
// GenSec: --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 1 10k
const ExprSetup BASEtOOL_SETUP_GenSec(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(1, 10240),
        8);
// GenVtf: --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 1 10k
const ExprSetup BASEtOOL_SETUP_GenVtf(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(1, 10240),
        8);
// GnuGenBootSector: --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 1 10k
const ExprSetup BASEtOOL_SETUP_GnuGenBootSector(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(1, 10240),
        8);
// LzmaCompress: --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 1 10k
const ExprSetup BASEtOOL_SETUP_LzmaCompress(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(1, 10240),
        8);
// Split: --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 1 10k
const ExprSetup BASEtOOL_SETUP_Split(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(1, 10240),
        8);
// TianoCompress: --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 1 10k
const ExprSetup BASEtOOL_SETUP_TianoCompress(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(1, 10240),
        8);
// VfrCompile: --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 1 10k
const ExprSetup BASEtOOL_SETUP_VfrCompile(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(1, 10240),
        8);
// VolInfo: --sym-args 0 1 16 --sym-args 0 4 2 -sym-files 1 10k
const ExprSetup BASEtOOL_SETUP_VolInfo(
        SymArgsConfig(0, 1, 16),
        SymArgsConfig(0, 4, 2),
        vector<uint64_t>(1, 10240),
        8);


const map<string, const ExprSetup> baseTool_setups {
    {"BootSectImage",    BASEtOOL_SETUP_BootSectImage},
    {"EfiRom",           BASEtOOL_SETUP_EfiRom},
    {"GenFfs",           BASEtOOL_SETUP_GenFfs},
    {"GenFw",            BASEtOOL_SETUP_GenFw},
    {"GenSec",           BASEtOOL_SETUP_GenSec},
    {"GnuGenBootSector", BASEtOOL_SETUP_GnuGenBootSector},
    {"Split",            BASEtOOL_SETUP_Split},
    {"VfrCompile",       BASEtOOL_SETUP_VfrCompile},
    {"EfiLdrImage",      BASEtOOL_SETUP_EfiLdrImage},
    {"GenCrc32",         BASEtOOL_SETUP_GenCrc32},
    {"GenFv",            BASEtOOL_SETUP_GenFv},
    {"GenPage",          BASEtOOL_SETUP_GenPage},
    {"GenVtf",           BASEtOOL_SETUP_GenVtf},
    {"LzmaCompress",     BASEtOOL_SETUP_LzmaCompress},
    {"TianoCompress",    BASEtOOL_SETUP_TianoCompress},
    {"VolInfo",          BASEtOOL_SETUP_VolInfo}
};

// =================

const uint64_t DASE_GREP_DIFF_SYM_FILE_SIZE  = 100;

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

        fprintf(stderr, "],");
        if(!it->m_sym_file_size.empty())
        {
            fprintf(stderr, "sym-file [%lu, %lu bytes], ",
                            it->m_sym_file_size.size(),
                            it->m_sym_file_size.front());
        }

        fprintf(stderr, "sym-stdin[%lu]\n", it->m_sym_stdin_size);
    }
}


void CreteTests::generate_crete_config(const ParsedSymArgs& parsed_config,
        const string& exec_name)
{
    config::RunConfiguration crete_config;
    crete_config.set_executable(exec_name);

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
    if(parsed_config.m_sym_stdin_size > 0)
    {
        config::STDStream config_stdin;
        config_stdin.size = parsed_config.m_sym_stdin_size;
        config_stdin.value.resize(config_stdin.size, 0);
        config_stdin.concolic = true;
        crete_config.set_stdin(config_stdin);
    }

    // 3. sym-file
    for(uint64_t i = 0; i <= parsed_config.m_sym_file_size.size(); ++i)
    {
        if(i > parsed_config.m_sym_args.size()) break;

        if(i != 0)
        {
            // Remove the last argument for adding symbolic file
            crete_config.remove_last_argument();

            config::File config_file;
            stringstream sym_file_name;
            sym_file_name << "crete_sym_file_" << i;

            config_file.path = fs::path(CRETE_RAMDISK_PATH) / sym_file_name.str();
            config_file.size = parsed_config.m_sym_file_size.front();
            config_file.concolic = true;
            crete_config.add_file(config_file);
        }

        stringstream config_name;
        config_name << "auto." << std::setfill('0') << std::setw(3) << ++m_config_count << "."
                    << fs::path(exec_name).filename().string() << ".xml" ;
        fs::path out_config_file = fs::path(m_outputDirectory) / config_name.str();
        cerr << out_config_file.string() << endl;

        boost::property_tree::ptree pt_config;
        crete_config.write_mini(pt_config);
        boost::property_tree::write_xml(
                out_config_file.string(), pt_config, std::locale(),
                boost::property_tree::xml_writer_make_settings<std::string>('\t', 1));
    };
}

// suite_name: "klee" for coreutils, or "dase" for grep/diff
void CreteTests::gen_crete_tests_coreutils_grep_diff(string suite_name)
{
    vector<ParsedSymArgs> parsed_configs;
    set<string> eval;
    if(suite_name == "klee")
    {
        vector<ParsedSymArgs> normal_configs = KLEE_SETUP_NORMAL.get_ParsedSymArgs();
        for(set<string>::const_iterator it = coreutil_klee_osdi.begin();
                it != coreutil_klee_osdi.end(); ++ it) {
            map<string, const ExprSetup>::const_iterator special_setup_it =  coreutil_klee_osdi_special.find(*it);
            if( special_setup_it != coreutil_klee_osdi_special.end())
            {
                parsed_configs = special_setup_it->second.get_ParsedSymArgs();
            } else {
                parsed_configs = normal_configs;
            }

            printf_parsed_config(parsed_configs);

            for(uint64_t i = 0; i < parsed_configs.size(); ++i)
            {
                generate_crete_config(parsed_configs[i], (guest_executable_folder + *it));
            }
        }
    } else if (suite_name == "dase") {
//        parsed_configs = parse_symArgsConfig(KLEE_SYM_ARGS_CONFIG_1, KLEE_SYM_ARGS_CONFIG_2,
//                DASE_GREP_DIFF_SYM_FILE_SIZE, KLEE_SYM_STDIN_SIZE);

//        eval = dase_expr;
        assert(0);
    } else if (suite_name == "basetools") {
        for(set<string>::const_iterator it = baseTools.begin();
                it != baseTools.end(); ++ it) {
            map<string, const ExprSetup>::const_iterator special_setup_it =  baseTool_setups.find(*it);
            if( special_setup_it != baseTool_setups.end())
            {
                parsed_configs = special_setup_it->second.get_ParsedSymArgs();
            } else {
                fprintf(stderr, "[CRETE ERROR] Missing setup for: %s\n", it->c_str());
                continue;
            }

            printf_parsed_config(parsed_configs);

            for(uint64_t i = 0; i < parsed_configs.size(); ++i)
            {
                generate_crete_config(parsed_configs[i], (guest_executable_folder + *it));
            }
        }
    } else {
        assert(0);
    }

    // Generate all configs for each gnu coreutil progs


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

CreteConfig::CreteConfig(int argc, char* argv[])
:m_ops_descr(make_options())
{
    parse_options(argc, argv);
    process_options();
}

po::options_description CreteConfig::make_options()
{
    po::options_description desc("Options");

    desc.add_options()
            ("help,h", "displays help message")
            ("suite,s", po::value<string>(), "generate configurations for suite: klee, dase, basetools, parse-cmd")
            ("path,p", po::value<fs::path>(), "input file with sample commandline invocation for generating configs, required for parse-cmd")
        ;

    return desc;
}

void CreteConfig::parse_options(int argc, char* argv[])
{
    po::store(po::parse_command_line(argc, argv, m_ops_descr), m_var_map);
    po::notify(m_var_map);
}

void CreteConfig::process_options()
{
    using namespace std;

    if(m_var_map.size() == 0)
    {
        cout << "Missing arguments" << endl;
        cout << "Use '--help' for more details" << endl;
        BOOST_THROW_EXCEPTION(Exception() << err::arg_count(0));
    }
    if(m_var_map.count("help"))
    {
        cerr << m_ops_descr << endl;
        exit(0);
    }

    if(m_var_map.count("suite"))
    {
        string m_suite_name = m_var_map["suite"].as<string>();

        if(m_suite_name.empty())
        {
            cerr << "please give the name of the test suite for generating config files\n";
            exit(1);
        }

        if(m_suite_name == "klee")
        {
            CreteTests crete_tests(NULL);
            crete_tests.gen_crete_tests_coreutils_grep_diff("klee");
        } else if (m_suite_name == "dase")
        {
            CreteTests crete_tests(NULL);
            crete_tests.gen_crete_tests_coreutils_grep_diff("dase");
        } else if (m_suite_name == "basetools")
        {
            CreteTests crete_tests(NULL);
            crete_tests.gen_crete_tests_coreutils_grep_diff("basetools");
        } else if (m_suite_name == "parse-cmd")
        {
            assert(m_var_map.count("path"));
            fs::path input_path =  m_var_map["path"].as<fs::path>();
            if(!fs::is_regular_file(input_path))
            {
                BOOST_THROW_EXCEPTION(Exception() << err::file_missing(input_path.string()));
            }

            CreteTests crete_tests(input_path.string().c_str());
            crete_tests.gen_crete_tests(false);
        } else {
            assert(0);
        }
    }
}

int main (int argc, char **argv)
{
    try
    {
        CreteConfig configs(argc, argv);
    }
    catch(...)
    {
        cerr << "[CRETE Config Generator] Exception Info: \n"
                << boost::current_exception_diagnostic_information() << endl;
        return -1;
    }

    return 0;
}
