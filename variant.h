#include <iostream>
#include <variant>
#include <new>
//
template<typename... Types>
union VariadicUnion {
    template<size_t N>
    bool& get() {
        static_assert(N < 0, "N exceeds number of alternatives in variant");
        bool b = false; return b;
    }

    template<typename T>
    void put(const T&) {
        static_assert(std::is_same_v<T, void>,"N exceeds number of alternatives in variant" );
    } 

    template<typename T>
    void put(T&&) {
        static_assert(std::is_same_v<T, void>, "N exceeds number of alternatives in variant");
    }

    template<typename T>
    void assign(const T&) {
        static_assert(std::is_same_v<T, void>, "N exceeds number of alternatives in variant");
    }

    template<typename T>
    void assign(T&&) {
        static_assert(std::is_same_v<T, void>, "N exceeds number of alternatives in variant");
    }
};

template<typename Head, typename... Tail>
union VariadicUnion<Head, Tail...> {
    Head head;
    VariadicUnion<Tail...> tail;

    VariadicUnion() {}
    ~VariadicUnion() {}
    VariadicUnion(const VariadicUnion&) {}
    VariadicUnion(VariadicUnion&&) {}
    VariadicUnion& operator=(const VariadicUnion&) {return *this;}
    VariadicUnion& operator=(VariadicUnion&&) {return *this;}

    template<size_t N>
    auto& get() {
        if constexpr (N == 0) {
            return head;
        }
        else {
            return tail.template get<N-1>();
        }
    }

    template<size_t N>
    const auto& get() const {
        if constexpr (N == 0) {
            return head;
        }
        else {
            return tail.template get<N-1>();
        }
    } 

    template<typename T>
    void put(const T& value) {
        if constexpr (std::is_same_v<T, Head>) {
            new(std::launder(&head)) T(value);
        }
        else {
            tail.template put<T>(value);
        }
    }

    template<typename T>
    void put(T&& value) {
        if constexpr (std::is_same_v<T, Head>) {
            new(std::launder(const_cast<std::remove_cv_t<T>*>(&head))) T(std::move(value));
        }
        else {
            tail.template put<T>(std::move(value));
        }
    }

    void put() {
        new(std::launder(&head)) Head();
    }

    template<typename T>
    void assign(const T& value) {
        if constexpr (std::is_same_v<T, Head>) {
            head = value;
        }
        else {
            tail.template assign<T>(value);
        } 
    }

    template<typename T>
    void assign(T&& value) {
        if constexpr (std::is_same_v<T, Head>) {
            head = std::move(value);
        }
        else {
            tail.template assign<T>(std::move(value));
        } 
    }

    template<size_t N, typename... Args>
    auto& emplace(Args&&... args) {
        if constexpr (N == 0) {
            new (std::launder(const_cast<std::remove_cv_t<Head>*>(&head))) Head(std::forward<Args>(args)...);
            return head;
        }
        else {
            return tail.template emplace<N-1, Args...>(std::forward<Args>(args)...);
        }
    }
    
    template<typename T>
    void destroy() {
       if constexpr (std::is_same_v<T, Head>) {
           std::launder(&head)->~T();
       }
       else {
           tail.template destroy<T>();
       }
    }
};

template<size_t N, typename T, typename Head, typename... Tail>
struct index_by_type_impl {
    static constexpr size_t value = std::is_same_v<T, Head> 
        ? N
        : index_by_type_impl<N+1, T, Tail...>::value;
};

template<size_t N, typename T, typename Last>
struct index_by_type_impl<N, T, Last> {
    static constexpr size_t value = std::is_same_v<T, Last> ? N : N+1;
};

template<typename T, typename... Types>
static constexpr size_t index_by_type = index_by_type_impl<0, T, Types...>::value;

template<typename T, typename... Types>
class VariantAlternative;

template<typename... Types>
class Variant;

template<typename T, typename... Types>
T& get(Variant<Types...>& v);

template<typename T, typename... Types>
const T& get(const Variant<Types...>& v);

template<typename T, typename... Types>
T&& get(Variant<Types...>&& v);

template<size_t N, typename... Types>
auto& get(Variant<Types...>& v);

template<size_t N, typename... Types>
const auto& get(const Variant<Types...>& v);

template<size_t N, typename... Types>
auto&& get(Variant<Types...>&& v);

template<typename... Types>
struct VariantStorage {
    VariadicUnion<Types...> storage;
    size_t i;
    bool valueless;
};

template<typename T, typename... Types>
class VariantAlternative {

public:
    using Derived = Variant<Types...>;
    
    static constexpr size_t idx = index_by_type<T, Types...>;

    VariantAlternative() {}
    ~VariantAlternative() {}

    template<typename Tp, typename = std::enable_if_t<std::is_constructible_v<T, Tp>>>
    VariantAlternative(Tp&& value) {
        auto ptr = static_cast<Derived*>(this);
        ptr->storage.template put<T>(std::forward<Tp>(value));
        ptr->i = idx;
        ptr->valueless = false;
    } 

