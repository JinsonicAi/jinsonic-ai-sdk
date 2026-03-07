#pragma once

#include <any>
#include <memory>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
// rmwei add .

namespace anyx {
// tiny helpers for std::any (C++17)
// usage
//   if (auto p = anyx::get<MyType>(any_or_sp)) { /* *p available */ }
//   anyx::visit<A,B>(a, [&](const A&){...}, [&](const B&){...}); // execute if hit one
//   auto v = anyx::view(entry->result); if (!v) return; entry->alarm_fn(*v.a);

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>

#include <cstdlib>
#include <memory>
inline std::string demangle(const char* n) {
	int									   status = 0;
	std::unique_ptr<char, void (*)(void*)> p(abi::__cxa_demangle(n, nullptr, nullptr, &status), std::free);
	return status == 0 && p ? std::string(p.get()) : std::string(n);
}
#else
inline std::string demangle(const char* n) { return n; }
#endif
inline std::string type_name(const std::any& a) {
	const auto& ti = a.type();
	return ti == typeid(void) ? "void" : demangle(ti.name());
}

// only allowed value type / std::shared_ptr<T> / std::monostate
template <class T>
struct is_allowed_storable
	: std::bool_constant<
		  !std::is_pointer_v<std::decay_t<T>> &&
		  !std::is_reference_v<T>> {};

// unified structure: any_make<T>(args...) -> std::shared_ptr<std::any>
// usage example:
//   auto a = anyx_util::any_make<MyStruct>(...);
//   auto b = anyx_util::any_make<std::shared_ptr<Foo>>(foo_sp);
//   auto n = anyx_util::any_make<std::monostate>();
template <class T, class... Args,
		  std::enable_if_t<is_allowed_storable<T>::value, int> = 0>
inline std::shared_ptr<std::any> any_make(Args&&... args) {
	using D = std::decay_t<T>;
	return std::make_shared<std::any>(std::in_place_type<D>, std::forward<Args>(args)...);
}

// prevent misuse: if someone tries to store a raw pointer reference,will report an error at compile time(friendly message)
template <class T, class... Args,
		  std::enable_if_t<!is_allowed_storable<T>::value, int> = 0>
inline std::shared_ptr<std::any> any_make(Args&&...) {
	static_assert(is_allowed_storable<T>::value,
				  "any_make<T>: it is prohibited to store raw pointers or references std::any，please change to value type or std::shared_ptr<T>。");
	return {};
}

inline std::shared_ptr<std::any> any_wrap(const std::any& a) {
	return std::make_shared<std::any>(a);
}
inline std::shared_ptr<std::any> any_wrap(std::any&& a) {
	return std::make_shared<std::any>(std::move(a));
}

template <class T>
inline std::shared_ptr<std::any> any_try_cast(const std::any& a) {
	if (auto p = std::any_cast<T>(&a)) {
		return std::make_shared<std::any>(*p);
	}
	return {};	// failure: return empty cooperate anyx::view(...) just judge if it s empty
}

// 1) get: zero exception acquisition(cannot return nullptr)
template <class T>
inline T* get(std::any& a) { return a.has_value() ? std::any_cast<T>(&a) : nullptr; }
template <class T>
inline const T* get(const std::any& a) { return a.has_value() ? std::any_cast<T>(&a) : nullptr; }
template <class T>
inline T* get(const std::shared_ptr<std::any>& sp) { return (sp && sp->has_value()) ? std::any_cast<T>(sp.get()) : nullptr; }
template <class T>
inline const T* get(const std::shared_ptr<const std::any>& sp) { return (sp && sp->has_value()) ? std::any_cast<const T>(sp.get()) : nullptr; }

// 2) visit:statement Ts...,hit one and call the corresponding callback(do not throw exceptions;return whether it hit)
template <class... Ts, class... Fs>
bool visit(std::any& a, Fs&&... fs) {
	static_assert(sizeof...(Ts) == sizeof...(Fs), "handlers != types");
	bool hit = false;
	// ((a.type() == typeid(Ts) ? (std::forward<Fs>(fs)(*std::any_cast<Ts>(&a)), hit = true) : void()), ...);
	((a.type() == typeid(Ts)
		  ? (static_cast<void>(std::forward<Fs>(fs)(*std::any_cast<const Ts>(&a))), hit = true, 0)
		  : 0),
	 ...);
	return hit;
}
template <class... Ts, class... Fs>
bool visit(const std::any& a, Fs&&... fs) {
	static_assert(sizeof...(Ts) == sizeof...(Fs), "handlers != types");
	bool hit = false;
	// ((a.type() == typeid(Ts) ? (std::forward<Fs>(fs)(*std::any_cast<const Ts>(&a)), hit = true) : void()), ...);
	((a.type() == typeid(Ts)
		  ? (static_cast<void>(std::forward<Fs>(fs)(*std::any_cast<const Ts>(&a))), hit = true, 0)
		  : 0),
	 ...);
	return hit;
}

// 3) view:take "shared_ptr<any> it may not have value" unify into a view that can be objectively verified
struct view_t {
	const std::any* a{};
	explicit		operator bool() const { return a && a->has_value(); }
	template <class T>
	const T* get() const { return a ? std::any_cast<T>(a) : nullptr; }
	template <class... Ts, class... Fs>
	bool visit(Fs&&... fs) const {
		if (!*this) return false;
		return anyx::visit<Ts...>(*a, std::forward<Fs>(fs)...);
	}
};
inline view_t view(const std::shared_ptr<std::any>& sp) {
	return view_t{sp && sp->has_value() ? sp.get() : nullptr};
}

}  // namespace anyx
