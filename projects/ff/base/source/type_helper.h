#pragma once

namespace ff::internal
{
    class type_helper
    {
    protected:
        template<class T, class... Args>
        static typename std::enable_if_t<!std::is_trivially_constructible<T, Args...>::value> construct_item(T* data, Args&&... args)
        {
            ::new(data) T(std::forward<Args>(args)...);
        }

        template<class T>
        static typename std::enable_if_t<std::is_trivially_constructible<T>::value> construct_item(T* data)
        {
        }

        template<class T>
        static typename std::enable_if_t<!std::is_trivially_default_constructible<T>::value> default_construct_items(T* data, size_t count)
        {
            for (T* endData = data + count; data != endData; data++)
            {
                ff::internal::type_helper::construct_item(data);
            }
        }

        template<class T>
        static typename std::enable_if_t<std::is_trivially_default_constructible<T>::value> default_construct_items(T* data, size_t count)
        {
        }

        template<class T>
        static typename std::enable_if_t<!std::is_trivially_copy_constructible<T>::value> copy_construct_item(T* data, const T& source)
        {
            ff::internal::type_helper::construct_item(data, source);
        }

        template<class T>
        static typename std::enable_if_t<std::is_trivially_copy_constructible<T>::value> copy_construct_item(T* data, const T& source)
        {
            std::memcpy(data, &source, sizeof(T));
        }

        template<class T>
        static typename std::enable_if_t<!std::is_trivially_move_constructible<T>::value> move_construct_item(T* data, T&& source)
        {
            ff::internal::type_helper::construct_item(data, std::move(source));
        }

        template<class T>
        static typename std::enable_if_t<std::is_trivially_move_constructible<T>::value> move_construct_item(T* data, T&& source)
        {
            std::memcpy(data, &source, sizeof(T));
        }

        template<class T>
        static typename std::enable_if_t<!std::is_trivially_move_constructible<T>::value> shift_items(T* destData, T* sourceData, size_t count)
        {
            if (destData < sourceData)
            {
                for (T* endDestData = destData + count; destData != endDestData; destData++, sourceData++)
                {
                    ff::internal::type_helper::move_construct_item(destData, std::move(*sourceData));
                    ff::internal::type_helper::destruct_item(sourceData);
                }
            }
            else if (destData > sourceData)
            {
                for (T* endDestData = destData, *curDestData = destData + count, *curSourceData = sourceData + count;
                    curDestData != endDestData; curDestData--, curSourceData--)
                {
                    ff::internal::type_helper::move_construct_item(curDestData - 1, std::move(curSourceData[-1]));
                    ff::internal::type_helper::destruct_item(curSourceData - 1);
                }
            }
        }

        template<class T>
        static typename std::enable_if_t<std::is_trivially_move_constructible<T>::value> shift_items(T* destData, T* sourceData, size_t count)
        {
            std::memmove(destData, sourceData, count * sizeof(T));
        }

        template<class T>
        static typename std::enable_if_t<!std::is_trivially_destructible<T>::value> destruct_item(T* data)
        {
            data->~T();
        }

        template<class T>
        static typename std::enable_if_t<std::is_trivially_destructible<T>::value> destruct_item(T* data)
        {
        }

        template<class T>
        static typename std::enable_if_t<!std::is_trivially_destructible<T>::value> destruct_items(T* data, size_t count)
        {
            for (T* endData = data + count; data != endData; data++)
            {
                ff::internal::type_helper::destruct_item(data);
            }
        }

        template<class T>
        static typename std::enable_if_t<std::is_trivially_destructible<T>::value> destruct_items(T* data, size_t count)
        {
        }
    };

    template<class T>
    constexpr bool is_iterator_t = std::_Is_iterator_v<T>;
}
