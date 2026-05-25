// Тестирование SkipList<T, Compare>.
// Это вспомогательный файл для самопроверки; в сдачу входят только
// skiplist.hpp, iorderedset.hpp, skiplist_except.hpp.

#include "skiplist.hpp"
#include "skiplist_except.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

static int g_failed = 0;
static int g_passed = 0;

#define CHECK(expr)                                                            \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::cerr << "FAIL  " << __FILE__ << ":" << __LINE__ << "  "       \
                      << #expr << "\n";                                        \
            ++g_failed;                                                        \
        } else {                                                               \
            ++g_passed;                                                        \
        }                                                                     \
    } while (0)

static void test_basic_insert_contains_size() {
    SkipList<int> s;
    CHECK(s.empty());
    CHECK(s.size() == 0);

    s.insert(10);
    s.insert(5);
    s.insert(20);
    s.insert(15);
    s.insert(5); // дубликат — не должен увеличить размер

    CHECK(s.size() == 4);
    CHECK(!s.empty());
    CHECK(s.contains(10));
    CHECK(s.contains(5));
    CHECK(s.contains(20));
    CHECK(s.contains(15));
    CHECK(!s.contains(7));
    CHECK(!s.contains(100));
}

static void test_iteration_sorted() {
    SkipList<int> s{50, 10, 30, 20, 40, 10, 50};
    CHECK(s.size() == 5);

    std::vector<int> got(s.begin(), s.end());
    std::vector<int> expected{10, 20, 30, 40, 50};
    CHECK(got == expected);

    int sum = 0;
    for (int x : s) sum += x;
    CHECK(sum == 150);

    // std::find_if / std::count_if на итераторах
    auto it = std::find_if(s.begin(), s.end(), [](int x) { return x > 25; });
    CHECK(it != s.end() && *it == 30);

    auto cnt = std::count_if(s.begin(), s.end(), [](int x) { return x >= 30; });
    CHECK(cnt == 3);

    CHECK(std::distance(s.begin(), s.end()) == 5);
}

static void test_find_lower_upper_bound() {
    SkipList<int> s{1, 3, 5, 7, 9, 11};

    CHECK(s.find(5) != s.end() && *s.find(5) == 5);
    CHECK(s.find(4) == s.end());

    CHECK(*s.lower_bound(5) == 5);
    CHECK(*s.lower_bound(4) == 5);
    CHECK(*s.lower_bound(6) == 7);
    CHECK(s.lower_bound(100) == s.end());

    CHECK(*s.upper_bound(5) == 7);
    CHECK(*s.upper_bound(4) == 5);
    CHECK(s.upper_bound(11) == s.end());
}

