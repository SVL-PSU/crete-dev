#ifndef CRETE_TEST_CASE_H
#define CRETE_TEST_CASE_H

#include <iostream>
#include <stdint.h>
#include <vector>
#include <iterator>
#include <crete/trace_tag.h>

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
    };

    typedef std::vector<TestCaseElement> TestCaseElements;

    class TestCase
    {
    public:
        typedef size_t Priority;

    public:
        TestCase();
        void add_element(const TestCaseElement& e) { elems_.push_back(e); }

        const TestCaseElements& get_elements() const { return elems_; }
        void write(std::ostream& os) const;
        Priority get_priority() const { return priority_; }
        void set_priority(const Priority& p) { priority_ = p; }

        void set_traceTag(const creteTraceTag_ty &explored_nodes, const creteTraceTag_ty &new_nodes);
        creteTraceTag_ty get_traceTag_explored_nodes() const { return m_explored_nodes; }
        creteTraceTag_ty get_traceTag_new_nodes() const { return m_new_nodes; }

        friend std::ostream& operator<<(std::ostream& os, const TestCase& tc);

        template <typename Archive>
        void serialize(Archive& ar, const unsigned int version)
        {
            (void)version;

            ar & elems_;
            ar & priority_;

            ar & m_explored_nodes;
            ar & m_new_nodes;
        }

    protected:
    private:
        TestCaseElements elems_;
        Priority priority_; // TODO: meaningless now. In the future, can be used to sort tests.

        creteTraceTag_ty m_explored_nodes;
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
