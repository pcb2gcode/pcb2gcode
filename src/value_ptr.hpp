// Copyright 2017-2018 by Tom Clunie
// https://github.com/clunietp/value_ptr
// Distributed under the Boost Software License, Version 1.0.
//    (See http://www.boost.org/LICENSE_1_0.txt)

#ifndef SMART_PTR_VALUE_PTR
#define SMART_PTR_VALUE_PTR

#include <memory>		// std::unique_ptr
#include <functional>	// std::less
#include <cassert>		// assert

#if defined( _MSC_VER)

#if (_MSC_VER >= 1915)	// constexpr tested/working _MSC_VER 1915 (vs17 15.8)
#define VALUE_PTR_CONSTEXPR constexpr
#else	// msvc 15 bug prevents constexpr in some cases
#define VALUE_PTR_CONSTEXPR
#endif

//	https://blogs.msdn.microsoft.com/vcblog/2016/03/30/optimizing-the-layout-of-empty-base-classes-in-vs2015-update-2-3/
#define VALUE_PTR_USE_EMPTY_BASE_OPTIMIZATION __declspec(empty_bases)	// requires vs2015 update 2 or later.  still needed vs2017

#else
#define VALUE_PTR_CONSTEXPR constexpr
#define VALUE_PTR_USE_EMPTY_BASE_OPTIMIZATION
#endif

namespace smart_ptr {

	namespace detail {

		// has clone() method detection
		template<class T, class = void> struct has_clone : std::false_type {};
		template<class T> struct has_clone<T, decltype(void(std::declval<T>().clone()))> : std::true_type {};

		// Returns flag if test passes (false==slicing is probable)
		// T==base pointer, U==derived/supplied pointer
		template <typename T, typename U, bool IsDefaultCopier>
		struct slice_test : std::integral_constant<bool,
			std::is_same<T, U>::value	// if U==T, no need to check for slicing
			|| std::is_same<std::nullptr_t, U>::value	// nullptr is fine
			|| !IsDefaultCopier	// user provided cloner, assume they're handling it
			|| has_clone<typename std::remove_pointer<U>::type>::value	// using default cloner, clone method must exist in U
		>::type
		{};

		// default copier/cloner, analogous to std::default_delete
		template <typename T>
		struct default_copy {
		private:
			struct _clone_tag {};
			struct _copy_tag {};
			T* operator()(const T* what, _clone_tag) const { return what->clone(); }
			T* operator()(const T* what, _copy_tag) const { return new T(*what); }
		public:
			T* operator()(const T* what) const {	// copy operator
				if (!what)
					return nullptr;
				return this->operator()(what, typename std::conditional<detail::has_clone<T>::value, _clone_tag, _copy_tag>::type());	// tag dispatch on has_clone
			}	//
		};	// default_copy

		// ptr_data:  holds pointer, deleter, copier
		//	pointer and deleter held in unique_ptr member, this struct is derived from copier to minimize overall footprint
		//	uses EBCO to solve sizeof(value_ptr<T>) == sizeof(T*) problem
		template <typename T, typename Deleter, typename Copier>
			struct
			VALUE_PTR_USE_EMPTY_BASE_OPTIMIZATION
			ptr_data
			: public Copier
		{

			using unique_ptr_type = std::unique_ptr<T, Deleter>;
			using pointer = typename unique_ptr_type::pointer;
			using deleter_type = typename unique_ptr_type::deleter_type;
			using copier_type = Copier;

			unique_ptr_type uptr;

			ptr_data() = default;

			template <typename Dx, typename Cx>
			constexpr ptr_data( T* px, Dx&& dx, Cx&& cx )
				: copier_type( std::forward<Cx>(cx) )
				, uptr(px, std::forward<Dx>(dx))
			{}

			ptr_data( ptr_data&& ) = default;
			ptr_data& operator=( ptr_data&& ) = default;

			constexpr ptr_data( const ptr_data& that )
				: ptr_data( that.clone() )
			{}

			ptr_data& operator=( const ptr_data& that ) {
				if ( this != &that )
					*this = that.clone();
				return *this;
			}

			// get_copier, analogous to std::unique_ptr<T>::get_deleter()
			copier_type& get_copier() noexcept { return *this; }

			// get_copier, analogous to std::unique_ptr<T>::get_deleter()
			const copier_type& get_copier() const noexcept { return *this; }

			ptr_data clone() const {
				// get a copier, use it to clone ptr, construct/return a ptr_data
				return{
					this->get_copier()(this->uptr.get())
					, this->uptr.get_deleter()
					, this->get_copier()
				};
			}

		};	// ptr_data
	}	// detail

