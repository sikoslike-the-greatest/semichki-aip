#ifndef SKIPLIST_HPP
#define SKIPLIST_HPP

#include "iorderedset.hpp"
#include "skiplist_except.hpp"

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <new>
#include <random>
#include <string>
#include <utility>

// =============================================================================
// SkipList<T, Compare> — упорядоченный индексируемый контейнер на основе
// вероятностной структуры данных skip list.
//
// Особенности реализации:
//   * Все основные операции (insert/erase/find/lower_bound/upper_bound/kth/
//     count_less/count_range) — ожидаемая сложность O(log n).
//   * Поддерживаются порядковые запросы за O(log n): kth(k), count_less(v),
//     count_range(lo, hi). Это достигается за счёт хранения «ширины»
//     (span) каждого перехода: span[i] = число level-0 шагов, которые
//     перекрывает указатель forward[i].
//   * Узел skip-листа выделяется один раз; внутренние массивы forward/span
//     длины = level узла. Голова (header_) хранит MAX_LEVEL указателей.
//   * Итератор — ForwardIterator. reference/pointer — const, поскольку
//     мутация значения нарушила бы инвариант упорядоченности.
//   * Дубликаты не хранятся: insert уже имеющегося значения — no-op
//     (поведение std::set, согласующееся с IOrderedSet).
// =============================================================================
template <typename T, typename Compare = std::less<T>>
class SkipList : public IOrderedSet<T> {
private:
    static constexpr int    MAX_LEVEL   = 16;
    static constexpr double PROBABILITY = 0.5;

    // ---------------------------------------------------------------------
    // Узел skip-листа.
    //
    // Используется анонимное union-поле для хранения value, чтобы голова
    // (header_) могла не требовать наличия конструктора по умолчанию у T:
    // в голове value не конструируется и не разрушается. Для остальных
    // узлов value создаётся placement-new в конструкторе и разрушается
    // явным вызовом деструктора в ~Node().
    // ---------------------------------------------------------------------
    struct Node {
        union {
            T value;
        };
        Node**      forward;
        std::size_t* span;
        int         level;
        bool        is_header;

        // Конструктор для head-узла: value не конструируется.
        explicit Node(int lvl)
            : forward(nullptr), span(nullptr), level(lvl), is_header(true) {
            forward = new Node*[lvl];
            span    = new std::size_t[lvl];
            for (int i = 0; i < lvl; ++i) {
                forward[i] = nullptr;
                span[i]    = 0;
            }
        }

        // Конструктор для data-узла (копирование значения).
        Node(const T& v, int lvl)
            : forward(nullptr), span(nullptr), level(lvl), is_header(false) {
            forward = new Node*[lvl];
            span    = new std::size_t[lvl];
            for (int i = 0; i < lvl; ++i) {
                forward[i] = nullptr;
                span[i]    = 0;
            }
            ::new (static_cast<void*>(&value)) T(v);
        }

        // Конструктор для data-узла (перемещение значения).
        Node(T&& v, int lvl)
            : forward(nullptr), span(nullptr), level(lvl), is_header(false) {
            forward = new Node*[lvl];
            span    = new std::size_t[lvl];
            for (int i = 0; i < lvl; ++i) {
                forward[i] = nullptr;
                span[i]    = 0;
            }
            ::new (static_cast<void*>(&value)) T(std::move(v));
        }

        ~Node() {
            if (!is_header) {
                value.~T();
            }
            delete[] forward;
            delete[] span;
        }

        Node(const Node&)            = delete;
        Node& operator=(const Node&) = delete;
        Node(Node&&)                 = delete;
        Node& operator=(Node&&)      = delete;
    };

    Node*       header_;
    std::size_t size_;
    int         current_level_;
    Compare     comp_;

    // ГПСЧ инициализируется один раз в конструкторе (требование задания).
    mutable std::mt19937              rng_;
    mutable std::bernoulli_distribution coin_;

    // -------------------------- утилиты сравнения --------------------------
    bool less_(const T& a, const T& b) const { return comp_(a, b); }
    bool equal_(const T& a, const T& b) const {
        return !comp_(a, b) && !comp_(b, a);
    }