    VariantAlternative(T&& value) {
        auto ptr = static_cast<Derived*>(this);
        ptr->storage.template put<T>(std::forward<T>(value));
        ptr->i = idx;
        ptr->valueless = false;
    } 

    template<typename Tp, typename = std::enable_if_t<std::is_constructible_v<T, Tp>>>
    VariantAlternative& operator=(Tp&& value) {
        auto ptr = static_cast<Derived*>(this);
        if (index_by_type<T, Types...> == ptr->i) {
            ptr->storage.template assign<T>(std::forward<Tp>(value));
            ptr->valueless = false;
        }
        else {
            try { 
                ptr->destruct();
                ptr->storage.template put<T>(std::forward<Tp>(value));
                ptr->i = idx;
                ptr->valueless = false;
            } catch(...) {
                ptr->valueless = true;
            }
        }
        return *this; 
    }

    VariantAlternative& operator=(T&& value) {
        auto ptr = static_cast<Derived*>(this);
        if (index_by_type<T, Types...> == ptr->i) {
            ptr->storage.template assign<T>(std::forward<T>(value));
        }
        else {
            try {
                ptr->destruct();
                ptr->storage.template put<T>(std::forward<T>(value));
                ptr->i = idx;
                ptr->valueless = false;
            } catch(...) {
                ptr->valueless = true;
            }
        }
        return *this; 
    }

    VariantAlternative(const VariantAlternative& other) {
        auto other_ptr = static_cast<const Derived*>(&other);
        if (other_ptr->i == idx) {
            auto ptr = static_cast<Derived*>(this);
            try {
                ptr->storage.template put<T>(other_ptr->storage.template get<idx>());
                ptr->i = other_ptr->i;
                ptr->valueless = false;
            } catch(...) {
                ptr->valueless = true;
            }
        }
    }

    VariantAlternative& operator=(const VariantAlternative& other) {
        auto other_ptr = static_cast<const Derived*>(&other);
        if (other_ptr->i == idx) {
            auto ptr = static_cast<Derived*>(this);
            try {
                ptr->destruct();
                ptr->storage.template put<T>(other_ptr->storage.template get<idx>());
                ptr->i = other_ptr->i;
                ptr->valueless = false;
                return *this;
            } catch(...) {
                ptr->valueless = true;
            }
        }
        return *this;
    }

    VariantAlternative(VariantAlternative&& other) {
        auto other_ptr = static_cast<Derived*>(&other);
        if (other_ptr->i == idx) {
            auto ptr = static_cast<Derived*>(this);
            ptr->storage.template put<T>(std::move(other_ptr->storage.template get<idx>()));
            ptr->i = other_ptr->i;
            ptr->valueless = false; 
            other_ptr->valueless = true;
        }
    }

    VariantAlternative& operator=(VariantAlternative&& other) {
        auto other_ptr = static_cast<Derived*>(&other);
        if (other_ptr->i == idx) {
            auto ptr = static_cast<Derived*>(this);
            try {
                ptr->destruct();
                ptr->storage.template put<T>(std::move(other_ptr->storage.template get<idx>()));
                ptr->i = other_ptr->i;
                ptr->valueless = false;
                other_ptr->valueless = true;
            } catch(...) {
                ptr->valueless = true;
            }
        }
        return *this;
    }

    void destroy() {
        auto ptr = static_cast<Derived*>(this); 
        if (ptr->i == idx) {
            ptr->storage.template destroy<T>();
        }
    }
};

template<typename... Types>
class VariantAlternative<double, Types...> {
public:
    using Derived = Variant<Types...>;
    
    static constexpr size_t idx = index_by_type<double, Types...>;

    VariantAlternative() {}
    ~VariantAlternative() {}

    VariantAlternative(double&& value) {
        auto ptr = static_cast<Derived*>(this);
        ptr->storage.template put<double>(std::move(value));
        ptr->i = idx;
        ptr->valueless = false;
    }

    VariantAlternative& operator=(double&& value) {
        auto ptr = static_cast<Derived*>(this);
        if (index_by_type<double, Types...> == ptr->i) {
            ptr->storage.template assign<double>(std::move(value));
        }
        else {
            try {
                ptr->destruct();
                ptr->storage.template put<double>(std::move(value));
                ptr->i = idx;
                ptr->valueless = false;
            }
            catch(...) {
                ptr->valueless = true;
            }
        }
        return *this; 
    }

    VariantAlternative(float&& value) {
        auto ptr = static_cast<Derived*>(this);
        ptr->storage.template put<double>(std::move(value));
        ptr->i = idx;
        ptr->valueless = false; 
    }

    VariantAlternative& operator=(float&& value) {
        auto ptr = static_cast<Derived*>(this);
        if (index_by_type<double, Types...> == ptr->i) {
            ptr->storage.template assign<double>(std::move(value));
        }
        else {
            try {
                ptr->destruct();
                ptr->storage.template put<double>(std::move(value));
                ptr->i = idx;
                ptr->valueless = false;
            } catch(...) {
                ptr->valueless = true;
            }
        }
        return *this; 
    }

