/**
 * @file
 * @author AKIYAMA Kouhei
 * @since 2011-08-05
 */
#ifndef AUSH_HEAPARRAY_H_INCLUDED
#define AUSH_HEAPARRAY_H_INCLUDED

namespace aush{
    template<typename T>
    class HeapArray
    {
        T *elems;
    public:
        explicit HeapArray(std::size_t size)
                : elems(new T[size])
        {}
        ~HeapArray(){
            delete elems;
        }
        T *get() { return elems;}
        const T *get() const { return elems;}
        T &operator[](std::size_t index) { return elems[index];}
        const T &operator[](std::size_t index) const { return elems[index];}
    private:
        HeapArray(const HeapArray &); //=delete
        HeapArray &operator=(const HeapArray &); //=delete
    };

}//namespace aush
#endif
