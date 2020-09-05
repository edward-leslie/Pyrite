#pragma once

#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

// TODO: After moving to C++20, we should probably replace this functionality with the ranges library.
// TODO: Add a size hint function to the iterator classes.
namespace py {

template <typename DerivedT>
class StdIteratorImpl {
public:
    using Derived = DerivedT;

    class Iterator {
    private:
        Derived& _iter;
    public:
        explicit Iterator(Derived& iter) : _iter(iter) { }

        typename Derived::Element operator*() const { return *_iter; }
        void operator++() { ++_iter; }
        bool operator!=(Iterator const&) { return _iter; }
    };

    Iterator begin() { return Iterator(Downcast()); }
    Iterator end() { return Iterator(Downcast()); }
private:
    Derived& Downcast() { return *static_cast<Derived*>(this); }
};

template <typename ContainerT>
class StdContainerIterator : public StdIteratorImpl<StdContainerIterator<ContainerT>> {
private:
    using Container = ContainerT;
    using Iterator = typename ContainerT::iterator;

    Iterator _iter;
    Iterator _end;
public:
    using Type = StdContainerIterator<ContainerT>;
    using Element = typename std::iterator_traits<Iterator>::reference;

    explicit StdContainerIterator(Container& c) : _iter(c.begin()), _end(c.end()) { }
    StdContainerIterator(Iterator& begin, Iterator& end) : _iter(begin), _end(end) { }

    StdContainerIterator(Type const&) = delete;
    StdContainerIterator(Type&&) noexcept = default;
    Type& operator=(Type const&) = delete;
    Type& operator=(Type&&) noexcept = default;

    Element operator*() const { return *_iter; }
    operator bool() { return _iter != _end; }
    void operator++() { ++_iter; }
};

template <typename ContainerT>
class StdContainerConstIterator : public StdIteratorImpl<StdContainerConstIterator<ContainerT>> {
private:
    using Container = ContainerT;
    using Iterator = typename ContainerT::const_iterator;

    Iterator _iter;
    Iterator _end;
public:
    using Type = StdContainerConstIterator<ContainerT>;
    using Element = typename std::iterator_traits<Iterator>::reference;

    explicit StdContainerConstIterator(Container const& c) : _iter(c.cbegin()), _end(c.cend()) { }
    StdContainerConstIterator(Iterator& begin, Iterator& end) : _iter(begin), _end(end) { }

    StdContainerConstIterator(Type const&) = delete;
    StdContainerConstIterator(Type&&) noexcept = default;
    Type& operator=(Type const&) = delete;
    Type& operator=(Type&&) noexcept = default;

    Element operator*() const { return *_iter; }
    operator bool() { return _iter != _end; }
    void operator++() { ++_iter; }
};

template <typename IterT, typename FnT>
class MapIterator : public StdIteratorImpl<MapIterator<IterT, FnT>> {
private:
    IterT _iter;
    FnT _map;
public:
    using Type = MapIterator<IterT, FnT>;
    using Element = typename std::invoke_result<FnT, typename IterT::Element>::type;

    explicit MapIterator(IterT&& iter, FnT&& map) : _iter(std::move(iter)), _map(std::move(map)) { }

    MapIterator(Type const&) = delete;
    MapIterator(Type&&) noexcept = default;
    Type& operator=(Type const&) = delete;
    Type& operator=(Type&&) noexcept = default;

    Element operator*() const { return _map(*_iter); }
    operator bool() { return _iter; }
    void operator++() { ++_iter; }
};

template <typename FnT>
struct Map {
    FnT _fn;
    explicit Map(FnT&& map) : _fn(std::move(map)) { }
};

template <typename IterT, typename FnT>
MapIterator<IterT, FnT> operator|(IterT&& iter, Map<FnT>&& map) {
    return MapIterator(std::forward<IterT>(iter), std::move(map._fn));
}

template <typename ContainerT, typename FnT>
MapIterator<StdContainerIterator<ContainerT>, FnT> operator|(ContainerT& c, Map<FnT>&& map) {
    return MapIterator(StdContainerIterator(c), std::move(map._fn));
}

template <typename ContainerT, typename FnT>
MapIterator<StdContainerConstIterator<ContainerT>, FnT> operator|(ContainerT const& c, Map<FnT>&& map) {
    return MapIterator(StdContainerConstIterator(c), std::move(map._fn));
}

struct VectorizeType {};
VectorizeType Vectorize;

template <typename IterT>
std::vector<typename IterT::Element> operator|(IterT&& iter, VectorizeType) {
    std::vector<typename IterT::Element> vec;
    for (auto&& elem : iter) {
        vec.emplace_back(std::forward<typename IterT::Element>(elem));
    }
    return vec;
}

template <typename ElementT>
struct AppendTo {
    std::vector<ElementT> _vec;
    explicit AppendTo(std::vector<ElementT>&& vec) : _vec(std::forward<std::vector<ElementT>>(vec)) { }
};

template <typename IterT>
std::vector<typename IterT::Element> operator|(IterT&& iter, AppendTo<typename IterT::Element>&& appendTo) {
    for (auto&& elem : iter) {
        appendTo._vec.emplace_back(std::forward<typename IterT::Element>(elem));
    }
    return std::move(appendTo._vec);
}
}
