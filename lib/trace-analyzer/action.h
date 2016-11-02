#ifndef CRETE_TRACE_ACTION_H
#define CRETE_TRACE_ACTION_H

namespace crete
{
namespace trace
{

enum class Action { move_left, move_right };

// Will the possible actions be move-left, move-right?
// If so, should I really be doing this with classes? I mean, I have to map policy(state, action) = probability
//class Action
//{
//public:
//protected:
//private:
//};

//class MoveLeft : public Action
//{
//};

//class MoveRight : public Action
//{
//};



} // namespace trace
} // namespace crete
#endif // CRETE_TRACE_ACTION_H

