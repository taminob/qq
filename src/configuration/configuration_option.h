#ifndef QQ_CONFIGURATION_OPTION_H
#define QQ_CONFIGURATION_OPTION_H

#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <utility>

enum class ConfigurationLayer : std::uint8_t {
    current, // highest available layer

    preset, // built-in fallback value
    file, // value read from config files
    commandLine, // value overwritten by CLI
    runtime, // value overwritten by process
};

template <typename T, typename Func>
concept IsCreateFunction = requires(Func func) {
    { func() } -> std::same_as<T>;
};
template <typename ValueType, typename Func>
// concept IsCombineFunction = requires(Func func) {
//     { func(std::declval<ValueType>(), std::declval<ValueType>()) } -> std::constructible_from<ValueType>;
concept IsCombineFunction = requires(Func func, ValueType previous_value, ValueType new_value) {
    { func(previous_value, new_value) } -> std::constructible_from<ValueType>;
};

template <typename T>
class ConfigurationOption {
public:
    using Layer = ConfigurationLayer;

    using Type = T;

public:
    template <typename U>
        requires std::is_convertible_v<U, T>
    explicit ConfigurationOption(U&& preset_value)
        : preset_value_ { std::forward<U>(preset_value) }
    {
    }

    /**
     * Check if a value for the given layer exists.
     */
    [[nodiscard]] bool has(Layer layer) const
    {
        if (auto index = layerIndex(layer)) {
            return layer_values_.at(*index).has_value();
        }
        // current and preset layers always exist
        return true;
    }

    /**
     * Get the combined value across all layers except the preset layer.
     * If no value is set, the value of the preset layer is returned instead.
     *
     * @param combine_function Function that combines the previous and next layers into a single value
     * @param initial_value Value that will be passed as the previous value in the first function call
     *
     * @note If the preset layer should be included, set initial_value to get(Layer::preset)
     */
    template <typename Func>
        requires IsCombineFunction<T, Func>
    T get(Func&& combine_function, T initial_value = T { }) const
    {
        bool no_layer_set { true };
        auto&& result = std::ranges::fold_left(layer_values_, std::move(initial_value), [&](auto current, auto next) {
            if (next.has_value()) {
                no_layer_set = false;
                return std::forward<Func>(combine_function)(current, *next);
            }
            return current;
        });
        if (no_layer_set) {
            return get(Layer::preset);
        }
        return result;
    }
    /**
     * Get value of given layer.
     * The layer must have a value, otherwise an exception will be raised.
     *
     * @note check with has() before using this function if a layer has a value
     */
    auto& get(this auto& self, Layer layer = Layer::current)
    {
        if (auto index = self.layerIndex(layer)) {
            return self.layer_values_.at(*index).value();
        }
        if (layer == Layer::current) {
            return self.get(self.currentLayer());
        }
        return self.preset_value_;
    }

    template <typename U, typename Func>
        requires IsCombineFunction<T, Func>
    bool add(Layer layer, U&& value, Func&& merge_function)
    {
        if (auto index = layerIndex(layer)) {
            auto prev_value = layer_values_.at(*index);
            if (prev_value) {
                prev_value = T { std::forward<Func>(merge_function)(std::move(*prev_value), std::forward<U>(value)) };
            } else {
                prev_value = T { std::forward<U>(value) };
            }
            return true;
        }
        if (layer == Layer::current) {
            add(currentLayer(), std::forward<U>(value), merge_function);
            return true;
        }
        // impossible to mutate read-only preset layer
        return false;
    }

    template <typename U>
    bool overwrite(Layer layer, U&& value)
    {
        if (auto index = layerIndex(layer)) {
            layer_values_.at(*index) = T { std::forward<U>(value) };
            return true;
        }
        if (layer == Layer::current) {
            overwrite(currentLayer(), std::forward<U>(value));
            return true;
        }
        // impossible to mutate read-only preset layer
        return false;
    }

    /**
     * Ensure no value is duplicate elements across the layers and delete
     * from upper layers if duplicates are detected.
     * This function is only available for container types.
     *
     * @return true if a modification was made
     */
    bool removeDuplicatesAcrossLayers()
        requires requires { typename T::value_type; }
    {
        bool is_any_modified { false };
        std::unordered_set<typename T::value_type> all_elements;
        auto cleanup_next_layer = [&](auto&& layer) {
            if (!has(layer)) {
                return;
            }
            bool is_modified { false };
            auto values = get(layer);
            for (auto it = values.begin(); it != values.end(); ++it) {
                auto is_inserted = all_elements.insert(*it).second;
                if (!is_inserted) {
                    it = values.erase(it);
                    is_modified = true;
                }
            }
            if (is_modified) {
                overwrite(layer, values);
                is_any_modified = true;
            }
        };
        cleanup_next_layer(Layer::preset);
        cleanup_next_layer(Layer::file);
        cleanup_next_layer(Layer::commandLine);
        cleanup_next_layer(Layer::runtime);
        return is_any_modified;
    }

    /**
     * Remove value of layer if it is equal to the value of the next lower set layer.
     */
    bool removeDuplicatesBetweenLayers() // TODO: better naming removeDuplicatesAcrossLayers vs removeDuplicatesBetweenLayers
    {
        auto last = get(Layer::preset);
        auto delete_if_equal = [&](auto&& next) {
            if (last == next) {
                clear(Layer::file);
            } else {
                last = next;
            }
        };
        if (has(Layer::file)) {
            delete_if_equal(get(Layer::file));
        }
        if (has(Layer::commandLine)) {
            delete_if_equal(get(Layer::commandLine));
        }
        if (has(Layer::runtime)) {
            delete_if_equal(get(Layer::runtime));
        }
    }

    /**
     * Execute given function for each layer.
     */
    template <typename Func>
    void forEachLayer(Func&& func)
    {
        auto call_func = [&](auto&& layer) {
            if (has(layer)) {
                func(layer, get(layer));
            }
        };
        call_func(Layer::preset);
        call_func(Layer::file);
        call_func(Layer::commandLine);
        call_func(Layer::runtime);
    }

    /**
     * Delete value from given layer.
     * If Layer::current is passed, all layers (except preset) will be deleted.
     */
    void clear(Layer layer)
    {
        if (auto index = layerIndex(layer)) {
            layer_values_.at(*index).clear();
        } else if (layer == Layer::current) {
            layer_values_.fill(std::nullopt);
        }
    }

private:
    [[nodiscard]] std::optional<std::size_t> layerIndex(Layer layer) const
    {
        switch (layer) {
        case Layer::current:
            [[fallthrough]];
        case Layer::preset:
            return std::nullopt;

        case Layer::file:
            return 0;
        case Layer::commandLine:
            return 1;
        case Layer::runtime:
            return 2;
        }
        return std::nullopt;
    }

    [[nodiscard]] Layer currentLayer() const
    {
        auto it = std::ranges::find_last_if(layer_values_, [](auto&& value) { return value.has_value(); }).begin();
        if (it == layer_values_.end()) {
            return Layer::preset;
        }
        auto index = std::distance(layer_values_.begin(), it);
        return static_cast<Layer>(std::to_underlying(Layer::file) + index);
    }

private:
    T preset_value_;
    std::array<std::optional<T>, 3> layer_values_;
};

#endif // QQ_CONFIGURATION_OPTION_H
