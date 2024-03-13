#ifndef _QUEUE_HPP
#define _QUEUE_HPP

namespace edcom {
namespace queue {
#include <stddef.h>

template <typename T>
class Queue {
   private:
    struct QueueNode {
        QueueNode* next{};
        T value{};
    };
    QueueNode* _front{};
    QueueNode* _back{};
    size_t size{};

   public:
    [[nodiscard]] const T& front() { return _front->value; }

    void enqueue(const T& value) {
        if (size == 0) {
            _front = new QueueNode{nullptr, value};
            _back = _front;
            size++;
            return;
        }
        _back->next = new QueueNode{nullptr, value};
        _back = _back->next;
        size++;
    }

    void pop() {
        QueueNode* tmp = _front;
        _front = _front->next;
        delete tmp;
        size--;
    }

    ~Queue() {
       while (_front != nullptr) {
           QueueNode* tmp = _front;
           _front = _front->next;
           delete tmp;
       } 
    }
};

}  // namespace queue
}  // namespace edcom

#endif
