#include <iostream>
#include <type_traits>
#include <variant>

#undef USE_BOOST

#ifdef USE_BOOST
#include <boost/variant.hpp>
#endif

// TO_STD_BOOL
template<typename T>
struct to_std_bool : std::conditional_t<bool(T::value),
    std::true_type, std::false_type> {};

// IS_VARIANT
template<class T>
struct is_variant : std::false_type {};
template<class... T>
struct is_variant<std::variant<T...>> : std::true_type {};
#ifdef USE_BOOST
template<class... T>
struct is_variant<boost::variant<T...>> : std::true_type {};
#endif // USE_BOOST
template<class T>
constexpr bool is_variant_v = is_variant<T>::value;

// EXCLUSIVE_DISJUNCTION
namespace detail {

template<bool Prev, class... B>
struct _exclusive_disjunction : std::false_type {};
template<class B>
struct _exclusive_disjunction<false, B>
    : std::conditional_t<bool(B::value), std::true_type, std::false_type> {};
template<class B>
struct _exclusive_disjunction<true, B>
    : std::conditional_t<bool(B::value), std::false_type, std::true_type> {};
template<class B, class... Bn>
struct _exclusive_disjunction<false, B, Bn...>
    : _exclusive_disjunction<bool(B::value), Bn...> {};
template<class B, class... Bn>
struct _exclusive_disjunction<true, B, Bn...>
    : std::conditional_t<bool(B::value),
        std::false_type, _exclusive_disjunction<true, Bn...>> {};

}

template<class... B>
struct exclusive_disjunction : detail::_exclusive_disjunction<false, B...> {};

// NTH_TYPE
struct index_out_of_bounds_t {};
namespace detail {
    template<size_t N, class T1, class... T2>
    struct _nth_type : _nth_type<N-1, T2...> {};
    template<class T1, class... T2>
    struct _nth_type<0, T1, T2...> {
        using type = T1;
    };
    template<>
    struct _nth_type<0, index_out_of_bounds_t> {
        using type = index_out_of_bounds_t;
    };
} // namespace detail
template<size_t N, class... T>
struct nth_type : detail::_nth_type<N, T..., index_out_of_bounds_t> {};
template<size_t N, class... T>
using nth_type_t = typename nth_type<N, T...>::type;

// type definitions
template<class... T>
struct type_list_t {};
template<class... T>
struct type_stack_t {};

// TYPE_LIST
namespace type_list {
    template<class T>
    struct extract_parameter_list {};
    template<class... Tn>
    struct extract_parameter_list<std::variant<Tn...>> {
        using type = type_list_t<Tn...>;
    };
    template<class T>
    using extract_parameter_list_t = typename extract_parameter_list<T>::type;

    // empty list constant
    using empty_t = type_list_t<>;

    // push_back
    template<class TypeList, class T>
    struct push_back {};
    template<class T, class... Tn>
    struct push_back<type_list_t<Tn...>, T> {
        using type = type_list_t<Tn..., T>;
    };
    template<class TypeList, class T>
    using push_back_t = typename push_back<TypeList, T>::type;
} // namespace type_list

// TYPE_STACK
namespace type_stack {
    // emtpy stack constant
    using empty_t = type_stack_t<>;

    // is_empty
    template<class TypeStack>
    struct is_empty : std::true_type {};
    template<class T, class... Tn>
    struct is_empty<type_stack_t<T, Tn...>> : std::false_type {};
    template<class TypeStack>
    constexpr bool is_empty_v = is_empty<TypeStack>::value;

    // push
    template<class TypeStack, class T>
    struct push {};
    template<class T, class... Tn>
    struct push<type_stack_t<Tn...>, T> {
        using type = type_stack_t<Tn..., T>;
    };
    template<class TypeStack, class T>
    using push_t = typename push<TypeStack, T>::type;

    // top
    template<class TypeStack>
    struct top {};
    template<class... Tn>
    struct top<type_stack_t<Tn...>> : nth_type<sizeof...(Tn)-1, Tn...> {};
    template<class TypeStack>
    using top_t = typename top<TypeStack>::type;

