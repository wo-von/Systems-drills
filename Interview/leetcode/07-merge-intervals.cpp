#include <algorithm>
#include <cassert>
#include <vector>

std::vector<std::pair<int, int>> mergeIntervals(std::vector<std::pair<int, int>> &intervals) {
    if (intervals.empty()) {
        return {};
    }
    // sort intervals based in the starting point
    std::sort(intervals.begin(), intervals.end());
    std::vector<std::pair<int, int>> res;
    res.push_back(intervals[0]);
    int start = intervals[0].first;
    int end = intervals[0].second;
    for (size_t i = 1; i < intervals.size(); i++) {
        int new_start = intervals[i].first;
        int new_end = intervals[i].second;
        if (new_start <= end) {
            res.pop_back();
            end = std::max(end, new_end);
            res.push_back({start, end});
        } else {
            res.push_back({new_start, new_end});
            start = new_start;
            end = new_end;
        }
    }
    return res;
}

int main(void) {
    // test to see if merging works
    {
        std::vector<std::pair<int, int>> test = {{1, 3}, {2, 6}, {8, 10}, {15, 18}};
        std::vector<std::pair<int, int>> res = mergeIntervals(test);
        std::vector<std::pair<int, int>> sentinel = {{1, 6}, {8, 10}, {15, 18}};
        assert((res == sentinel));
    }
    // test multiple consecutive merges
    {
        std::vector<std::pair<int, int>> test = {{1, 3}, {2, 6}, {5, 10}, {15, 18}};
        std::vector<std::pair<int, int>> res = mergeIntervals(test);
        std::vector<std::pair<int, int>> sentinel = {{1, 10}, {15, 18}};
        assert((res == sentinel));
    }
    // test nested intervals
    {
        std::vector<std::pair<int, int>> test = {{1, 10}, {2, 3}};
        std::vector<std::pair<int, int>> res = mergeIntervals(test);
        std::vector<std::pair<int, int>> sentinel = {{1, 10}};
        assert((res == sentinel));
    }
    // test touching intervals
    {
        std::vector<std::pair<int, int>> test = {{1, 4}, {4, 6}};
        std::vector<std::pair<int, int>> res = mergeIntervals(test);
        std::vector<std::pair<int, int>> sentinel = {{1, 6}};
        assert((res == sentinel));
    }
    // test unsorted intervals
    {
        std::vector<std::pair<int, int>> test = {{8, 10}, {1, 3}};
        std::vector<std::pair<int, int>> res = mergeIntervals(test);
        std::vector<std::pair<int, int>> sentinel = {{1, 3}, {8, 10}};
        assert((res == sentinel));
    }
    return 0;
}
