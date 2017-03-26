#ifndef CRETE_TEST_CASE_H
#define CRETE_TEST_CASE_H

#include <iostream>
#include <stdint.h>
#include <vector>
#include <iterator>
#include <crete/trace_tag.h>

#include <boost/functional/hash.hpp>

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
            std::size_t seed = 0;
            boost::hash_combine(seed, i.name_size);
            boost::hash_combine(seed, i.name);
            boost::hash_combine(seed, i.data_size);
            boost::hash_combine(seed, i.data);

            return seed;
        }
    };

    typedef std::vector<TestCaseElement> TestCaseElements;
    typedef size_t TestCaseHash;

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

        void set_traceTag(const creteTraceTag_ty &explored_nodes,
                const creteTraceTag_ty &semi_explored_node, const creteTraceTag_ty &new_nodes);
        creteTraceTag_ty get_traceTag_explored_nodes() const { return m_explored_nodes; }
        creteTraceTag_ty get_traceTag_semi_explored_node() const { return m_semi_explored_node; }
        creteTraceTag_ty get_traceTag_new_nodes() const { return m_new_nodes; }

        TestCaseHash hash() const; // Hash for test case elements
        TestCaseHash complete_hash() const;

        friend std::ostream& operator<<(std::ostream& os, const TestCase& tc);

        template <typename Archive>
        void serialize(Archive& ar, const unsigned int version)
        {
            (void)version;

            ar & priority_;

            ar & elems_;

            ar & m_explored_nodes;
            ar & m_semi_explored_node;
            ar & m_new_nodes;
        }

        bool operator==(TestCase const& other) const
        {
            return  elems_ == other.elems_;
        }

        friend std::size_t hash_value(TestCase const& i)
        {
            return boost::hash_value(i.elems_);
        }

    protected:
    private:
        Priority priority_; // TODO: meaningless now. In the future, can be used to sort tests.

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
