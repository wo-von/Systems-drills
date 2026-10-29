#include <algorithm>
#include <cassert>
#include <cstdio>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using Entry = std::pair<std::string, int>;

// Ranks a ahead of b: higher count first, then smaller word. Strict (<, never <=).
static bool better(const Entry &a, const Entry &b) {
    return a.second != b.second ? a.second > b.second : a.first < b.first;
}

// 1. unordered_map: operator[] inserts 0 for a missing key, so ++ is enough.
std::unordered_map<std::string, int> count_words(const std::vector<std::string> &words) {
    std::unordered_map<std::string, int> res;
    for (const auto &word : words) {
        res[word]++;
    }
    return res;
}

// 2. sort with a lambda: copy the map into a vector first (a hash map has no order,
//    and its keys are const, so it can't be sorted in place).
std::vector<Entry> sorted_by_count(const std::unordered_map<std::string, int> &counts) {
    std::vector<Entry> v(counts.begin(), counts.end());
    std::sort(v.begin(), v.end(), [](const Entry &a, const Entry &b) {
        return a.second != b.second ? a.second > b.second : a.first < b.first;
    });
    return v;
}

// 3. priority_queue with a custom order: top() is the LARGEST by the comparator.
//    With `better` as the comparator, "better" counts as "smaller", so top() is the
//    WORST of the kept entries: exactly the one to evict. O(n log k).
std::vector<std::string> top_k(const std::unordered_map<std::string, int> &counts, size_t k) {
    std::priority_queue<Entry, std::vector<Entry>, decltype(&better)> pq(&better);
    for (const auto &e : counts) {
        pq.push(e);
        if (pq.size() > k) {
            pq.pop();
        }
    }
    std::vector<std::string> res;
    while (!pq.empty()) { // pops worst first
        res.push_back(pq.top().first);
        pq.pop();
    }
    std::reverse(res.begin(), res.end()); // best first
    return res;
}

int main() {
    const std::vector<std::string> words = {"vm", "kvm", "vm",   "exit", "kvm",
                                            "vm", "ept", "exit", "kvm",  "mmio"};

    auto counts = count_words(words);
    assert(counts.size() == 5);
    assert(counts.at("vm") == 3 && counts.at("kvm") == 3 && counts.at("exit") == 2);
    assert(counts.at("ept") == 1 && counts.at("mmio") == 1);

    auto sorted = sorted_by_count(counts);
    const std::vector<Entry> want = {{"kvm", 3}, {"vm", 3}, {"exit", 2}, {"ept", 1}, {"mmio", 1}};
    assert(sorted == want);

    assert((top_k(counts, 2) == std::vector<std::string>{"kvm", "vm"}));
    assert((top_k(counts, 3) == std::vector<std::string>{"kvm", "vm", "exit"}));
    assert(top_k(counts, 0).empty());
    assert(top_k(counts, 10).size() == 5); // k larger than the input

    std::puts("all tests passed");
    return 0;
}