    VariantAlternative(const VariantAlternative& other) {
        auto other_ptr = static_cast<const Derived*>(&other);
        if (other_ptr->i == idx) {
            auto ptr = static_cast<Derived*>(this);
            ptr->storage.template put<double>(other_ptr->storage.template get<idx>());
            ptr->i = other_ptr->i;
            ptr->valueless = false; 
        }
    }

    VariantAlternative& operator=(const VariantAlternative& other) {
        auto other_ptr = static_cast<const Derived*>(&other);
        if (other_ptr->i == idx) {
            auto ptr = static_cast<Derived*>(this);
            try {
                ptr->destruct();
                ptr->storage.template put<double>(other_ptr->storage.template get<idx>());
                ptr->i = other_ptr->i;
                ptr->valueless = false;
            } catch(...) {
                ptr->valueless = true;
            }
        }
        return *this;
    }

    VariantAlternative(VariantAlternative&& other) {
        auto other_ptr = static_cast<Derived*>(&other);
        if (other_ptr->i == idx) {
            auto ptr = static_cast<Derived*>(this);
            ptr->storage.template put<double>(std::move(other_ptr->storage.template get<idx>()));
            ptr->i = other_ptr->i;
            ptr->valueless = false;
            other_ptr->valueless = true;
        }
    }

    VariantAlternative& operator=(VariantAlternative&& other) {
        auto other_ptr = static_cast<Derived*>(&other);
        if (other_ptr->i == idx) {
            auto ptr = static_cast<Derived*>(this);
            try {
                ptr->destruct();
                ptr->storage.template put<double>(std::move(other_ptr->storage.template get<idx>()));
                ptr->i = other_ptr->i;
                ptr->valueless = false;
                other_ptr->valueless = true; 
            }
            catch(...) {
                ptr->valueless = true;
            }
        }
        return *this;
    }
    
    void destroy() {
        auto ptr = static_cast<Derived*>(this); 
        if (ptr->i == idx) {
            ptr->storage.template destroy<double>();
        }
    }
};

template<typename... Types>
class Variant
    : public VariantStorage<Types...>
    , public VariantAlternative<Types, Types...>...
{
private: 
    template<size_t N, typename... Ts>
    friend auto& get(Variant<Ts...>& v);  

    template<size_t N, typename... Ts>
    friend const auto& get(const Variant<Ts...>& v);

    template<typename T, typename... Ts>
    friend class VariantAlternative; 

public:
    using VariantAlternative<Types, Types...>::VariantAlternative...;
    using ::VariantAlternative<Types, Types...>::operator=...;
    using ::VariantAlternative<Types, Types...>::destroy...;

    Variant() {
        this->storage.put();    
        this->i = 0;
    } 
    
    Variant(const Variant&) = default;
    Variant(Variant&&) = default;
    Variant& operator=(const Variant&) = default;
    Variant& operator=(Variant&&) = default;

    template<typename T, typename... Args>
    T& emplace(Args&&... args) {
        return emplace<index_by_type<T, Types...>, Args...>(std::forward<Args>(args)...);
    }

    template<typename T, typename U, typename... Args>
    T& emplace(std::initializer_list<U> il, Args&&... args ) {
        return emplace<index_by_type<T, Types...>, Args...>(il, std::forward<Args>(args)...);
    }

    template<size_t N, typename... Args>
    auto& emplace(Args&&... args) {
        destruct();
        this->i = N;
        try {
            this->valueless = false;
            return this->storage.template emplace<N, Args...>(std::forward<Args>(args)...);
        } catch(...) {
            this->valueless = true;
            throw 1;
        }
    }
    
    void destruct() {
        (::VariantAlternative<Types, Types...>::destroy(), ...);
    }

    constexpr size_t index() const {
        return this->i;
    }

    bool valueless_by_exception() const {
        return this->valueless;        
    }

    ~Variant() {
        destruct();
    }
};

template<typename T, typename... Types>
bool holds_alternative(const Variant<Types...>& v) {
    return index_by_type<T, Types...> == v.index();
}

template<size_t N, typename... Types>
auto& get(Variant<Types...>& v) {
    if (v.index() == N) {
        return v.storage.template get<N>();
    }
    throw std::bad_variant_access();
}

template<size_t N, typename... Types>
const auto& get(const Variant<Types...>& v) {
    if (v.index() == N) {
        return v.storage.template get<N>();
    }
    throw std::bad_variant_access();
}

template<size_t N, typename... Types>
auto&& get(Variant<Types...>&& v) {
    if (v.index() == N) {
        return v.storage.template get<N>();
    }
    throw std::bad_variant_access();
}

template<typename T, typename... Types>
T& get(Variant<Types...>& v) {
    return get<index_by_type<T, Types...>>(v); 
}

template<typename T, typename... Types>
const T& get(const Variant<Types...>& v) {
    return get<index_by_type<T, Types...>>(v); 
}

template<typename T, typename... Types>
T&& get(Variant<Types...>&& v) {
    return get<index_by_type<T, Types...>>(v); 
}