static void test_erase_by_value_and_iterator() {
    SkipList<int> s{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    CHECK(s.erase(5));
    CHECK(!s.contains(5));
    CHECK(s.size() == 9);
    CHECK(!s.erase(5));
    CHECK(!s.erase(999));

    auto it = s.find(7);
    CHECK(it != s.end());
    auto nxt = s.erase(it);
    CHECK(nxt != s.end() && *nxt == 8);
    CHECK(!s.contains(7));
    CHECK(s.size() == 8);

    // удаляем первый и последний
    CHECK(s.erase(1));
    CHECK(s.erase(10));
    CHECK(s.size() == 6);

    std::vector<int> got(s.begin(), s.end());
    std::vector<int> expected{2, 3, 4, 6, 8, 9};
    CHECK(got == expected);
}

static void test_kth_and_count() {
    SkipList<int> s{10, 20, 30, 40, 50, 60, 70, 80, 90, 100};

    for (std::size_t i = 0; i < s.size(); ++i) {
        CHECK(*s.kth(i) == static_cast<int>(10 * (i + 1)));
    }

    bool threw = false;
    try {
        (void)s.kth(s.size());
    } catch (const IndexOutOfRangeException&) {
        threw = true;
    }
    CHECK(threw);

    threw = false;
    try {
        (void)s.kth(1000);
    } catch (const SkipListException&) {
        threw = true;
    }
    CHECK(threw);

    CHECK(s.count_less(10) == 0);
    CHECK(s.count_less(11) == 1);
    CHECK(s.count_less(55) == 5);
    CHECK(s.count_less(100) == 9);
    CHECK(s.count_less(101) == 10);
    CHECK(s.count_less(-5) == 0);

    CHECK(s.count_range(20, 80) == 6);  // 20,30,40,50,60,70
    CHECK(s.count_range(25, 75) == 5);  // 30,40,50,60,70
    CHECK(s.count_range(0, 1000) == 10);
    CHECK(s.count_range(50, 50) == 0);  // полуоткрытый
    CHECK(s.count_range(80, 20) == 0);  // lo > hi
}

static void test_clear_and_reuse() {
    SkipList<int> s{1, 2, 3, 4, 5};
    s.clear();
    CHECK(s.empty());
    CHECK(s.size() == 0);
    CHECK(s.begin() == s.end());

    s.insert(42);
    CHECK(s.size() == 1);
    CHECK(*s.begin() == 42);
}

static void test_move_semantics() {
    SkipList<int> a{1, 2, 3, 4, 5};
    SkipList<int> b(std::move(a));
    CHECK(b.size() == 5);
    CHECK(a.empty()); // moved-from валидно пуст

    SkipList<int> c;
    c = std::move(b);
    CHECK(c.size() == 5);
    CHECK(b.empty());

    std::vector<int> got(c.begin(), c.end());
    std::vector<int> expected{1, 2, 3, 4, 5};
    CHECK(got == expected);
}

static void test_custom_compare() {
    // Сортировка по убыванию.
    SkipList<int, std::greater<int>> s{1, 2, 3, 4, 5};
    std::vector<int> got(s.begin(), s.end());
    std::vector<int> expected{5, 4, 3, 2, 1};
    CHECK(got == expected);

    CHECK(*s.kth(0) == 5);
    CHECK(*s.kth(4) == 1);
    CHECK(s.count_less(3) == 2); // элементов "меньше" по компаратору > = {5,4}
}

static void test_string_type() {
    SkipList<std::string> s{"banana", "apple", "cherry", "date"};
    CHECK(s.size() == 4);
    auto it = s.begin();
    CHECK(*it++ == "apple");
    CHECK(*it++ == "banana");
    CHECK(*it++ == "cherry");
    CHECK(*it++ == "date");
    CHECK(it == s.end());

    CHECK(s.count_less("cherry") == 2);
    CHECK(*s.lower_bound("blueberry") == "cherry");
}

static void test_polymorphic_via_interface() {
    SkipList<int> impl{3, 1, 4, 1, 5, 9, 2, 6};
    IOrderedSet<int>* iface = &impl;
    CHECK(iface->size() == 7);
    CHECK(iface->contains(5));
    CHECK(iface->erase(5));
    CHECK(!iface->contains(5));
    iface->insert(42);
    CHECK(iface->contains(42));
    iface->clear();
    CHECK(iface->empty());
}

// Сравнение с std::set на случайных данных - проверка корректности
// поиска / kth / count_less / итерации.
static void test_stress_vs_std_set() {
    std::mt19937 gen(123456);
    std::uniform_int_distribution<int> dist(-1000, 1000);

    SkipList<int> sl;
    std::set<int> ref;

    constexpr int OPS = 5000;
    for (int op = 0; op < OPS; ++op) {
        int v = dist(gen);
        int action = gen() % 4;
        if (action == 0) {
            sl.insert(v);
            ref.insert(v);
        } else if (action == 1) {
            bool a = sl.erase(v);
            bool b = (ref.erase(v) > 0);
            CHECK(a == b);
        } else if (action == 2) {
            CHECK(sl.contains(v) == (ref.count(v) > 0));
        } else {
            auto a = sl.lower_bound(v);
            auto b = ref.lower_bound(v);
            CHECK((a == sl.end()) == (b == ref.end()));
            if (a != sl.end() && b != ref.end()) {
                CHECK(*a == *b);
            }
        }
    }

    CHECK(sl.size() == ref.size());

    // полная сверка итерации
    {
        std::vector<int> got(sl.begin(), sl.end());
        std::vector<int> expected(ref.begin(), ref.end());
        CHECK(got == expected);
    }

    // kth и count_less
    {
        std::vector<int> ordered(ref.begin(), ref.end());
        for (std::size_t k = 0; k < ordered.size(); ++k) {
            CHECK(*sl.kth(k) == ordered[k]);
        }

        for (int probe = -1100; probe <= 1100; probe += 37) {
            std::size_t expected_lt = static_cast<std::size_t>(
                std::distance(ordered.begin(),
                              std::lower_bound(ordered.begin(),
                                               ordered.end(), probe)));
            CHECK(sl.count_less(probe) == expected_lt);
        }
    }
}

int main() {
    test_basic_insert_contains_size();
    test_iteration_sorted();
    test_find_lower_upper_bound();
    test_erase_by_value_and_iterator();
    test_kth_and_count();
    test_clear_and_reuse();
    test_move_semantics();
    test_custom_compare();
    test_string_type();
    test_polymorphic_via_interface();
    test_stress_vs_std_set();

    std::cout << "passed: " << g_passed << ", failed: " << g_failed << "\n";
    return g_failed == 0 ? 0 : 1;
}