	template <typename T
		, typename Deleter = std::default_delete<T>
		, typename Copier = detail::default_copy<T>
	>
	struct value_ptr {

		using deleter_type = Deleter;
		using copier_type = Copier;
		using _data_type = detail::ptr_data<T, deleter_type, copier_type>;

		using unique_ptr_type = typename _data_type::unique_ptr_type;
		using element_type = T;
		using pointer = typename _data_type::pointer;
		using reference = typename std::add_lvalue_reference<element_type>::type;

		_data_type _data;

		// construct with pointer
		template <typename Px, typename Dx = Deleter, typename Cx = Copier, typename = typename std::enable_if<std::is_convertible<Px, pointer>::value>::type>
		VALUE_PTR_CONSTEXPR
			value_ptr(Px px, Dx&& dx = {}, Cx&& cx = {} )
			: _data(
				std::forward<Px>(px)
				, std::forward<Dx>(dx)
				, std::forward<Cx>(cx)
			)
		{
			static_assert(
				detail::slice_test<pointer, Px, std::is_convertible<detail::default_copy<T>, Copier>::value>::value
				, "value_ptr; clone() method not detected and not using custom copier; slicing may occur"
				);
		}

		// construct from unique_ptr, copier
		VALUE_PTR_CONSTEXPR
			value_ptr( std::unique_ptr<T, Deleter> uptr, Copier copier = {})
			: value_ptr(uptr.release(), std::move( uptr.get_deleter() ), std::move(copier))
		{}

		// std::nullptr_t, default ctor
		explicit
			VALUE_PTR_CONSTEXPR
			value_ptr(std::nullptr_t = nullptr)
			: value_ptr(nullptr, Deleter(), Copier())
		{}

		// return unique_ptr, ref qualified
		const unique_ptr_type& uptr() const & noexcept {
			return this->_data.uptr;
		}

		// return unique_ptr, ref qualified
		unique_ptr_type& uptr() & noexcept {
			return this->_data.uptr;
		}

		// conversion to unique_ptr, ref qualified
		operator unique_ptr_type const&() const & noexcept {
			return this->uptr();
		}

		// conversion to unique_ptr, ref qualified
		operator unique_ptr_type& () & noexcept {
			return this->uptr();
		}

		deleter_type& get_deleter() noexcept { return this->uptr().get_deleter(); }
		const deleter_type& get_deleter() const noexcept { return this->uptr().get_deleter(); }

		copier_type& get_copier() noexcept { return this->_data.get_copier(); }
		const copier_type& get_copier() const noexcept { return this->_data.get_copier(); }

		// get pointer
		pointer get() const noexcept { return this->uptr().get(); }

		// reset pointer to compatible type
		template <typename Px, typename = typename std::enable_if<std::is_convertible<Px, pointer>::value>::type>
		void reset(Px px) {

			static_assert(
				detail::slice_test<pointer, Px, std::is_same<detail::default_copy<T>, Copier>::value>::value
				, "value_ptr; clone() method not detected and not using custom copier; slicing may occur"
				);

			*this = value_ptr(std::forward<Px>(px), std::move( this->get_deleter() ), std::move( this->get_copier() ) );
		}

		// reset pointer
		void reset() { this->reset(nullptr); }

		// release pointer
		pointer release() noexcept {
			return this->uptr().release();
		}	// release

		// return flag if has pointer
		explicit operator bool() const noexcept {
			return this->get() != nullptr;
		}

		// return reference to T, UB if null
		reference operator*() const noexcept { return *this->get(); }

		// return pointer to T
		pointer operator-> () const noexcept { return this->get(); }

		// swap with other value_ptr
		void swap(value_ptr& that) { std::swap(this->_data, that._data); }

	};	// value_ptr


	// non-member swap
	template <class T1, class D1, class C1, class T2, class D2, class C2> void swap( value_ptr<T1, D1, C1>& x, value_ptr<T2, D2, C2>& y ) { x.swap( y ); }

