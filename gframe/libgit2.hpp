// Copyright (c) 2020 Dylam De La Torre <dyxel04@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
// Refer to the COPYING file included.

#ifndef LIBGIT2_HPP
#define LIBGIT2_HPP
#include <memory>
#include <tuple>
#include <type_traits>

#include <git2.h>

namespace Git
{

namespace Detail
{

// Template-based interface to remove all pointers from a type T
template <typename T>
struct Identity
{
	using type = T;
};

template<typename T>
struct RemoveAllPointers : std::conditional_t<std::is_pointer<T>::value,
	RemoveAllPointers<std::remove_pointer_t<T>>, Identity<T>>
{};

template<typename T>
using RemoveAllPointers_t = typename RemoveAllPointers<T>::type;

// Template-based interface to query argument Ith type from a function pointer
template<std::size_t I, typename Sig>
struct GetArg;

template<std::size_t I, typename Ret, typename... Args>
struct GetArg<I, Ret(*)(Args...)>
{
	using type = typename std::tuple_element<I, std::tuple<Args...>>::type;
};

template<std::size_t I, typename Sig>
using GetArg_t = typename GetArg<I, Sig>::type;

// Template-based interface to deduce a libgit object destructor from T
template<typename T>
struct DtorType;

#define DESTRUCTOR(objtype) template<>\
struct DtorType<objtype>\
{\
	static constexpr auto& value = objtype##_free;\
	using type = decltype(value);\
}

DESTRUCTOR(git_commit);
DESTRUCTOR(git_diff);
DESTRUCTOR(git_index);
DESTRUCTOR(git_object);
DESTRUCTOR(git_remote);
DESTRUCTOR(git_repository);
DESTRUCTOR(git_revwalk);
DESTRUCTOR(git_tree);

#undef DESTRUCTOR

template<typename T>
using DtorType_t = typename DtorType<T>::type;

template<typename T>
constexpr auto& DtorType_v = DtorType<T>::value;

} // namespace Detail

// Wrapper for any libgit2 object on a std::unique_ptr
template<typename T>
using UniqueObj = std::unique_ptr<T, Detail::DtorType_t<T>>;

// Check error value and throw in case there is an error
inline void Check(int error)
{
	if(error == 0)
		return;
#if LIBGIT2_VER_MAJOR>0 || LIBGIT2_VER_MINOR>27
	const auto err = git_error_last();
#else
	const auto err = giterr_last();
#endif
	throw std::runtime_error(err ? err->message : "Undefined error");
}

// Helper function to create RAII-managed objects for libgit C objects
template<typename Ctor,
	typename T = Detail::RemoveAllPointers_t<Detail::GetArg_t<0, Ctor>>,
	typename... Args>
UniqueObj<T> MakeUnique(Ctor ctor, Args&& ...args)
{
	T* obj;
	Check(ctor(&obj, std::forward<Args>(args)...));
	return UniqueObj<T>(std::move(obj), Detail::DtorType_v<T>);
}

} // namespace Git

#endif // LIBGIT2_HPP