    // Случайный уровень нового узла: геометрическое распределение, p = 0.5,
    // ограничено сверху MAX_LEVEL.
    int random_level_() const {
        int lvl = 1;
        while (lvl < MAX_LEVEL && coin_(rng_)) {
            ++lvl;
        }
        return lvl;
    }

    // ---------------------------------------------------------------------
    // Поиск предшественников `value` на каждом уровне.
    //   update[i] — последний узел на уровне i, чьё forward[i] >= value;
    //   rank[i]   — позиция (1-индексная) узла update[i] на уровне 0;
    //               для головы rank == 0.
    // Используется в insert и erase.
    // ---------------------------------------------------------------------
    void find_predecessors_(const T& value, Node** update,
                            std::size_t* rank) const {
        Node* x = header_;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            std::size_t r = (i == MAX_LEVEL - 1) ? 0 : rank[i + 1];
            while (x->forward[i] && less_(x->forward[i]->value, value)) {
                r += x->span[i];
                x = x->forward[i];
            }
            update[i] = x;
            rank[i]   = r;
        }
    }

    // Унифицированный insert (для lvalue и rvalue value).
    template <typename U>
    void insert_impl_(U&& value) {
        Node*       update[MAX_LEVEL];
        std::size_t rank[MAX_LEVEL];
        find_predecessors_(value, update, rank);

        Node* next = update[0]->forward[0];
        if (next && equal_(next->value, value)) {
            return; // дубликат — игнорируем (set-семантика)
        }

        int lvl = random_level_();
        if (lvl > current_level_) {
            for (int i = current_level_; i < lvl; ++i) {
                update[i]        = header_;
                rank[i]          = 0;
                header_->span[i] = size_;
            }
            current_level_ = lvl;
        }

        Node* new_node = new Node(std::forward<U>(value), lvl);

        // Перелинковка и обновление span на уровнях [0, lvl).
        for (int i = 0; i < lvl; ++i) {
            new_node->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = new_node;

            // Стандартные формулы для skip-листа с индексированием:
            //   span нового перехода update[i] -> new_node
            //     = (rank[0] - rank[i]) + 1;
            //   span старого перехода update[i] -> next (теперь new_node -> next)
            //     = old_span - (rank[0] - rank[i]).
            new_node->span[i]  = update[i]->span[i] - (rank[0] - rank[i]);
            update[i]->span[i] = (rank[0] - rank[i]) + 1;
        }
        // На уровнях выше lvl новый узел "пройден" - просто +1 к span.
        for (int i = lvl; i < current_level_; ++i) {
            update[i]->span[i] += 1;
        }

        ++size_;
    }

    // Удалить узел target. update[] — заранее найденные предшественники
    // (find_predecessors_ был вызван с target->value).
    void erase_node_(Node* target, Node** update) {
        for (int i = 0; i < current_level_; ++i) {
            if (update[i]->forward[i] == target) {
                update[i]->span[i] += target->span[i] - 1;
                update[i]->forward[i] = target->forward[i];
            } else {
                update[i]->span[i] -= 1;
            }
        }
        delete target;
        --size_;
        while (current_level_ > 1 &&
               header_->forward[current_level_ - 1] == nullptr) {
            header_->span[current_level_ - 1] = 0;
            --current_level_;
        }
    }

    // ---------- внутренние поисковые помощники (без обёртки итератором) ----
    Node* find_node_(const T& value) const {
        Node* x = header_;
        for (int i = current_level_ - 1; i >= 0; --i) {
            while (x->forward[i] && less_(x->forward[i]->value, value)) {
                x = x->forward[i];
            }
        }
        x = x->forward[0];
        return (x && equal_(x->value, value)) ? x : nullptr;
    }

    Node* lower_bound_node_(const T& value) const {
        Node* x = header_;
        for (int i = current_level_ - 1; i >= 0; --i) {
            while (x->forward[i] && less_(x->forward[i]->value, value)) {
                x = x->forward[i];
            }
        }
        return x->forward[0];
    }

    Node* upper_bound_node_(const T& value) const {
        Node* x = header_;
        for (int i = current_level_ - 1; i >= 0; --i) {
            // step while forward.value <= value, т.е. !(value < forward.value)
            while (x->forward[i] && !less_(value, x->forward[i]->value)) {
                x = x->forward[i];
            }
        }
        return x->forward[0];
    }

    Node* kth_node_(std::size_t k) const {
        // 0-индексный k-й элемент = 1-индексная цель target = k + 1.
        const std::size_t target    = k + 1;
        Node*             x         = header_;
        std::size_t       traversed = 0;
        for (int i = current_level_ - 1; i >= 0; --i) {
            while (x->forward[i] && traversed + x->span[i] <= target) {
                traversed += x->span[i];
                x = x->forward[i];
            }
            if (traversed == target) {
                return x;
            }
        }
        return x; // недостижимо, если k < size_ (проверяется снаружи)
    }

    void destroy_all_() noexcept {
        if (!header_) return;
        Node* x = header_->forward[0];
        while (x) {
            Node* next = x->forward[0];
            delete x;
            x = next;
        }
        delete header_;
        header_ = nullptr;
    }