    // pop
    namespace detail {
        template<class Rebuild, class... Tn>
        struct _pop_impl {};
        template<class Rebuild, class T, class... Tn>
        struct _pop_impl<Rebuild, T, Tn...>
            : _pop_impl<push_t<Rebuild, T>, Tn...> {};
        template<class Rebuild, class T>
        struct _pop_impl<Rebuild, T> {
            using type = Rebuild;
        };
    }
    template<class TypeStack>
        struct pop {};
    template<class... Tn>
    struct pop<type_stack_t<Tn...>> : detail::_pop_impl<empty_t, Tn...> {};
    template<class TypeStack>
    using pop_t = typename pop<TypeStack>::type;
} // namespace type_stack

// create_variant function

namespace detail {
    template<class ValueType, class Variant, bool _Unused = false>
    struct _variant_type_path;

    template<class Current, class T,
        bool IsSame = std::is_same_v<Current, T>,
        bool IsVariant = is_variant_v<Current>>
    struct _variant_unique_check {};
    template<class Current, class T, bool IsVariant>
    struct _variant_unique_check<Current, T, true, IsVariant> : std::true_type {
        using type = type_stack::empty_t;
    };
    template<class Current, class T>
    struct _variant_unique_check<Current, T, false, false> : std::false_type {};
    template<class Current, class T>
    struct _variant_unique_check<Current, T, false, true>
        : _variant_type_path<Current, T> {};

    template<typename T, typename... Tn>
    struct _variant_type_path_find
        : std::conditional_t<bool(T::value), T,
            _variant_type_path_find<Tn...>> {};
    template<typename T>
    struct _variant_type_path_find<T> : T {};

    template<class Xor, class... CheckResults>
    struct _variant_type_path_select_impl : std::conditional_t<bool(Xor::value),
        _variant_type_path_find<CheckResults...>, std::false_type> {};
    template<class... CheckResults>
    struct _variant_type_path_select : _variant_type_path_select_impl<
            exclusive_disjunction<CheckResults...>, CheckResults...> {};

    template<class Variant, class Selected, bool Valid = bool(Selected::value)>
    struct _variant_type_path_create_result {};
    template<class Variant, class Selected>
    struct _variant_type_path_create_result<Variant, Selected, true>
        : std::true_type {
        using type = type_stack::push_t<typename Selected::type, Variant>;
    };
    template<class Variant, class Selected>
    struct _variant_type_path_create_result<Variant, Selected, false>
        : std::false_type {
        using type = type_stack::empty_t;
    };

    template<class ValueType, class Variant, class TypeList>
    struct _variant_type_path_impl {};
    template<class ValueType, class Variant, class... Types>
    struct _variant_type_path_impl<ValueType, Variant, type_list_t<Types...>>
        : _variant_type_path_create_result<Variant,
            _variant_type_path_select<
                _variant_unique_check<Types, ValueType>...>> {};
    template<class Variant, class ValueType>
    struct _variant_type_path<Variant, ValueType, false> 
        : _variant_type_path_impl<ValueType, Variant,
            type_list::extract_parameter_list_t<Variant>> {};
    template<class Variant, class ValueType>
    using _variant_type_path_t =
        typename _variant_type_path<Variant, ValueType>::type;
    template<class Variant, class ValueType>
    constexpr bool _variant_type_path_v =
        _variant_type_path<Variant, ValueType>::value;
} // namespace detail

namespace detail {
    template<class TypeStack, class T>
    auto _create_variant(T value) {
        if constexpr(std::is_same_v<TypeStack, type_stack::empty_t>)
            return value;
        else {
            type_stack::top_t<TypeStack> result;
            result = _create_variant<type_stack::pop_t<TypeStack>>(value);
            return result;
        }
    }
} // namespace detail
template<class Target, class T>
Target create_variant(T value) {
    using Data = detail::_variant_type_path<Target, T>;
    static_assert(is_variant_v<Target>, "Target is not a variant type!");
    static_assert(bool(Data::value),
        "The given Target type cannot contain an instance "
        "of T or the position of type T within the variant is ambiguous!");
    return detail::_create_variant<typename Data::type>(value);
}

