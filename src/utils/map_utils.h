#ifndef QQ_UTILS_MAP_UTILS_H
#define QQ_UTILS_MAP_UTILS_H

template <typename Map>
auto get(Map&& map, auto&& key, auto&& default_value)
{
    auto it = map.find(key);
    if (it == map.end()) {
        return static_cast<decltype(it->second)>(default_value);
    }
    return it->second;
}

template <typename Multimap, typename MatchFunction>
bool eraseMultimapElement(Multimap& multimap, auto&& key, MatchFunction&& is_match)
{
    auto [begin, end] = multimap.equal_range(key);
    for (auto it = begin; it != end; ++it) {
        if (is_match(it->second)) {
            multimap.erase(it);
            return true;
        }
    }
    return false;
}

#endif // QQ_UTILS_MAP_UTILS_H