public:
    // ============================== Итератор ==============================
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using pointer           = const T*;
        using reference         = const T&;

        Iterator() noexcept : node_(nullptr) {}

        reference operator*() const { return node_->value; }
        pointer   operator->() const { return &node_->value; }

        Iterator& operator++() {
            node_ = node_->forward[0];
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const noexcept {
            return node_ == other.node_;
        }
        bool operator!=(const Iterator& other) const noexcept {
            return node_ != other.node_;
        }

    private:
        friend class SkipList;
        explicit Iterator(Node* n) noexcept : node_(n) {}
        Node* node_;
    };

    // Итератор и const_iterator у нас совпадают: reference/pointer уже const,
    // модифицировать значения через итератор нельзя в любом случае.
    using iterator        = Iterator;
    using const_iterator  = Iterator;
    using value_type      = T;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference       = const T&;
    using const_reference = const T&;
    using pointer         = const T*;
    using const_pointer   = const T*;
    using key_compare     = Compare;
    using value_compare   = Compare;

    // ============================ Конструкторы =============================
    SkipList() : SkipList(Compare()) {}

    explicit SkipList(Compare comp)
        : header_(new Node(MAX_LEVEL)),
          size_(0),
          current_level_(1),
          comp_(std::move(comp)),
          rng_(std::random_device{}()),
          coin_(PROBABILITY) {}

    SkipList(std::initializer_list<T> ilist) : SkipList() {
        for (const auto& v : ilist) {
            insert(v);
        }
    }

    // -------------------------------------------------------------------------
    // Копирование запрещено: SkipList владеет графом динамически выделенных
    // узлов с вероятностной топологией уровней. «Честная» глубокая копия
    // требует пересоздать всю структуру (O(n log n) ожидаемо) и аккуратно
    // продублировать значения T, что мало где нужно на практике. Если нужна
    // копия — она должна быть явной (например, через отдельную фабричную
    // функцию или итерирование с insert в новый контейнер). Поэтому копи-
    // конструктор и копи-присваивание удалены, как требует задание.
    // -------------------------------------------------------------------------
    SkipList(const SkipList&)            = delete;
    SkipList& operator=(const SkipList&) = delete;

    SkipList(SkipList&& other) noexcept
        : header_(other.header_),
          size_(other.size_),
          current_level_(other.current_level_),
          comp_(std::move(other.comp_)),
          rng_(std::move(other.rng_)),
          coin_(other.coin_) {
        // Оставляем `other` в валидном пустом состоянии. Аллокация нового
        // заголовка теоретически может бросить bad_alloc; на практике для
        // одного указателя с массивами длиной MAX_LEVEL это пренебрежимо
        // редкое событие, поэтому помечаем noexcept (как и требует задание).
        other.header_        = new Node(MAX_LEVEL);
        other.size_          = 0;
        other.current_level_ = 1;
    }

    SkipList& operator=(SkipList&& other) noexcept {
        if (this != &other) {
            // Просто меняем содержимое местами; деструктор `other` уберёт
            // старое состояние при выходе из своей области видимости (если
            // временный) либо когда вызывающий код этого захочет.
            std::swap(header_, other.header_);
            std::swap(size_, other.size_);
            std::swap(current_level_, other.current_level_);
            std::swap(comp_, other.comp_);
            std::swap(rng_, other.rng_);
            std::swap(coin_, other.coin_);
        }
        return *this;
    }

    ~SkipList() override { destroy_all_(); }

    // ============================ Модификаторы ============================
    void insert(const T& value) override { insert_impl_(value); }
    void insert(T&& value)                { insert_impl_(std::move(value)); }

    bool erase(const T& value) override {
        Node*       update[MAX_LEVEL];
        std::size_t rank[MAX_LEVEL];
        find_predecessors_(value, update, rank);

        Node* target = update[0]->forward[0];
        if (!target || !equal_(target->value, value)) {
            return false;
        }
        erase_node_(target, update);
        return true;
    }

    iterator erase(iterator pos) {
        // Поведение при невалидном итераторе — UB (по аналогии со STL).
        Node* target = pos.node_;
        Node* next   = target->forward[0];

        Node*       update[MAX_LEVEL];
        std::size_t rank[MAX_LEVEL];
        find_predecessors_(target->value, update, rank);

        erase_node_(target, update);
        return iterator(next);
    }

    void clear() override {
        if (!header_) {
            header_        = new Node(MAX_LEVEL);
            size_          = 0;
            current_level_ = 1;
            return;
        }
        Node* x = header_->forward[0];
        while (x) {
            Node* next = x->forward[0];
            delete x;
            x = next;
        }
        for (int i = 0; i < MAX_LEVEL; ++i) {
            header_->forward[i] = nullptr;
            header_->span[i]    = 0;
        }
        size_          = 0;
        current_level_ = 1;
    }

    // ============================ Поиск / доступ ============================
    bool contains(const T& value) const override {
        return find_node_(value) != nullptr;
    }

    iterator find(const T& value) {
        Node* x = find_node_(value);
        return x ? iterator(x) : end();
    }
    const_iterator find(const T& value) const {
        Node* x = find_node_(value);
        return x ? const_iterator(x) : end();
    }

    iterator lower_bound(const T& value) {
        return iterator(lower_bound_node_(value));
    }
    const_iterator lower_bound(const T& value) const {
        return const_iterator(lower_bound_node_(value));
    }

    iterator upper_bound(const T& value) {
        return iterator(upper_bound_node_(value));
    }
    const_iterator upper_bound(const T& value) const {
        return const_iterator(upper_bound_node_(value));
    }

    // ========================== Порядковые запросы =========================
    iterator kth(std::size_t k) {
        if (k >= size_) {
            throw IndexOutOfRangeException(
                "k=" + std::to_string(k) +
                ", size=" + std::to_string(size_));
        }
        return iterator(kth_node_(k));
    }
    const_iterator kth(std::size_t k) const {
        if (k >= size_) {
            throw IndexOutOfRangeException(
                "k=" + std::to_string(k) +
                ", size=" + std::to_string(size_));
        }
        return const_iterator(kth_node_(k));
    }

    std::size_t count_less(const T& value) const {
        std::size_t count = 0;
        Node*       x     = header_;
        for (int i = current_level_ - 1; i >= 0; --i) {
            while (x->forward[i] && less_(x->forward[i]->value, value)) {
                count += x->span[i];
                x = x->forward[i];
            }
        }
        return count;
    }

    std::size_t count_range(const T& lo, const T& hi) const {
        // Полуоткрытый интервал [lo, hi). Если lo >= hi - пустой диапазон.
        if (!less_(lo, hi)) {
            return 0;
        }
        return count_less(hi) - count_less(lo);
    }

    // =============================== Размер ===============================
    std::size_t size()  const override { return size_; }
    bool        empty() const override { return size_ == 0; }

    // ============================== Итераторы ==============================
    iterator       begin()        { return iterator(header_->forward[0]); }
    iterator       end()          { return iterator(nullptr); }
    const_iterator begin()  const { return const_iterator(header_->forward[0]); }
    const_iterator end()    const { return const_iterator(nullptr); }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend()   const { return end(); }
};

#endif // SKIPLIST_HPP
