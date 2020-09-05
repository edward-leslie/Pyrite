#pragma once

#include <iterator>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

// TODO: After moving to C++20, we should probably replace this functionality with the ranges library.
namespace py {
struct Bounds {
    size_t lo;
    std::optional<size_t> hi;

    Bounds(size_t lo, size_t hi) : lo(lo), hi(hi) { }
    Bounds(size_t lo, std::optional<size_t> hi) : lo(lo), hi(hi) { }
    explicit Bounds(size_t lo) : lo(lo), hi(std::nullopt) { }
    explicit Bounds(std::optional<size_t> hi) : lo(0), hi(hi) { }
};

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
    std::optional<size_t> _upperBound;
public:
    using Type = StdContainerIterator<ContainerT>;
    using Element = typename std::iterator_traits<Iterator>::reference;

    explicit StdContainerIterator(Container& c) : _iter(c.begin()), _end(c.end()), _upperBound(c.size()) { }
    StdContainerIterator(Iterator& begin, Iterator& end) : _iter(begin), _end(end), _upperBound(std::nullopt) { }

    StdContainerIterator(Type const&) = delete;
    StdContainerIterator(Type&&) noexcept = default;
    Type& operator=(Type const&) = delete;
    Type& operator=(Type&&) noexcept = default;

    Element operator*() const { return *_iter; }
    operator bool() { return _iter != _end; }
    void operator++() { ++_iter; }

    [[nodiscard]] Bounds SizeHint() const { return Bounds(_upperBound); }
};

template <typename ContainerT>
class StdContainerConstIterator : public StdIteratorImpl<StdContainerConstIterator<ContainerT>> {
private:
    using Container = ContainerT;
    using Iterator = typename ContainerT::const_iterator;

    Iterator _iter;
    Iterator _end;
    std::optional<size_t> _upperBound;
public:
    using Type = StdContainerConstIterator<ContainerT>;
    using Element = typename std::iterator_traits<Iterator>::reference;

    explicit StdContainerConstIterator(Container& c) : _iter(c.begin()), _end(c.end()), _upperBound(c.size()) { }
    StdContainerConstIterator(Iterator& begin, Iterator& end) : _iter(begin), _end(end), _upperBound(std::nullopt) { }

    StdContainerConstIterator(Type const&) = delete;
    StdContainerConstIterator(Type&&) noexcept = default;
    Type& operator=(Type const&) = delete;
    Type& operator=(Type&&) noexcept = default;

    Element operator*() const { return *_iter; }
    operator bool() { return _iter != _end; }
    void operator++() { ++_iter; }

    [[nodiscard]] Bounds SizeHint() const { return Bounds(_upperBound); }
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

    [[nodiscard]] Bounds SizeHint() const { return _iter.SizeHint(); }
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
    return MapIterator(StdContainerIterator(c), std::forward<FnT>(map._fn));
}

template <typename ContainerT, typename FnT>
MapIterator<StdContainerConstIterator<ContainerT>, FnT> operator|(ContainerT const& c, Map<FnT>&& map) {
    return MapIterator(StdContainerConstIterator(c), std::forward<FnT>(map._fn));
}

struct VectorizeType {};
VectorizeType Vectorize;

template <typename IterT>
std::vector<typename IterT::Element> operator|(IterT&& iter, VectorizeType) {
    std::vector<typename IterT::Element> vec;
    auto [lowerBound, upperBound] = iter.SizeHint();
    if (upperBound) {
        vec.reserve(*upperBound);
    }

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
    std::vector<typename IterT::Element>& vec = appendTo._vec;
    auto [lowerBound, upperBound] = iter.SizeHint();
    if (upperBound) {
        vec.reserve(*upperBound + vec.size());
    }

    for (auto&& elem : iter) {
        vec.emplace_back(std::forward<typename IterT::Element>(elem));
    }
    return std::move(vec);
}
}