	// non-member operators, based on https://en.cppreference.com/w/cpp/memory/unique_ptr/operator_cmp
	template <class T1, class D1, class C1, class T2, class D2, class C2> bool operator == ( const value_ptr<T1, D1, C1>& x, const value_ptr<T2, D2, C2>& y ) { return x.get() == y.get(); }
	template<class T1, class D1, class C1, class T2, class D2, class C2> bool operator != ( const value_ptr<T1, D1, C1>& x, const value_ptr<T2, D2, C2>& y ) { return x.get() != y.get(); }
	template<class T1, class D1, class C1, class T2, class D2, class C2> bool operator < ( const value_ptr<T1, D1, C1>& x, const value_ptr<T2, D2, C2>& y ) {
		using common_type = typename std::common_type<typename value_ptr<T1, D1, C1>::pointer, typename value_ptr<T2, D2, C2>::pointer>::type;
		return std::less<common_type>()( x.get(), y.get() );
	}
	template<class T1, class D1, class C1, class T2, class D2, class C2> bool operator <= ( const value_ptr<T1, D1, C1>& x, const value_ptr<T2, D2, C2>& y ) { return !( y < x ); }
	template<class T1, class D1, class C1, class T2, class D2, class C2> bool operator > ( const value_ptr<T1, D1, C1>& x, const value_ptr<T2, D2, C2>& y ) { return y < x; }
	template<class T1, class D1, class C1, class T2, class D2, class C2> bool operator >= ( const value_ptr<T1, D1, C1>& x, const value_ptr<T2, D2, C2>& y ) { return !( x < y ); }

	template <class T, class D, class C> bool operator == ( const value_ptr<T, D, C>& x, std::nullptr_t ) noexcept { return !x; }
	template <class T, class D, class C> bool operator == (std::nullptr_t, const value_ptr<T, D, C>& y) noexcept { return !y; }
	template <class T, class D, class C> bool operator != (const value_ptr<T, D, C>& x, std::nullptr_t) noexcept { return (bool)x; }
	template <class T, class D, class C> bool operator != (std::nullptr_t, const value_ptr<T, D, C>& y) noexcept { return (bool)y; }

	template <class T, class D, class C> bool operator < (const value_ptr<T, D, C>& x, std::nullptr_t) { return std::less<typename value_ptr<T, D, C>::pointer>()(x.get(), nullptr); }
	template <class T, class D, class C> bool operator < (std::nullptr_t, const value_ptr<T, D, C>& y) { return std::less<typename value_ptr<T, D, C>::pointer>()(nullptr, y.get()); }

	template <class T, class D, class C> bool operator <= (const value_ptr<T, D, C>& x, std::nullptr_t) { return !(nullptr < x); }
	template <class T, class D, class C> bool operator <= (std::nullptr_t, const value_ptr<T, D, C>& y) { return !(y < nullptr); }

	template <class T, class D, class C> bool operator > (const value_ptr<T, D, C>& x, std::nullptr_t) { return nullptr < x; }
	template <class T, class D, class C> bool operator > (std::nullptr_t, const value_ptr<T, D, C>& y) { return y < nullptr; }

	template <class T, class D, class C> bool operator >= (const value_ptr<T, D, C>& x, std::nullptr_t) { return !(x < nullptr); }
	template <class T, class D, class C> bool operator >= (std::nullptr_t, const value_ptr<T, D, C>& y) { return !(nullptr < y); }

	// make value_ptr with default deleter and copier, analogous to std::make_unique
	template<typename T, typename... Args>
	value_ptr<T> make_value(Args&&... args) {
		return value_ptr<T>(new T(std::forward<Args>(args)...));
	}

	// make a value_ptr from pointer with custom deleter and copier
	template <typename T, typename Deleter = std::default_delete<T>, typename Copier = detail::default_copy<T>>
	static inline auto make_value_ptr(T* ptr, Deleter&& dx = {}, Copier&& cx = {}) -> value_ptr<T, Deleter, Copier> {
		return value_ptr<T, Deleter, Copier>( ptr, std::forward<Deleter>( dx ), std::forward<Copier>(cx) );
	}	// make_value_ptr

}	// smart_ptr ns

#undef VALUE_PTR_CONSTEXPR
#undef VALUE_PTR_USE_EMPTY_BASE_OPTIMIZATION

#endif // !SMART_PTR_VALUE_PTR
