#include <cassert>
#include <list>
#include <unordered_map>

using It = std::list<std::pair<int, int>>::iterator;

class LRUCache {
  public:
    explicit LRUCache(int capacity) {
        this->capacity = static_cast<size_t>(capacity);
    }
    int get(int key) {
        auto k = keys.find(key);
        if (k == keys.end()) {
            return -1;
        }
        l_.splice(l_.begin(), l_, k->second);
        return l_.front().second;
    }
    void put(int key, int value) {
        auto k = keys.find(key);
        if (k != keys.end()) { // it exists
            l_.splice(l_.begin(), l_, k->second);
            l_.front().second = value;
        } else {
            l_.push_front({key, value});
            keys[key] = l_.begin();
            if (l_.size() > capacity) {
                auto back = l_.back();
                l_.pop_back();
                keys.erase(back.first);
            }
        }
    }

  private:
    std::list<std::pair<int, int>> l_;
    std::unordered_map<int, It> keys;
    size_t capacity;
};

int main(void) {
    {
        LRUCache c(3);
        c.put(1, 1);
        c.put(2, 2);
        c.put(3, 3);
        c.put(4, 4);
        int g = c.get(1);
        assert(g == -1);
    }
    {
        LRUCache c(2);
        c.put(1, 1);
        c.put(2, 2);
        assert(c.get(1) == 1);
        c.put(3, 3);
        assert(c.get(2) == -1);
        c.put(4, 4);
        assert(c.get(1) == -1);
        assert(c.get(3) == 3);
        assert(c.get(4) == 4);
    }
    {
        LRUCache c(2);
        c.put(1, 1);
        c.put(1, 10);
        assert(c.get(1) == 10);
    }
    return 0;
}
