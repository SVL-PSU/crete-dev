#ifndef CRETE_LOGGER_ATTR_H
#define CRETE_LOGGER_ATTR_H

#include <string>
#include <stdint.h>

#include <boost/shared_ptr.hpp>

#include <crete/logger.h>

namespace crete
{
namespace log
{
    class StopWatchImpl;

    class Attribute
    {
    public:
        Attribute();
        Attribute(std::string title);

        virtual std::string print() const = 0;
        const std::string& title() const;

    private:
        std::string title_;
    };

    std::ostream& operator<<(std::ostream& lhs, const Attribute& rhs);

    class Stopwatch : public Attribute
    {
    public:
        Stopwatch();
        Stopwatch(std::string title);

        std::string get_elapsed() const;
        virtual std::string print() const;

    private:
        boost::shared_ptr<StopWatchImpl> impl_;
    };

    template <typename Attr>
    class Scope
    {
    public:
        Scope(Logger& logger, std::string title);
        Scope(Attr attr, Logger& logger);
        Scope(Attr attr, Logger& logger, std::string title, bool log_attr_scope_end);
        ~Scope();

        template <typename T>
        friend std::ostream& operator<<(std::ostream& lhs, const Scope<T>& rhs);

    private:
        Attr attr_;
        Logger& logger_;
        std::string title_;
        bool log_attr_scope_end_;
    };

    template <typename Attr>
    Scope<Attr>::Scope(Attr attr, Logger& logger) :
        attr_(attr),
        logger_(logger),
        title_(""),
        log_attr_scope_end_(true)
    {
    }

    template <typename Attr>
    Scope<Attr>::Scope(Attr attr, Logger& logger, std::string title, bool log_at_scope_end) :
        attr_(attr),
        logger_(logger),
        title_(title),
        log_attr_scope_end_(log_at_scope_end)
    {
        if(!title_.empty())
            logger_ << "<" << title_ << ">";
    }

    template <typename Attr>
    Scope<Attr>::~Scope()
    {
        if(log_attr_scope_end_ && title_.empty())
            logger_ << attr_;
        else if(log_attr_scope_end_ && !title_.empty())
            logger_ << "</" << title_ << ": " << attr_ << ">";
        else if(!title_.empty())
            logger_ << "</" << title_ << ">";
    }

    template <typename Attr>
    std::ostream& operator<<(std::ostream& lhs, const Scope<Attr>& rhs)
    {
        lhs << rhs.attr_;

        return lhs;
    }
}
}

#endif // CRETE_LOGGER_ATTR_H
