#ifndef CRETE_TEST_CASE_H
#define CRETE_TEST_CASE_H

#include <iostream>
#include <stdint.h>
#include <vector>
#include <iterator>
#include <crete/trace_tag.h>

#include <boost/functional/hash.hpp>
#include <boost/serialization/utility.hpp>

namespace crete
{
    struct TestCaseElement
    {
        uint32_t name_size;
        std::vector<uint8_t> name;
        uint32_t data_size;
        std::vector<uint8_t> data;

        template <typename Archive>
        void serialize(Archive& ar, const unsigned int version)
        {
            (void)version;

            ar & name_size;
            ar & name;
            ar & data_size;
            ar & data;
        }

#if !defined(CRETE_TC_COMPARE_H)
        bool operator==(TestCaseElement const& other) const
        {
            return  name_size == other.name_size &&
                    name == other.name &&
                    data_size == other.data_size &&
                    data == other.data;
        }
#endif

        friend std::size_t hash_value(TestCaseElement const& i)
        {
            return boost::hash_value(string(i.name.begin(), i.name.end())  +
                                     string(i.data.begin(), i.data.end()));
        }

        void print() const;
    };

    typedef std::vector<TestCaseElement> TestCaseElements;
    typedef uint64_t TestCaseIssueIndex;
    // <index of trace-tag node to negate, index of branch within a tt node to negate>
    typedef std::pair<uint32_t, uint32_t> TestCasePatchTraceTag_ty;
    // <index within an tc element, value>

    typedef std::vector<std::pair<uint32_t, uint8_t> > tcpe_data_ty;
    struct TestCasePatchElement_ty
    {
        std::string name;
        tcpe_data_ty data;

        template <typename Archive>
        void serialize(Archive& ar, const unsigned int version)
        {
            (void)version;

            ar & name;
            ar & data;
        }
    };

    class TestCase
    {
    public:
        typedef size_t Priority;

    public:
        TestCase();
        TestCase(const TestCase& tc);
        TestCase(const crete::TestCasePatchTraceTag_ty& tcp_tt,
                 const std::vector<crete::TestCasePatchElement_ty>& tcp_elems,
                 const TestCaseIssueIndex& base_tc_issue_index);

        void add_element(const TestCaseElement& e) { elems_.push_back(e); }

        const TestCaseElements& get_elements() const { return elems_; }
        void set_elements(const TestCaseElements &elems) {elems_ = elems;}
        void write(std::ostream& os) const;
        Priority get_priority() const { return priority_; }
        void set_priority(const Priority& p) { priority_ = p; }

        void set_traceTag(const creteTraceTag_ty &explored_nodes,
                const creteTraceTag_ty &semi_explored_node, const creteTraceTag_ty &new_nodes);
        creteTraceTag_ty get_traceTag_explored_nodes() const { return m_explored_nodes; }
        creteTraceTag_ty get_traceTag_semi_explored_node() const { return m_semi_explored_node; }
        creteTraceTag_ty get_traceTag_new_nodes() const { return m_new_nodes; }

        uint32_t get_tt_last_node_index() const;
        bool is_test_patch() const;
        void assert_tc_patch() const;

        void assert_issued_tc() const;
        TestCaseIssueIndex get_base_tc_issue_index() const;
        TestCaseIssueIndex get_issue_index() const;
        void set_issue_index(TestCaseIssueIndex index);

        void print() const;

        friend TestCase generate_complete_tc_from_patch(const TestCase& patch, const TestCase& base);
        friend std::ostream& operator<<(std::ostream& os, const TestCase& tc);

        template <typename Archive>
        void serialize(Archive& ar, const unsigned int version)
        {
            (void)version;

            ar & priority_;

            ar & m_patch;
            ar & m_issue_index;
            ar & m_base_tc_issue_index;
            ar & m_tcp_tt;
            ar & m_tcp_elems;

            ar & elems_;

            ar & m_explored_nodes;
            ar & m_semi_explored_node;
            ar & m_new_nodes;
        }

        bool operator==(TestCase const& other) const
        {
            return  elems_ == other.elems_;
        }

    protected:
    private:
        Priority priority_; // TODO: meaningless now. In the future, can be used to sort tests.

        // true: is tc_p (test case patch); false: is a tc_c (complete tc)
        bool m_patch;

        TestCaseIssueIndex m_issue_index; // The index of the current tc being issued by testPool
        TestCaseIssueIndex m_base_tc_issue_index;  // The issue_index of the base tc

        TestCasePatchTraceTag_ty m_tcp_tt;
        vector<TestCasePatchElement_ty> m_tcp_elems;

        TestCaseElements elems_;
        creteTraceTag_ty m_explored_nodes;
        creteTraceTag_ty m_semi_explored_node;
        creteTraceTag_ty m_new_nodes;
    };

    std::ostream& operator<<(std::ostream& os, const TestCaseElement& elem);
    std::ostream& operator<<(std::ostream& os, const TestCase& tc);

    void write(std::ostream& os, const std::vector<TestCase>& tcs);
    void write(std::ostream& os, const std::vector<TestCaseElement>& elems);
    void write(std::ostream& os, const TestCaseElement& elem);
    std::vector<TestCase> read_test_cases(std::istream& is);
    TestCaseElement read_test_case_element(std::istream& is);
    TestCase read_test_case(std::istream& is);
    bool empty_test_case(std::istream& is);
    uint32_t element_count_test_case(std::istream& is);
    std::vector<TestCase> retrieve_tests(const std::string& tc_dir);
    TestCase retrieve_test(const std::string& tc_path);

    void write_serialized(ostream& os, const TestCase& tc);
    TestCase read_serialized(istream& is);

    TestCase retrieve_test_serialized(const std::string& tc_path);
    vector<TestCase> retrieve_tests_serialized(const string& tc_dir);
}

#endif // CRETE_TEST_CASE_H
