/*
 * tc-replay.h
 *
 *  Created on: Apr 30, 2017
 *      Author: chenbo
 */

#ifndef LIB_INCLUDE_CRETE_TC_REPLAY_H_
#define LIB_INCLUDE_CRETE_TC_REPLAY_H_

#include <boost/filesystem.hpp>
#include <boost/unordered_map.hpp>

#include <boost/serialization/serialization.hpp>
//#include <boost/serialization/string.hpp>
//#include <boost/serialization/utility.hpp>

//#include <boost/archive/binary_iarchive.hpp>

#include <vector>
#include <string>

using namespace std;
namespace fs = boost::filesystem;

static const char *CRETE_TC_REPLAY_CK_EXP_INFO = "/tmp/crete.replay.ck_exp.bin";
static const char *CRETE_TC_REPLAY_GDB_SCRIPT = "crete.replay.script.gdb";
static const char *CRETE_EXPLO_SUMMARY_LOG = "summary.log";

namespace crete{

struct CheckExploitable
{
    string m_p_exploitable_script;

    string m_p_launch;
    string m_p_exec;
    vector<string> m_args;
    vector<string> m_files;
    string m_stdin_file;

    CheckExploitable() {};
    ~CheckExploitable() {};

    void sanity_check()
    {
        assert(fs::is_regular(m_p_exploitable_script));
        assert(fs::is_directory(m_p_launch));
        assert(fs::is_regular(m_p_exec));

        assert(!m_args.empty());
        for(uint64_t i = 0; i < m_files.size(); ++i)
        {
            fs::path file_p(m_files[i]);
            if(file_p.string().find('/') != 0)
            {
                assert(fs::is_regular(fs::path(m_p_launch) / file_p));
            } else {
                assert(fs::is_regular(file_p));
            }

            bool found = false;
            for(uint64_t j = 0; j < m_args.size(); ++j)
            {
                if(m_args[j] == m_files[i])
                {
                    found = true;
                    break;
                }
            }
            assert(found && "file from m_files is not used in m_args");
        }

        if(m_stdin_file.find('/') != 0)
        {
            assert(fs::is_regular(fs::path(m_p_launch) / m_stdin_file));
        } else {
            assert(fs::is_regular(m_stdin_file));
        }
    }

    static inline string adjust_argv_for_cmd(string cmd)
    {
        string ret;

        // 1. add '' to surround the whole string
        ret += '\'';

        for(unsigned long i = 0; i < cmd.size(); ++i)
        {
            //2. add escape to '
            if(cmd[i] == '\'')

            {
                ret += '\\';
            }

            ret += cmd[i];
        }

        ret += '\'';

        return ret;
    }

    void gen_gdb_script(const string& output_file)
    {
        sanity_check();

        ofstream ofs(output_file.c_str());
        ofs << "#---------------------------------\n"
            << "# GDB script to replay the crash:\n"
            << "# `gdb -x " << CRETE_TC_REPLAY_GDB_SCRIPT << "`\n"
            << "#---------------------------------\n";

        // Add potential libc debug symbolic location
        ofs << "set debug-file-directory /usr/lib/debug"<< endl;
        ofs << "source " << m_p_exploitable_script << endl;
        ofs << "file " << m_p_exec << endl;

        ofs << "run";
        for(uint64_t i = 1; i < m_args.size(); ++i)
        {
            ofs << ' ' << adjust_argv_for_cmd(m_args[i]);
        }
        ofs << " < " << m_stdin_file << endl;

        ofs << "exploitable\n";
        ofs << "quit\n";
        ofs.close();
    }

    template <typename Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & m_p_launch;
        ar & m_p_exploitable_script;
        ar & m_p_exec;
        ar & m_args;
        ar & m_files;
        ar & m_stdin_file;
    }

};

enum ExploitableStatus
{
    EXPLOITABLE = 0,
    PROBABLY_EXPLOITABLE = 1,
    PROBABLY_NOT_EXPLOITABLE = 2,
    NOT_EXPLOITABLE = 3,
    UNKNOWN = 4,
    UNDEFINED = 5
};

const string CER_KEY_DESCR = "Description: ";
const string CER_KEY_SHORT_DESCR = "Short description: ";
const string CER_KEY_HASH = "Hash: ";
const string CER_KEY_EXP_TY = "Exploitability Classification: ";
const string CER_KEY_EXPLANATION = "Explanation: ";
const string CER_KEY_TAGS = "Other tags: ";

#define __MATCH_CER_KEY(key, out)                    \
        if(gdb_out[i].find(key) == 0)                \
        {                                            \
            out = gdb_out[i].substr(key.length());   \
            continue;                                \
        }

struct CheckExploitableResult
{
    string m_hash;
    ExploitableStatus m_exp_ty;

    string m_exp_ty_msg;
    string m_description;
    string m_short_desc;
    string m_explanation;
    string m_tags;

    CheckExploitableResult(const vector<string>& gdb_out)
    {
        for(uint64_t i = 0; i < gdb_out.size(); ++i)
        {
            __MATCH_CER_KEY(CER_KEY_DESCR, m_description);
            __MATCH_CER_KEY(CER_KEY_SHORT_DESCR, m_short_desc);
            __MATCH_CER_KEY(CER_KEY_HASH, m_hash);
            __MATCH_CER_KEY(CER_KEY_EXP_TY, m_exp_ty_msg);
            __MATCH_CER_KEY(CER_KEY_EXPLANATION, m_explanation);
            __MATCH_CER_KEY(CER_KEY_TAGS, m_tags);
        }

        if(!m_hash.empty())
        {
            std::size_t found = m_hash.find('.');
            if(found != string::npos)
            {
                string hash_1 = m_hash.substr(0, found);
                string hash_2 = m_hash.substr(found+1);
                if(hash_1 == hash_2)
                {
                    m_hash = hash_1;
                }
            }
        }

        if(m_exp_ty_msg == "EXPLOITABLE")
        {
            m_exp_ty = EXPLOITABLE;
        } else if(m_exp_ty_msg == "PROBABLY_EXPLOITABLE")
        {
            m_exp_ty = PROBABLY_EXPLOITABLE;
        } else if(m_exp_ty_msg == "PROBABLY_NOT_EXPLOITABLE")
        {
            m_exp_ty = PROBABLY_NOT_EXPLOITABLE;
        } else if(m_exp_ty_msg == "NOT_EXPLOITABLE")
        {
            m_exp_ty = NOT_EXPLOITABLE;
        } else if(m_exp_ty_msg == "UNKNOWN")
        {
            m_exp_ty = UNKNOWN;
        } else {
            m_exp_ty = UNDEFINED;
            m_exp_ty_msg = "UNDEFINED";
        }
    }

    void print()
    {

        ExploitableStatus m_exp_ty;

        cerr << "m_exp_ty: " << m_exp_ty<< endl;
        cerr << "m_exp_ty_msg: " << m_exp_ty_msg << endl;
        cerr << "m_description: " << m_description << endl;

        cerr << "m_short_desc: " << m_short_desc << endl;
        cerr << "m_explanation: " << m_explanation << endl;
        cerr << "m_tags: " << m_tags << endl;
    }
};
}



#endif /* LIB_INCLUDE_CRETE_TC_REPLAY_H_ */
