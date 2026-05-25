# semichki-aip
- iorderedset.hpp - абстрактный интерфейс IOrderedSet<T>
- skiplist_except.hpp - иерархия исключений: SkipListException - ElementNotFoundException, IndexOutOfRangeException
- skiplist.hpp - реализация SkipList<T, Compare> (всё в одном заголовке)

## Бонус
- main.cpp - набор юнит и стресс-тестов (включая сверку с std::set на 5000 случайных операций)
- Makefile - make, make run, make clean

## Проверки
- Компиляция c++ -std=c++17 -Wall -Wextra -Wpedantic -O2 - без предупреждений
- Все 5795 проверок прошли
- ASan + UBSan - нет утечек памяти и UB
- Стресс-тест на 5000 случайных операций даёт полное совпадение с std::set по find, lower_bound, kth, count_less, порядку итерации